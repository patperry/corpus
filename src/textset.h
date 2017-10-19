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

#ifndef CORPUS_TEXTSET_H
#define CORPUS_TEXTSET_H

/**
 * \file textset.h
 *
 * Text set, assigning integer IDs to text strings.
 */

struct utf8lite_text;

/**
 * Text set.
 */
struct corpus_textset {
	struct corpus_table table;	/**< item hash table */
	struct utf8lite_text *items;	/**< items */
	int nitem;			/**< number of items in the set */
	int nitem_max;			/**< current set capacity */
};

/**
 * Initialize an empty text set.
 *
 * \param set the text set.
 *
 * \returns 0 on success.
 */
int corpus_textset_init(struct corpus_textset *set);

/**
 * Release a text set's resources.
 *
 * \param set the text set.
 */
void corpus_textset_destroy(struct corpus_textset *set);

/**
 * Remove all items from a text set.
 *
 * \param set the text set.
 */
void corpus_textset_clear(struct corpus_textset *set);

/**
 * Add an item to a text set if it does not already belong.
 *
 * \param set the text set
 * \param text the item to add
 * \param idptr if non-NULL, a location to store the item ID.
 *
 * \returns 0 on success.
 */
int corpus_textset_add(struct corpus_textset *set,
		       const struct utf8lite_text *text, int *idptr);

/**
 * Check whether an item exists in a text set.
 *
 * \param set the text set
 * \param text the item to query
 * \param idptr if non-NULL, a location to store the item ID if
 * 	it exists in the set.
 *
 * \returns non-zero if the item exists in the set, zero otherwise.
 */
int corpus_textset_has(const struct corpus_textset *set,
		       const struct utf8lite_text *text, int *idptr);

#endif /* CORPUS_TEXTSET_H */
