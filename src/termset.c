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
#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include "array.h"
#include "error.h"
#include "memory.h"
#include "table.h"
#include "termset.h"

static void corpus_termset_rehash(struct corpus_termset *set);
static int corpus_termset_grow_items(struct corpus_termset *set, int nadd);
static int corpus_termset_grow_buffer(struct corpus_termset *set, size_t nadd);

static int term_equals(const int *type_ids1, const int *type_ids2, int length);
static unsigned term_hash(const int *type_ids, int length);
static unsigned hash_combine(unsigned seed, unsigned hash);


int corpus_termset_init(struct corpus_termset *set)
{
	int err;

	if ((err = corpus_table_init(&set->table))) {
		corpus_log(err, "failed allocating item table");
		goto error_table;
	}

	set->items = NULL;
	set->nitem = 0;
	set->nitem_max = 0;

	set->buffer = NULL;
	set->nbuf = 0;
	set->nbuf_max = 0;

	return 0;

error_table:
	corpus_log(err, "failed initializing term set");
	return err;
}


void corpus_termset_destroy(struct corpus_termset *set)
{
	corpus_free(set->items);
	corpus_free(set->buffer);
	corpus_table_destroy(&set->table);
}


void corpus_termset_clear(struct corpus_termset *set)
{
	corpus_table_clear(&set->table);
	set->nitem = 0;
	set->nbuf = 0;
}


int corpus_termset_add(struct corpus_termset *set, const int *type_ids,
		       int length, int *idptr)
{
	int pos, id;
	int rehash = 0;
	int err;

	assert(length >= 0);

	if (corpus_termset_has(set, type_ids, length, &pos)) {
		id = pos;
	} else {
		id = set->nitem;

		// grow the items array if necessary
		if (id == set->nitem_max) {
			if ((err = corpus_termset_grow_items(set, 1))) {
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

		// grow the buffer if necessary
		if ((size_t)length > SIZE_MAX - set->nbuf_max) {
			err = CORPUS_ERROR_OVERFLOW;
			corpus_log(err, "term set data size exceeds maximum"
				   " (%"PRIu64" bytes)", (uint64_t)SIZE_MAX);
			goto error;
		}
		if (set->nbuf + (size_t)length > set->nbuf_max) {
			if ((err = corpus_termset_grow_buffer(set,
							(size_t)length))) {
				goto error;
			}
		}

		// initialize the new item
		memcpy(set->buffer + set->nbuf, type_ids,
		       length * sizeof(*type_ids));
		set->items[id].type_ids = set->buffer + set->nbuf;
		set->items[id].length = length;
		set->nbuf += (size_t)length;

		// update the count
		set->nitem++;

		// set the bucket
		if (rehash) {
			corpus_termset_rehash(set);
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
		corpus_termset_rehash(set);
	}
	corpus_log(err, "failed adding item to term set");
	return err;
}


int corpus_termset_has(const struct corpus_termset *set, const int *type_ids,
		       int length, int *idptr)
{
	struct corpus_table_probe probe;
	unsigned hash = term_hash(type_ids, length);
	int id = -1;
	int found = 0;

	corpus_table_probe_make(&probe, &set->table, hash);
	while (corpus_table_probe_advance(&probe)) {
		id = probe.current;
		if (set->items[id].length == length
		    && term_equals(type_ids, set->items[id].type_ids, length)) {
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


void corpus_termset_rehash(struct corpus_termset *set)
{
	const struct corpus_termset_term *items = set->items;
	const int *type_ids;
	struct corpus_table *table = &set->table;
	int length, i, n = set->nitem;
	unsigned hash;

	corpus_table_clear(table);

	for (i = 0; i < n; i++) {
		type_ids = items[i].type_ids;
		length = items[i].length;
		hash = term_hash(type_ids, length);
		corpus_table_add(table, hash, i);
	}
}


int corpus_termset_grow_items(struct corpus_termset *set, int nadd)
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


int corpus_termset_grow_buffer(struct corpus_termset *set, size_t nadd)
{
	int *buffer0 = set->buffer;
	void *base = buffer0;
	const int *type_ids;
	size_t size = set->nbuf_max;
	int err, i, n;

	if ((err = corpus_bigarray_grow(&base, &size, sizeof(*set->buffer),
					set->nbuf, nadd))) {
		corpus_log(err, "failed allocating term data buffer");
		return err;
	}
	set->buffer = base;
	set->nbuf_max = size;

	// update the items
	if (set->buffer != buffer0) {
		n = set->nitem;
		for (i = 0; i < n; i++) {
			type_ids = set->items[i].type_ids;
			set->items[i].type_ids = (set->buffer
						  + (type_ids - buffer0));
		}
	}

	return 0;
}


int term_equals(const int *type_ids1, const int *type_ids2, int length)
{
	if (memcmp(type_ids1, type_ids2, length * sizeof(*type_ids1)) == 0) {
		return 1;
	} else {
		return 0;
	}
}


unsigned term_hash(const int *type_ids, int length)
{
	int i;
	unsigned hash = 0;

	for (i = 0; i < length; i++) {
		hash = hash_combine(hash, type_ids[i]);
	}

	return hash;
}

/* adapted from boost/functional/hash/hash.hpp (Boost License, Version 1.0) */
unsigned hash_combine(unsigned seed, unsigned hash)
{
	seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	return seed;
}
