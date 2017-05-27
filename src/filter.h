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

#ifndef CORPUS_FILTER_H
#define CORPUS_FILTER_H

/**
 * \file filter.h
 *
 * Text filter, for converting a text into a sequence of terms.
 */

/**
 * Filter type, for specifying which word classes to drop.
 */
enum corpus_filter_type {
	CORPUS_FILTER_NONE = 0,		/**< do not filter any word types */
	CORPUS_FILTER_IGNORE_EMPTY = (1 << 0),	/**< ignore empty types */
	CORPUS_FILTER_DROP_SYMBOL = (1 << 1),	/**< drop symbol words */
	CORPUS_FILTER_DROP_NUMBER = (1 << 2),	/**< drop number words */
	CORPUS_FILTER_DROP_LETTER = (1 << 3)	/**< drop letter words */
};

/**
 * Text filter.
 */
struct corpus_filter {
	struct corpus_symtab symtab;	/**< token/type symbol table */
	struct corpus_tree combine;	/**< word sequences to combine */
	int *combine_rules;		/**< properties for nodes in the
					  combine tree */
	int *term_ids;			/**< term IDs for the types */
	int *type_ids;			/**< type IDs for the terms */
	int nterm;			/**< number of terms */
	int nterm_max;			/**< maximum number of terms before
						requiring reallocation */
	struct corpus_wordscan scan;	/**< current word scan */
	int flags;			/**< filter flags */
	int has_select;			/**< whether the filter has a
					  selection set */
	int has_scan;			/**< whether a scan is in progress */
	int error;			/**< last error code */
};

/**
 * Initialize a text filter.
 *
 * \param f the filter
 * \param type_kind flags for the symbol table indicating the type kind
 * \param stemmer the stemming algorithm name
 * \param flags a big mask of #corpus_filter_type values indicating filter
 * 	options
 *
 * \returns 0 on success
 */
int corpus_filter_init(struct corpus_filter *f, int type_kind,
		       const char *stemmer, int flags);

/**
 * Release a text filter's resources.
 *
 * \param f the filter
 */
void corpus_filter_destroy(struct corpus_filter *f);

/**
 * Add a stemming exception to the filter's symbol table.
 *
 * \param f the filter
 * \param type the unstemmed type
 *
 * \returns 0 on success
 */
int corpus_filter_stem_except(struct corpus_filter *f,
			      const struct corpus_text *type);

/**
 * Add a combination rule to to filter.
 *
 * \param f the filter
 * \param term the term to build a combination rule from
 *
 * \returns 0 on success
 */
int corpus_filter_combine(struct corpus_filter *f,
			  const struct corpus_text *term);

/**
 * Add a term to the filter's drop list. Terms on this list get negative
 * IDs when they are encountered in a text.
 *
 * \param f the filter
 * \param term the term to drop
 *
 * \returns 0 on success
 */
int corpus_filter_drop(struct corpus_filter *f,
		       const struct corpus_text *term);

/**
 * Add a term to the filter's drop exception list. This overrides the
 * drop settings in the flag used to initialize the filter and any
 * previous calls to corpus_filter_drop() for that term
 *
 * \param f the filter
 * \param term the term to add to the drop exception list
 *
 * \returns 0 on success
 */
int corpus_filter_drop_except(struct corpus_filter *f,
			      const struct corpus_text *term);

/**
 * Add a term to the filter's selection set if it is not already a
 * member. If non-empty, terms outside the selection set get negative
 * IDs when they are encountered in a text.
 *
 * \param f the filter
 * \param term the term to add to the selection set
 * \param idptr if non-NULL, a location to store the term's id
 *
 * \returns 0 on success
 */
int corpus_filter_select(struct corpus_filter *f,
			 const struct corpus_text *term, int *idptr);

/**
 * Start scanning a text.
 *
 * \param f the filter
 * \param text the text
 *
 * \returns 0 on success
 */
int corpus_filter_start(struct corpus_filter *f,
			const struct corpus_text *text);

/**
 * Advance a text to the next term.
 *
 * \param f the filter
 * \param idptr if non-NULL, a location to store the next term id.
 *
 * \returns 1 on success, 0 if at the end of the text or an error occurs,
 * 	in which case the `f->error` will be set to the error code
 */
int corpus_filter_advance(struct corpus_filter *f, int *idptr);

/**
 * Get the text corresponding to a given term id.
 *
 * \param f the filter
 * \param id the term id
 *
 * \returns the text for that term
 */
const struct corpus_text *corpus_filter_term(const struct corpus_filter *f,
					     int id);

#endif /* CORPUS_FILTER_H */
