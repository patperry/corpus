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
#include <stdint.h>
#include "error.h"
#include "memory.h"
#include "table.h"

/*
 * The hash corpus_table implementation is based on
 *
 *       src/sparsehash/internal/densehashcorpus_table.h
 *
 * from the Google SparseHash library (BSD 3-Clause License):
 *
 *       https://github.com/sparsehash/sparsehash
 */


/* Maximum occupy percentage before we resize. Must be in (0, 1]. */
#define CORPUS_TABLE_LOAD_FACTOR	0.75

/* Minimum size for hash corpus_tables. Must be a power of 2, and at least 1,
 * so that we can replace modulo operation 'i % m' with 'i & (m - 1)',
 * where 'm' is the corpus_table size.
 */
#define CORPUS_TABLE_SIZE_MIN	1

/* Default initial size for hash corpus_tables. Must be a power of 2, and
 * at least CORPUS_TABLE_SIZE_MIN.
 */
//#define CORPUS_TABLE_SIZE_INIT	32
#define CORPUS_TABLE_SIZE_INIT	1

/* The number of buckets must be a power of 2, and below (INT_MAX + 1) */
#define CORPUS_TABLE_SIZE_MAX ((unsigned)INT_MAX + 1)

/* Maximum number of occupied buckets. */
#define CORPUS_TABLE_COUNT_MAX \
	(int)(CORPUS_TABLE_LOAD_FACTOR * CORPUS_TABLE_SIZE_MAX)

static int corpus_table_next_empty(const struct corpus_table *tab,
				   unsigned hash);

// The smallest size a hash corpus_table can be while holding 'count' elements
static unsigned corpus_table_size_min(int count, unsigned size_min)
{
	unsigned n;

	assert(count <= CORPUS_TABLE_COUNT_MAX);
	assert(size_min <= CORPUS_TABLE_SIZE_MAX);
	assert(CORPUS_TABLE_SIZE_MIN > 0);

	n = CORPUS_TABLE_SIZE_MIN;

	while (n < size_min
		|| ((unsigned)count
			> (unsigned)(CORPUS_TABLE_LOAD_FACTOR * n))) {
		n *= 2;
	}

	return n;
}


int corpus_table_init(struct corpus_table *tab)
{
	unsigned size = CORPUS_TABLE_SIZE_INIT;
	int err;

	assert(size >= CORPUS_TABLE_SIZE_MIN);
	assert(size <= CORPUS_TABLE_SIZE_MAX);

	if (!(tab->items = corpus_malloc(size * sizeof(*tab->items)))) {
		goto error_nomem;
	}

	tab->capacity = (int)(CORPUS_TABLE_LOAD_FACTOR * size);
	tab->mask = size - 1;
	corpus_table_clear(tab);

	return 0;

error_nomem:
	err = CORPUS_ERROR_NOMEM;
	corpus_log(err, "failed allocating table");
	return err;
}


int corpus_table_reinit(struct corpus_table *tab, int min_capacity)
{
	int *items = tab->items;
	unsigned size = tab->mask + 1;
	int capacity = tab->capacity;
	int err;

	if (min_capacity > capacity) {
		size = corpus_table_size_min(min_capacity, size);
		capacity = (int)(CORPUS_TABLE_LOAD_FACTOR * size);

#if SIZE_MAX <= UINT_MAX
		// overflow possible if sizeof(size_t) <= sizeof(unsigned)
		if ((size_t)size > SIZE_MAX / sizeof(*items)) {
			err = CORPUS_ERROR_OVERFLOW;
			corpus_log(err, "table size (%d)"
				   " exceeds maximum (%"PRIu64")",
				   size,
				   (uint64_t)(SIZE_MAX / sizeof(*items)));

			return err;
		}
#endif

		items = corpus_realloc(items, size * sizeof(*items));
		if (!items) {
			err = CORPUS_ERROR_NOMEM;
			corpus_log(err, "failed allocating table");
			return err;
		}

		tab->items = items;
		tab->capacity = capacity;
		tab->mask = size - 1;
	}

	corpus_table_clear(tab);

	return 0;
}


void corpus_table_destroy(struct corpus_table *tab)
{
	corpus_free(tab->items);
}


void corpus_table_clear(struct corpus_table *tab)
{
	unsigned i, n = tab->mask + 1;

	for (i = 0; i < n; i++) {
		tab->items[i] = CORPUS_TABLE_ITEM_EMPTY;
	}
}


void corpus_table_add(struct corpus_table *tab, unsigned hash, int item)
{
	int index;

	index = corpus_table_next_empty(tab, hash);
	tab->items[index] = item;
}


int corpus_table_next_empty(const struct corpus_table *tab, unsigned hash)
{
	struct corpus_table_probe probe;

	corpus_table_probe_make(&probe, tab, hash);
	while (corpus_table_probe_advance(&probe)) {
		// skip over occupied corpus_table cells
	}

	return probe.index;
}
