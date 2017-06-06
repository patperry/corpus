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

#ifndef CORPUS_TERMSET_H
#define CORPUS_TERMSET_H

/**
 * \file termset.h
 *
 * Term set, assigning integer IDs to terms (type ID arrays).
 */

#include <stddef.h>

/**
 * A member of a term set.
 */
struct corpus_termset_term {
	const int *type_ids;	/**< the term's type ID array */
	int length;		/**< the term's length */
};

/**
 * Term set.
 */
struct corpus_termset {
	struct corpus_tree prefix;	/**< prefix tree */
	int *term_ids;			/**< term IDs for tree nodes */
	struct corpus_termset_term *items;/**< items */
	int nitem;			/**< number of items in the set */
	int nitem_max;			/**< current set capacity */
	int *buffer;			/**< term type ID data buffer */
	size_t nbuf;			/**< buffer size */
	size_t nbuf_max;		/**< buffer capacity */
	int error;			/**< code for last failing operation */
};

/**
 * Initialize an empty term set.
 *
 * \param set the term set.
 *
 * \returns 0 on success.
 */
int corpus_termset_init(struct corpus_termset *set);

/**
 * Release a term set's resources.
 *
 * \param set the term set.
 */
void corpus_termset_destroy(struct corpus_termset *set);

/**
 * Remove all items from a term set.
 *
 * \param set the term set.
 */
void corpus_termset_clear(struct corpus_termset *set);

/**
 * Add an item to a term set if it does not already belong.
 *
 * \param set the term set
 * \param type_ids the term's type ID array
 * \param length the term's length
 * \param idptr if non-NULL, a location to store the item ID.
 *
 * \returns 0 on success.
 */
int corpus_termset_add(struct corpus_termset *set, const int *type_ids,
		       int length, int *idptr);

/**
 * Check whether an item exists in a term set.
 *
 * \param set the term set
 * \param type_ids the type ID array to query
 * \param length the term length
 * \param idptr if non-NULL, a location to store the item ID if
 * 	it exists in the set.
 *
 * \returns non-zero if the item exists in the set, zero otherwise.
 */
int corpus_termset_has(const struct corpus_termset *set, const int *type_ids,
		       int length, int *idptr);

#endif /* CORPUS_TERMSET_H */
