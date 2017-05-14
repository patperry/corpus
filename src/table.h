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

#ifndef CORPUS_TABLE_H
#define CORPUS_TABLE_H

/**
 * \file table.h
 *
 * Hash table, providing O(1) element access and insertion.
 */

/** Code for empty table cells. */
#define CORPUS_TABLE_ITEM_EMPTY (-1)

/**
 * Hash table buckets.
 */
struct corpus_table {
	int *items;	/**< indices of the items in the table */
	int capacity;	/**< maximum capacity of the table */
	unsigned mask;	/**< bitwise mask for indexing into the `items` array */
};

/**
 * Hash table probe, for looking up items by their hash value.
 */
struct corpus_table_probe {
	const struct corpus_table *table; /**< the underlying table */
	unsigned hash;		/**< starting hash value */
	unsigned nprobe;	/**< number of previous probes */
	int index;		/**< current index in the probe sequence */
	int current;		/**< current item in the probe sequence */
};

/**
 * Initialize a new hash table.
 *
 * \param tab the table
 *
 * \returns 0 on success
 */
int corpus_table_init(struct corpus_table *tab);

/**
 * Replace a hash table with a new empty table with a given minimum capacity.
 *
 * \param tab the previously-initialized table
 * \param min_capacity the minimum capacity for the new table
 *
 * \returns 0 on success; nonzero on failure. On failure, the `tab` object
 * is left unchanged.
 */
int corpus_table_reinit(struct corpus_table *tab, int min_capacity);

/**
 * Release a hash table's resources.
 *
 * \param tab the table
 */
void corpus_table_destroy(struct corpus_table *tab);

/**
 * Set all hash table items to #CORPUS_TABLE_ITEM_EMPTY.
 */
void corpus_table_clear(struct corpus_table *tab);

/**
 * Associate an item with the given hash code.
 *
 * \param tab a table with at least one empty cell
 * \param hash the hash code
 * \param item a non-negative item
 */
void corpus_table_add(struct corpus_table *tab, unsigned hash, int item);

/**
 * Start a new hash table probe at the given hash code.
 *
 * \param probe the new probe
 * \param tab the hash table
 * \param hash the hash code
 */
static inline void corpus_table_probe_make(struct corpus_table_probe *probe,
					   const struct corpus_table *tab,
					   unsigned hash)
{
	probe->table = tab;
	probe->hash = hash;
	probe->nprobe = 0;
	probe->index = 0;
	probe->current = CORPUS_TABLE_ITEM_EMPTY;
}

/**
 * Advance a probe to the next index in the sequence. Calling this function
 * repeatedly will loop over all indices in the hash table.
 *
 * \param probe the probe
 *
 * \returns zero if at the end of the sequence and `probe->current` is invalid;
 * 	nonzero if not at the end of the sequence
 */
static inline int corpus_table_probe_advance(struct corpus_table_probe *probe)
{
	unsigned index;

	if (probe->nprobe == 0) {
		index = probe->hash;
	} else {
		/* Linear probing:
		 *
		 *     h(k,i) = h(k) + i
		 *
		 * index += 1;
		 *
		 * Quadratic probing:
		 *
		 *     h(k,i) = h(k) + 0.5 i + 0.5 i^2
		 *
		 * index += nprobe;
		 *
		 * When the table size m is a power of 2, the values h(k,i) % m
		 * are all distinct for i = 0, 1, ..., m - 1
		 *
		 *     https://en.wikipedia.org/wiki/Quadratic_probing
		 */
		index = (unsigned)probe->index + probe->nprobe;
	}

	index &= probe->table->mask;
	probe->current = probe->table->items[index];
	probe->index = (int)index;
	probe->nprobe++;

	return (probe->current != CORPUS_TABLE_ITEM_EMPTY);
}

#endif /* CORPUS_TABLE_H */
