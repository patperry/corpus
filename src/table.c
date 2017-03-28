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

#include <assert.h>
#include <limits.h>
#include <inttypes.h>
#include <stdlib.h>
#include <syslog.h>
#include "errcode.h"
#include "xalloc.h"
#include "table.h"


#define MIN(x, y) ((x) > (y) ? (x) : (y))


/* Maximum occupy percentage before we resize. Must be in (0, 1]. */
#define LOAD_FACTOR	0.75

/* Minimum size for hash tables. Must be a power of 2, and at least 1,
 * so that we can replace modulo operation 'i % m' with 'i & (m - 1)',
 * where 'm' is the table size.
 */
#define TABLE_SIZE_MIN	1

/* Default initial size for hash tables. Must be a power of 2, and
 * at least TABLE_SIZE_MIN.
 */
//#define TABLE_SIZE_INIT	32
#define TABLE_SIZE_INIT	1

/* The number of buckets must be a power of 2, and below (INT_MAX + 1) */
#define TABLE_SIZE_MAX ((unsigned)INT_MAX + 1)

/* Maximum number of occupied buckets. */
#define TABLE_COUNT_MAX	(int)(LOAD_FACTOR * TABLE_SIZE_MAX)


// The smallest size a hash table can be while holding 'count' elements
static unsigned table_size_min(int count, unsigned size_min)
{
	unsigned n;

	assert(count <= TABLE_COUNT_MAX);
	assert(size_min <= TABLE_SIZE_MAX);
	assert(TABLE_SIZE_MIN > 0);

	n = TABLE_SIZE_MIN;

	while (n < size_min || (unsigned)count > (unsigned)(LOAD_FACTOR * n)) {
		n *= 2;
	}

	return n;
}


int table_init(struct table *tab)
{
	unsigned size = TABLE_SIZE_INIT;

	assert(size >= TABLE_SIZE_MIN);
	assert(size <= TABLE_SIZE_MAX);

	if (!(tab->items = xmalloc(size * sizeof(*tab->items)))) {
		goto error_nomem;
	}

	tab->max_count = (int)(LOAD_FACTOR * size);
	tab->mask = size - 1;
	table_clear(tab);

	return 0;

error_nomem:
	syslog(LOG_ERR, "failed allocating table");
	return ERROR_NOMEM;
}


void table_destroy(struct table *tab)
{
	free(tab->items);
}


void table_clear(struct table *tab)
{
	unsigned i, n = tab->mask + 1;

	for (i = 0; i < n; i++) {
		tab->items[i] = TABLE_ITEM_EMPTY;
	}
}


int table_grow(struct table *tab, int count, int nadd)
{
	int *items = tab->items;
	int max_count = tab->max_count;
	unsigned size = tab->mask + 1;

	if (count > INT_MAX - nadd) {
		syslog(LOG_ERR, "table item count exceeds maximum (%d)",
		       TABLE_COUNT_MAX);
		return ERROR_OVERFLOW;
	}
	count = count + nadd;

	if (count <= max_count) {
		return 0;
	}

	if (count > TABLE_COUNT_MAX) {
		syslog(LOG_ERR, "table item count (%d) exceeds maximum (%d)",
		       count, TABLE_COUNT_MAX);
		return ERROR_OVERFLOW;
	}

	size = table_size_min(count, size);
	max_count = (int)(LOAD_FACTOR * size);

	if ((size_t)size > SIZE_MAX / sizeof(*items)) {
		syslog(LOG_ERR, "table size (%d) exceeds maximum (%zu)",
		       size, (size_t)(SIZE_MAX / sizeof(*items)));
		return ERROR_OVERFLOW;
	}

	if (!(items = xrealloc(items, size * sizeof(*items)))) {
		syslog(LOG_ERR, "failed allocating table");
		return ERROR_NOMEM;
	}

	tab->items = items;
	tab->max_count = max_count;
	tab->mask = size - 1;

	return 0;
}


int table_next_empty(const struct table *tab, unsigned hash)
{
	struct table_probe probe;
	const int *items = tab->items;

	table_probe_make(&probe, tab, hash);
	while (table_probe_advance(&probe)) {
		if (items[probe.current] == TABLE_ITEM_EMPTY) {
			break;
		}
	}

	return probe.current;
}
