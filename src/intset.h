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

#ifndef CORPUS_INTSET_H
#define CORPUS_INTSET_H

/**
 * \file intset.h
 *
 * Integer set.
 */

/**
 * Integer set.
 */
struct corpus_intset {
	struct corpus_table table;	/**< hash table for items */
	int *items;			/**< items */
	int nitem;			/**< intset size */
	int nitem_max;			/**< intset capacity */
};

/**
 * Initialize a new integer set.
 *
 * \param set the set
 *
 * \returns 0 on success
 */
int corpus_intset_init(struct corpus_intset *set);

/**
 * Release an integer set's resources.
 *
 * \param set the set
 */
void corpus_intset_destroy(struct corpus_intset *set);

/**
 * Remove all items from an integer set.
 *
 * \param set the set
 */
void corpus_intset_clear(struct corpus_intset *set);

/**
 * Increment an item to an integer set.
 *
 * \param set the intset
 * \param item the item to add
 * \param idptr if non-NULL, a location to store the added item's ID
 *
 * \returns 0 on success
 */
int corpus_intset_add(struct corpus_intset *set, int item, int *idptr);

/**
 * Query whether an integer set has a specific item.
 *
 * \param set the set
 * \param item the item
 * \param idptr if non-NULL, a pointer to store the item ID, if
 * 	the item exists
 *
 * \returns nonzero if the item exists, zero otherwise
 */
int corpus_intset_has(const struct corpus_intset *set, int item,
		      int *idptr);

/**
 * Sort the set items into ascending order, optionally applying the same
 * operations to another array.
 *
 * \param set the set
 * \param base a pointer the base of the array to sort, or NULL
 * \param width the array item size
 *
 * \returns 0 on success
 */
int corpus_intset_sort(struct corpus_intset *set, void *base, size_t width);

#endif /* CORPUS_INTSET_H */
