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
#include "array.h"
#include "error.h"
#include "memory.h"
#include "table.h"
#include "census.h"


static int corpus_census_find(const struct corpus_census *c, int item,
			      int *indexptr);
static int corpus_census_grow(struct corpus_census *c, int nadd);
static void corpus_census_rehash(struct corpus_census *c);

static unsigned item_hash(int item);


int corpus_census_init(struct corpus_census *c)
{
	int err;

	if ((err = corpus_table_init(&c->table))) {
		corpus_log(err, "failed initializing census");
		goto out;
	}

	c->items = NULL;
	c->weights = NULL;
	c->nitem = 0;
	c->nitem_max = 0;
	err = 0;
out:
	return err;
}


void corpus_census_destroy(struct corpus_census *c)
{
	corpus_free(c->weights);
	corpus_free(c->items);
	corpus_table_destroy(&c->table);
}


void corpus_census_clear(struct corpus_census *c)
{
	corpus_table_clear(&c->table);
	c->nitem = 0;
}


int corpus_census_add(struct corpus_census *c, int item, double weight)
{
	int err, pos, i, rehash;

	rehash = 0;

	if (isnan(weight)) {
		err = CORPUS_ERROR_INVAL;
		corpus_log(err, "invalid weight for census item %d (NaN)",
			   item);
		goto error;
	}

	if (corpus_census_find(c, item, &i)) {
		c->weights[i] += weight;
		err = 0;
		goto out;
	}
	pos = i;	// table position
	i = c->nitem;	// new index

	// grow the arrays if necessary
	if (c->nitem == c->nitem_max) {
		if ((err = corpus_census_grow(c, 1))) {
			goto error;
		}
	}

	// grow the table if necessary
	if (c->nitem == c->table.capacity) {
		if ((err = corpus_table_reinit(&c->table, c->nitem + 1))) {
			goto error;
		}
		rehash = 1;
	}

	// add the new item
	c->weights[i] = weight;
	c->items[i] = item;
	c->nitem++;

	if (rehash) {
		corpus_census_rehash(c);
	} else {
		c->table.items[pos] = i;
	}

	err = 0;
	goto out;

error:
	corpus_log(err, "failed adding item to census");
	if (rehash) {
		corpus_census_rehash(c);
	}
out:
	return err;
}


int corpus_census_has(const struct corpus_census *c, int item,
		      double *weightptr)
{
	double weight;
	int i, found;

	if (corpus_census_find(c, item, &i)) {
		found = 1;
		weight = c->weights[i];
	} else {
		found = 0;
		weight = 0;
	}

	if (weightptr) {
		*weightptr = weight;
	}

	return found;
}


struct corpus_census_item {
	int item;
	double weight;
};


static int int_cmp(int x1, int x2)
{
	if (x1 < x2) {
		return -1;
	} else if (x1 > x2) {
		return +1;
	} else {
		return 0;
	}
}


static int double_cmp(double x1, double x2)
{
	if (x1 < x2) {
		return -1;
	} else if (x1 > x2) {
		return +1;
	} else {
		return 0;
	}
}


static int corpus_census_item_cmp(const void *x1, const void *x2)
{
	const struct corpus_census_item *y1 = x1;
	const struct corpus_census_item *y2 = x2;
	int cmp;

	cmp = -double_cmp(y1->weight, y2->weight); // weight descending
	if (cmp == 0) {
		cmp = int_cmp(y1->item, y2->item); // item ascending
	}

	return cmp;
}


int corpus_census_sort(struct corpus_census *c)
{
	struct corpus_census_item *array;
	int i, n = c->nitem;
	int err;

	if ((size_t)n > SIZE_MAX / sizeof(*array)) {
		err = CORPUS_ERROR_OVERFLOW;
		corpus_log(err, "census items array size (%d)"
			   " is too large to sort", n);
		goto out;
	}

	if (!(array = corpus_malloc((size_t)n * sizeof(*array)))) {
		err = CORPUS_ERROR_NOMEM;
		corpus_log(err,
			   "failed allocating memory to sort census items");
		goto out;
	}

	for (i = 0; i < n; i++) {
		array[i].item = c->items[i];
		array[i].weight = c->weights[i];
	}

	qsort(array, (size_t)n, sizeof(*array), corpus_census_item_cmp);

	for (i = 0; i < n; i++) {
		c->items[i] = array[i].item;
		c->weights[i] = array[i].weight;
	}

	corpus_census_rehash(c);

	corpus_free(array);
	err = 0;
out:
	return err;
}


int corpus_census_find(const struct corpus_census *c, int item, int *indexptr)
{
	struct corpus_table_probe probe;
	unsigned hash = item_hash(item);
	int index = -1;
	int found;

	corpus_table_probe_make(&probe, &c->table, hash);
	while (corpus_table_probe_advance(&probe)) {
		index = probe.current;
		if (c->items[index] == item) {
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


int corpus_census_grow(struct corpus_census *c, int nadd)
{
	void *ibase, *wbase;
	int err, size;

	if (nadd <= 0 || c->nitem <= c->nitem_max - nadd) {
		return 0;
	}

	wbase = c->weights;
	size = c->nitem_max;
	err = corpus_array_grow(&wbase, &size, sizeof(*c->weights), c->nitem,
			 nadd);
	if (err) {
		corpus_log(err, "failed growing census weight array");
		goto out;
	}
	c->weights = wbase;

	ibase = corpus_realloc(c->items, (size_t)size * sizeof(*c->items));
	if (!ibase) {
		err = CORPUS_ERROR_NOMEM;
		corpus_log(err, "failed growing census items array");
		goto out;
	}
	c->items = ibase;
	c->nitem_max = size;

	err = 0;
out:
	return err;
}


void corpus_census_rehash(struct corpus_census *c)
{
	int item, i, n = c->nitem;
	unsigned hash;

	corpus_table_clear(&c->table);
	for (i = 0; i < n; i++) {
		item = c->items[i];
		hash = item_hash(item);
		corpus_table_add(&c->table, hash, i);
	}
}


unsigned item_hash(int item)
{
	return (unsigned)item;
}
