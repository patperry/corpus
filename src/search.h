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

#ifndef CORPUS_SEARCH_H
#define CORPUS_SEARCH_H

/**
 * \file search.h
 *
 * Searching for terms in text.
 */

/**
 * Internal search buffer.
 */
struct corpus_search_buffer {
	struct utf8lite_text *tokens;	/**< tokens */
	int *type_ids;			/**< type IDs */
	int size;			/**< size */
	int size_max;			/**< capacity */
};

/**
 * Term search.
 */
struct corpus_search {
	struct corpus_filter *filter;	/**< text filter, defining tokens */
	struct corpus_search_buffer buffer;	/**< internal buffer */
	struct corpus_termset terms;	/**< search query term set */
	int length_max;			/**< maximum term length */

	struct utf8lite_text current;	/**< current result instance token */
	int term_id;			/**< current result instance ID */
	int length;			/**< current result term length */
	int error;			/**< last non-zero error code */
};

/**
 * Initialize a new search with an empty query set.
 *
 * \param search the search
 *
 * \returns 0 on success
 */
int corpus_search_init(struct corpus_search *search);

/**
 * Release a search's resources.
 *
 * \param search the search
 */
void corpus_search_destroy(struct corpus_search *search);

/**
 * Add a term to the search query set.
 *
 * \param search the search
 * \param type_ids the term type IDs
 * \param length the term length
 * \param idptr if non-NULL, a location to store the term ID
 *
 * \returns 0 on success.
 */
int corpus_search_add(struct corpus_search *search,
		      const int *type_ids, int length, int *idptr);

/**
 * Query whether a term exists in the search query set.
 *
 * \param search the search
 * \param type_ids the term type IDs
 * \param length the term length
 * \param idptr if non-NULL, a location to store the term ID, if
 * 	it exists in the set
 *
 * \returns non-zero if the term exists in the query set, zero otherwise.
 */
int corpus_search_has(const struct corpus_search *search,
		      const int *type_ids, int length, int *idptr);

/**
 * Start a search for the query set terms.
 *
 * \param search the search
 * \param text the text to search over
 * \param filter the tokenization filter
 *
 * \returns 0 on success
 */
int corpus_search_start(struct corpus_search *search,
			const struct utf8lite_text *text,
			struct corpus_filter *filter);

/**
 * Advance a search to the next term result.
 *
 * \param search the search.
 *
 * \returns non-zero if a next term exists, zero if at the end of the text
 * 	or an error occurs (in which case `search->error` gets set).
 */
int corpus_search_advance(struct corpus_search *search);

#endif /* CORPUS_SEARCH_H */
