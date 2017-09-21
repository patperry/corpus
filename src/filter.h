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
 * Token filter, for converting a text into a sequence of tokens.
 */

/** ID code for a missing type or type that has been dropped. */
#define CORPUS_FILTER_NONE  (-1)

/**
 * Filter type, for specifying which word classes to drop.
 */
enum corpus_filter_type {
	CORPUS_FILTER_KEEP_ALL      = 0,	/**< keep all word types */
	CORPUS_FILTER_DROP_SPACE    = (1 << 0),	/**< drop white space */
	CORPUS_FILTER_DROP_LETTER   = (1 << 1),	/**< drop letter words */
	CORPUS_FILTER_DROP_NUMBER   = (1 << 2),	/**< drop number words */
	CORPUS_FILTER_DROP_PUNCT    = (1 << 3),	/**< drop punctuation words */
	CORPUS_FILTER_DROP_SYMBOL   = (1 << 4),	/**< drop symbol words */
	CORPUS_FILTER_DROP_OTHER    = (1 << 5)  /**< drop other words */
};

/**
 * Filter scanning options.
 */
enum corpus_filter_scan {
	CORPUS_FILTER_SCAN_TOKENS = 0,	/**< scan a token sequence */
	CORPUS_FILTER_SCAN_TYPES = 1	/**< scan a type sequence */
};

/**
 * Text filter.
 */
struct corpus_filter {
	struct corpus_symtab symtab;	/**< symbol table;
					  we refer to symbol table types as
					  "symbols" to distinguish them from
					  filter types */
	struct corpus_tree combine;	/**< word sequences to combine */
	int *combine_rules;		/**< properties for nodes in the
					  combine tree */
	int *type_ids;			/**< type IDs for the symbols */
	int *symbol_ids;		/**< symbol IDs for the types */
	int ntype;			/**< number of types */
	int ntype_max;			/**< maximum number of types before
						requiring reallocation */
	struct corpus_wordscan scan;	/**< current word scan */
	int flags;			/**< filter flags */
	int has_scan;			/**< whether a scan is in progress */
	int scan_type;			/**< #corpus_filter_scan value
					  indicating scan type */
	struct corpus_text current;	/**< current token */
	int type_id;			/**< current type ID */
	int error;			/**< last error code */
};

/**
 * Initialize a text filter.
 *
 * \param f the filter
 * \param symbol_kind flags for the symbol table indicating the symbol type kind
 * \param stemmer the stemming algorithm name
 * \param flags a big mask of #corpus_filter_type values indicating filter
 * 	options
 *
 * \returns 0 on success
 */
int corpus_filter_init(struct corpus_filter *f, int symbol_kind,
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
 * \param symbol the unstemmed symbol type
 *
 * \returns 0 on success
 */
int corpus_filter_stem_except(struct corpus_filter *f,
			      const struct corpus_text *symbol);

/**
 * Add a combination rule to to filter.
 *
 * \param f the filter
 * \param type the type to build a combination rule from
 *
 * \returns 0 on success
 */
int corpus_filter_combine(struct corpus_filter *f,
			  const struct corpus_text *type);

/**
 * Add a type to the filter's drop list.
 *
 * \param f the filter
 * \param type the type to drop
 *
 * \returns 0 on success
 */
int corpus_filter_drop(struct corpus_filter *f,
		       const struct corpus_text *type);

/**
 * Add a type to the filter's drop exception list. This overrides the
 * drop settings in the flag used to initialize the filter and any
 * previous calls to corpus_filter_drop() for that type.
 *
 * \param f the filter
 * \param type the type to add to the drop exception list
 *
 * \returns 0 on success
 */
int corpus_filter_drop_except(struct corpus_filter *f,
			      const struct corpus_text *type);

/**
 * Start scanning a text.
 *
 * \param f the filter
 * \param text the text
 * \param type a #corpus_filter_scan value indicating the scan type

 *
 * \returns 0 on success
 */
int corpus_filter_start(struct corpus_filter *f,
			const struct corpus_text *text, int type);

/**
 * Advance a text to the next type.
 *
 * \param f the filter
 *
 * \returns 1 on success, 0 if at the end of the text or an error occurs,
 * 	in which case the `f->error` will be set to the error code
 */
int corpus_filter_advance(struct corpus_filter *f);

/**
 * Get the text corresponding to a given type id.
 *
 * \param f the filter
 * \param id the type id
 *
 * \returns the text for that type
 */
const struct corpus_text *corpus_filter_type(const struct corpus_filter *f,
					     int id);

#endif /* CORPUS_FILTER_H */
