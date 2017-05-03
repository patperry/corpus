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

#ifndef CENSUS_H
#define CENSUS_H

/**
 * \file census.h
 *
 * Census, for tallying item occurrences
 */

/**
 * Census table.
 */
struct census {
	struct table table;	/**< hash table for items */
	int *items;		/**< item keys */
	double *weights;	/**< item weights */
	int nitem;		/**< census size */
	int nitem_max;		/**< census capacity */
};

/**
 * Initialize a new census.
 *
 * \param c the census
 *
 * \returns 0 on success
 */
int census_init(struct census *c);

/**
 * Release a census's resources.
 *
 * \param c the census
 */
void census_destroy(struct census *c);

/**
 * Remove all items from a census.
 *
 * \param c the census
 */
void census_clear(struct census *c);

/**
 * Increment an item weight by the given amount.
 *
 * \param c the census
 * \param item the item key
 * \param weight the increment amount (negative for decrement)
 *
 * \returns 0 on success
 */
int census_add(struct census *c, int item, double weight);

/**
 * Query whether a census has a specific item.
 *
 * \param c the census
 * \param item the item key
 * \param weightptr if non-NULL, a pointer to store the item weight, if
 * 	the item exists
 *
 * \returns nonzero if the item exists, zero otherwise
 */
int census_has(const struct census *c, int item, double *weightptr);

/**
 * Sort the census items by weight, in descending order. Break ties
 * by sorting according to item key in ascending order.
 *
 * \param c the census
 *
 * \returns 0 on success
 */
int census_sort(struct census *c);

#endif /* CENSUS_H */
