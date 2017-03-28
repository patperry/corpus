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

#ifndef TABLE_H
#define TABLE_H

/**
 * \file table.h
 *
 * Hash table, providing O(1) element access and insertion.
 */

#include <stddef.h>
#include <stdint.h>

/** Code for empty buckets. */
#define TABLE_ITEM_EMPTY (-1)

/** Linear probing:
 *
 *     h(k,i) = h(k) + i
 */
#define TABLE_PROBE_JUMP_LIN(nprobe)	(1)

/** Quadratic probing:
 *
 *     h(k,i) = h(k) + 0.5 i + 0.5 i^2
 *
 * When the table size "m" is a power of 2, the values h(k,i) % m
 * are all distinct for i = 0, 1, ..., m - 1
 *
 *     https://en.wikipedia.org/wiki/Quadratic_probing
 */
#define TABLE_PROBE_JUMP_QUAD(nprobe)	(nprobe)

/** Hash table probing method */
#define TABLE_PROBE_JUMP(nprobe) TABLE_PROBE_JUMP_QUAD(nprobe)

/**
 * Hash table buckets.
 */
struct table {
	int *items;	/**< indices of the items in the table */
	int max_count;	/**< maximum capacity of the table */
	unsigned mask;	/**< bitwise mask for indexing into the `items` array */
};

/**
 * Hash table probe, for looking up items by their hash value.
 */
struct table_probe {
	unsigned mask;   /**< hash table indexing mask */
	unsigned nprobe; /**< number of previous probes */
	unsigned hash;	 /**< starting hash value */
	int current;
};

/**
 * Start a new hash table probe at the given hash code.
 *
 * \param probe the new probe
 * \param tab the hash table
 * \param hash the hash code
 */
static inline void table_probe_make(struct table_probe *probe,
			            const struct table *tab, unsigned hash)
{
	probe->mask = tab->mask;
	probe->nprobe = 0;
	probe->hash = hash;
	probe->current = -1;
}

/**
 * Advance a probe to the next index in the sequence. Calling this function
 * repeatedly will loop over all indices in the hash table.
 *
 * \param probe the probe
 *
 * \returns 1
 */
static inline int table_probe_advance(struct table_probe *probe)
{
	/* Use an unsigned integer so that overflow and bitwise operations
	 * are well-defined. */
	unsigned current = (unsigned)probe->current;

	if (probe->nprobe == 0) {
		current = probe->hash;
	} else {
		/* Linear probing:
		 *
		 *     h(k,i) = h(k) + i
		 *
		 * current += 1;
		 *
		 * Quadratic probing:
		 *
		 *     h(k,i) = h(k) + 0.5 i + 0.5 i^2
		 *
		 * current += probe->nprobe;
		 *
		 * When the table size m is a power of 2, the values h(k,i) % m
		 * are all distinct for i = 0, 1, ..., m - 1
		 *
		 *     https://en.wikipedia.org/wiki/Quadratic_probing
		 */
		current += probe->nprobe;
	}

	probe->current = (int)(current & probe->mask);
	probe->nprobe++;

	return 1;
}

/**
 * Initialize a new hash table.
 */
int table_init(struct table *tab);

/**
 * Release a hash table's resources.
 */
void table_destroy(struct table *tab);

/**
 * Set all hash table items to #TABLE_ITEM_EMPTY.
 */
void table_clear(struct table *tab);

int table_grow(struct table *tab, int count, int nadd);
int table_next_empty(const struct table *tab, unsigned hash);

#endif /* TABLE_H */
