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
#include <stdlib.h>
#include <stdint.h>
#include "array.h"
#include "error.h"
#include "table.h"
#include "xalloc.h"
#include "census.h"


static int census_find(const struct census *c, int item, int *indexptr);
static int census_grow(struct census *c, int nadd);
static void census_rehash(struct census *c);

static unsigned item_hash(int item);


int census_init(struct census *c)
{
	int err;

	if ((err = table_init(&c->table))) {
		logmsg(err, "failed initializing census");
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


void census_destroy(struct census *c)
{
	free(c->weights);
	free(c->items);
	table_destroy(&c->table);
}


void census_clear(struct census *c)
{
	table_clear(&c->table);
	c->nitem = 0;
}


int census_add(struct census *c, int item, double weight)
{
	int err, pos, i, rehash;

	rehash = 0;

	if (isnan(weight)) {
		err = ERROR_INVAL;
		logmsg(err, "invalid weight for census item %d (NaN)", item);
		goto error;
	}

	if (census_find(c, item, &i)) {
		c->weights[i] += weight;
		err = 0;
		goto out;
	}
	pos = i;	// table position
	i = c->nitem;	// new index

	// grow the arrays if necessary
	if (c->nitem == c->nitem_max) {
		if ((err = census_grow(c, 1))) {
			goto error;
		}
	}

	// grow the table if necessary
	if (c->nitem == c->table.capacity) {
		if ((err = table_reinit(&c->table, c->nitem + 1))) {
			goto error;
		}
		rehash = 1;
	}

	// add the new item
	c->weights[i] = weight;
	c->items[i] = item;
	c->nitem++;

	if (rehash) {
		census_rehash(c);
	} else {
		c->table.items[pos] = i;
	}

	err = 0;
	goto out;

error:
	logmsg(err, "failed adding item to census");
	if (rehash) {
		census_rehash(c);
	}
out:
	return err;
}


int census_has(const struct census *c, int item, double *weightptr)
{
	double weight;
	int i, found;

	if (census_find(c, item, &i)) {
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


struct census_item {
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


static int census_item_cmp(const void *x1, const void *x2)
{
	const struct census_item *y1 = x1;
	const struct census_item *y2 = x2;
	int cmp;

	cmp = -double_cmp(y1->weight, y2->weight); // weight descending
	if (cmp == 0) {
		cmp = int_cmp(y1->item, y2->item); // item ascending
	}

	return cmp;
}


int census_sort(struct census *c)
{
	struct census_item *array;
	int i, n = c->nitem;
	int err;

	if ((size_t)n > SIZE_MAX / sizeof(*array)) {
		err = ERROR_OVERFLOW;
		logmsg(err, "census items array size (%d)"
		       " is too large to sort", n);
		goto out;
	}

	if (!(array = xmalloc(n * sizeof(*array)))) {
		err = ERROR_NOMEM;
		logmsg(err, "failed allocating memory to sort census items");
		goto out;
	}

	for (i = 0; i < n; i++) {
		array[i].item = c->items[i];
		array[i].weight = c->weights[i];
	}

	qsort(array, n, sizeof(*array), census_item_cmp);

	for (i = 0; i < n; i++) {
		c->items[i] = array[i].item;
		c->weights[i] = array[i].weight;
	}

	census_rehash(c);

	free(array);
	err = 0;
out:
	return err;
}


int census_find(const struct census *c, int item, int *indexptr)
{
	struct table_probe probe;
	unsigned hash = item_hash(item);
	int index = -1;
	int found;

	table_probe_make(&probe, &c->table, hash);
	while (table_probe_advance(&probe)) {
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


int census_grow(struct census *c, int nadd)
{
	void *ibase, *wbase;
	int err, size;

	if (nadd <= 0 || c->nitem <= c->nitem_max - nadd) {
		return 0;
	}

	wbase = c->weights;
	size = c->nitem_max;
	err = array_grow(&wbase, &size, sizeof(*c->weights), c->nitem,
			 nadd);
	if (err) {
		logmsg(err, "failed growing census weight array");
		goto out;
	}
	c->weights = wbase;

	ibase = xrealloc(c->items, size * sizeof(*c->items));
	if (!ibase) {
		err = ERROR_NOMEM;
		logmsg(err, "failed growing census items array");
		goto out;
	}
	c->items = ibase;
	c->nitem_max = size;

	err = 0;
out:
	return err;
}


void census_rehash(struct census *c)
{
	int item, i, n = c->nitem;
	unsigned hash;

	table_clear(&c->table);
	for (i = 0; i < n; i++) {
		item = c->items[i];
		hash = item_hash(item);
		table_add(&c->table, hash, i);
	}
}


unsigned item_hash(int item)
{
	return (unsigned)item;
}
