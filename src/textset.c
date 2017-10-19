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

#include <stddef.h>
#include "../lib/utf8lite/src/utf8lite.h"
#include "array.h"
#include "error.h"
#include "memory.h"
#include "table.h"
#include "textset.h"

static void corpus_textset_rehash(struct corpus_textset *set);
static int corpus_textset_grow(struct corpus_textset *set, int nadd);


int corpus_textset_init(struct corpus_textset *set)
{
	int err;

	if ((err = corpus_table_init(&set->table))) {
		corpus_log(err, "failed allocating item table");
		goto error_table;
	}

	set->items = NULL;
	set->nitem_max = 0;
	set->nitem = 0;

	return 0;

error_table:
	corpus_log(err, "failed initializing text set");
	return err;
}


void corpus_textset_destroy(struct corpus_textset *set)
{
	corpus_textset_clear(set);
	corpus_free(set->items);
	corpus_table_destroy(&set->table);
}


void corpus_textset_clear(struct corpus_textset *set)
{
	int nitem = set->nitem;

	while (nitem-- > 0) {
		utf8lite_text_destroy(&set->items[nitem]);
	}
	set->nitem = 0;

	corpus_table_clear(&set->table);
}


int corpus_textset_add(struct corpus_textset *set,
		       const struct utf8lite_text *text, int *idptr)
{
	int pos, id;
	int rehash = 0;
	int err;

	if (corpus_textset_has(set, text, &pos)) {
		id = pos;
	} else {
		id = set->nitem;

		// grow the items array if necessary
		if (id == set->nitem_max) {
			if ((err = corpus_textset_grow(set, 1))) {
				goto error;
			}
		}

		// grow the item table if necessary
		if (id == set->table.capacity) {
			if ((err = corpus_table_reinit(&set->table, id + 1))) {
				goto error;
			}
			rehash = 1;
		}

		// initialize the new item
		if ((err = utf8lite_text_init_copy(&set->items[id], text))) {
			goto error;
		}

		// update the count
		set->nitem++;

		// set the bucket
		if (rehash) {
			corpus_textset_rehash(set);
		} else {
			set->table.items[pos] = id;
		}
	}

	if (idptr) {
		*idptr = id;
	}

	return 0;

error:
	if (rehash) {
		corpus_textset_rehash(set);
	}
	corpus_log(err, "failed adding item to text set");
	return err;
}


int corpus_textset_has(const struct corpus_textset *set,
		       const struct utf8lite_text *text, int *idptr)
{

	struct corpus_table_probe probe;
	unsigned hash = (unsigned)utf8lite_text_hash(text);
	int id = -1;
	int found = 0;

	corpus_table_probe_make(&probe, &set->table, hash);
	while (corpus_table_probe_advance(&probe)) {
		id = probe.current;
		if (utf8lite_text_equals(text, &set->items[id])) {
			found = 1;
			goto out;
		}
	}

out:
	if (idptr) {
		*idptr = found ? id : probe.index;
	}

	return found;
}


void corpus_textset_rehash(struct corpus_textset *set)
{
	const struct utf8lite_text *items = set->items;
	struct corpus_table *table = &set->table;
	int i, n = set->nitem;
	unsigned hash;

	corpus_table_clear(table);

	for (i = 0; i < n; i++) {
		hash = (unsigned)utf8lite_text_hash(&items[i]);
		corpus_table_add(table, hash, i);
	}
}


int corpus_textset_grow(struct corpus_textset *set, int nadd)
{
	void *base = set->items;
	int size = set->nitem_max;
	int err;

	if ((err = corpus_array_grow(&base, &size, sizeof(*set->items),
				     set->nitem, nadd))) {
		corpus_log(err, "failed allocating item array");
		return err;
	}

	set->items = base;
	set->nitem_max = size;
	return 0;
}
