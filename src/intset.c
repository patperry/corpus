/*
 * Copyright 2017 Patrick O. Perry.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "array.h"
#include "error.h"
#include "memory.h"
#include "table.h"
#include "intset.h"


static int corpus_intset_find(const struct corpus_intset *set, int item,
			      int *indexptr);
static int corpus_intset_grow(struct corpus_intset *set, int nadd);
static void corpus_intset_rehash(struct corpus_intset *set);

static unsigned item_hash(int item);


int corpus_intset_init(struct corpus_intset *set)
{
	int err;

	if ((err = corpus_table_init(&set->table))) {
		corpus_log(err, "failed initializing integer set");
		goto out;
	}

	set->items = NULL;
	set->nitem = 0;
	set->nitem_max = 0;
	err = 0;
out:
	return err;
}


void corpus_intset_destroy(struct corpus_intset *set)
{
	corpus_free(set->items);
	corpus_table_destroy(&set->table);
}


void corpus_intset_clear(struct corpus_intset *set)
{
	corpus_table_clear(&set->table);
	set->nitem = 0;
}


int corpus_intset_add(struct corpus_intset *set, int item, int *idptr)
{
	int err, pos, id, rehash;

	rehash = 0;

	if (corpus_intset_find(set, item, &id)) {
		err = 0;
		goto out;
	}
	pos = id; // table position
	id = set->nitem; // new index

	// grow the array if necessary
	if (set->nitem == set->nitem_max) {
		if ((err = corpus_intset_grow(set, 1))) {
			goto error;
		}
	}

	// grow the table if necessary
	if (set->nitem == set->table.capacity) {
		if ((err = corpus_table_reinit(&set->table, set->nitem + 1))) {
			goto error;
		}
		rehash = 1;
	}

	// add the new item
	set->items[id] = item;
	set->nitem++;

	if (rehash) {
		corpus_intset_rehash(set);
	} else {
		set->table.items[pos] = id;
	}

	err = 0;
	goto out;

error:
	corpus_log(err, "failed adding item to intset");
	if (rehash) {
		corpus_intset_rehash(set);
	}
	id = -1;
out:
	if (idptr) {
		*idptr = id;
	}
	return err;
}


int corpus_intset_has(const struct corpus_intset *set, int item, int *idptr)
{
	int id, found;

	if (corpus_intset_find(set, item, &id)) {
		found = 1;
	} else {
		found = 0;
		id = -1;
	}

	if (idptr) {
		*idptr = id;
	}

	return found;
}

static int int_cmp(const void *x1, const void *x2)
{
	int y1 = *(const int *)x1;
	int y2 = *(const int *)x2;

	if (y1 < y2) {
		return -1;
	} else if (y1 > y2) {
		return +1;
	} else {
		return 0;
	}
}


static int intptr_cmp(const void *x1, const void *x2)
{
	const int *y1 = *(const int **)x1;
	const int *y2 = *(const int **)x2;
	return int_cmp(y1, y2);
}


int corpus_intset_sort(struct corpus_intset *set, void *base, size_t width)
{
	size_t i, j, n = (size_t)set->nitem;
	int **ptrs;
	int *items;
	char *buf;
	int err;

	if (n == 0) {
		return 0;
	}

	if (!base || width == 0) {
		qsort(set->items, n, sizeof(*set->items), int_cmp);
		corpus_intset_rehash(set);
		return 0;
	}

	items = corpus_malloc(n * sizeof(*items));
	buf = corpus_malloc(n * width);
	ptrs = corpus_malloc(n * sizeof(*ptrs));

	if (!(buf && ptrs)) {
		err = CORPUS_ERROR_NOMEM;
		goto out;
	}

	// determine the order
	for (i = 0; i < n; i++) {
		ptrs[i] = &set->items[i];
	}
	qsort(ptrs, n, sizeof(*ptrs), intptr_cmp);

	// copy the old items and client array
	memcpy(items, set->items, n * sizeof(*items));
	memcpy(buf, base, n * width);

	// put the items in order
	for (i = 0; i < n; i++) {
		j = (size_t)(ptrs[i] - set->items);
		set->items[i] = items[j];
		memcpy((char *)base + i * width, buf + j * width, width);
	}

	err = 0;
out:
	corpus_free(ptrs);
	corpus_free(buf);
	corpus_free(items);
	if (err) {
		corpus_log(err, "failed sorting integer set");
	}

	return err;
}


int corpus_intset_find(const struct corpus_intset *set, int item, int *indexptr)
{
	struct corpus_table_probe probe;
	unsigned hash = item_hash(item);
	int index = -1;
	int found;

	corpus_table_probe_make(&probe, &set->table, hash);
	while (corpus_table_probe_advance(&probe)) {
		index = probe.current;
		if (set->items[index] == item) {
			found = 1;
			goto out;
		}
	}
	found = 0;
out:
	if (indexptr) {
		*indexptr = found ? index : probe.index;
	}
	return found;
}


int corpus_intset_grow(struct corpus_intset *set, int nadd)
{
	void *base = set->items;;
	int err, size = set->nitem_max;


	err = corpus_array_grow(&base, &size, sizeof(*set->items),
		 	        set->nitem, nadd);
	if (err) {
		corpus_log(err, "failed growing integer set items array");
		goto out;
	}
	set->items = base;
	set->nitem_max = size;

	err = 0;
out:
	return err;
}


void corpus_intset_rehash(struct corpus_intset *set)
{
	int item, i, n = set->nitem;
	unsigned hash;

	corpus_table_clear(&set->table);
	for (i = 0; i < n; i++) {
		item = set->items[i];
		hash = item_hash(item);
		corpus_table_add(&set->table, hash, i);
	}
}


unsigned item_hash(int item)
{
	return (unsigned)item;
}
