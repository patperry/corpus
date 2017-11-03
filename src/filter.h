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

/** The default word connector for compound tokens */
#define CORPUS_FILTER_CONNECTOR '_'

/**
 * Filter type, for specifying which word classes to drop.
 */
enum corpus_filter_type {
	CORPUS_FILTER_KEEP_ALL      = 0,	/**< keep all word types */
	CORPUS_FILTER_DROP_LETTER   = (1 << 0),	/**< drop letter words */
	CORPUS_FILTER_DROP_NUMBER   = (1 << 1),	/**< drop number words */
	CORPUS_FILTER_DROP_PUNCT    = (1 << 2),	/**< drop punctuation words */
	CORPUS_FILTER_DROP_SYMBOL   = (1 << 3),	/**< drop symbol words */
};

/**
 * Text filter type properties.
 */
struct corpus_filter_prop {
	int stem;	/**< stem ID */
	int unspace;	/**< version with spaces removed */

	int has_stem;	/**< whether the stem has been computed */
	int has_unspace; /**< whether the unspaced version has
			    been computed */
	int drop;	/**< whether to drop the type */
};

/**
 * Text filter.
 */
struct corpus_filter {
	struct corpus_symtab symtab;	/**< symbol table */
	struct utf8lite_render render;	/**< type renderer */
	struct corpus_tree combine;	/**< word sequences to combine */
	int *combine_rules;		/**< properties for nodes in the
					  combine tree */
	struct corpus_stem stemmer;	/**< stemmer */
	int has_stemmer;		/**< whether stemmer is in use */
	struct corpus_filter_prop *props;/**< type properties */
	struct corpus_wordscan scan;	/**< current word scan */
	int flags;			/**< filter flags */
	int32_t connector;		/**< word connector */
	int has_scan;			/**< whether a scan is in progress */
	struct utf8lite_text current;	/**< current token */
	int type_id;			/**< current type ID */
	int error;			/**< last error code */
};

/**
 * Initialize a text filter.
 *
 * \param f the filter
 * \param flags a big mask of #corpus_filter_type values indicating filter
 * 	options
 * \param type_kind flags for the symbol table indicating the type kind
 * \param connector the word connector to join space-separated
 *   words in the combination rule, specified as a UTF-32 code
 * \param stemmer the stemming function
 * \param context the stemmer context
 *
 * \returns 0 on success
 */
int corpus_filter_init(struct corpus_filter *f, int flags, int type_kind,
		       int32_t connector, corpus_stem_func stemmer,
		       void *context);

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
			      const struct utf8lite_text *symbol);

/**
 * Add a combination rule to to filter.
 *
 * \param f the filter
 * \param tokens the tokens to build a combination rule from
 *
 * \returns 0 on success
 */
int corpus_filter_combine(struct corpus_filter *f,
			  const struct utf8lite_text *tokens);

/**
 * Add a type to a filter table if it does not already exist there, and
 * get the id of the type in the filter.
 *
 * \param f the filter
 * \param typ the type
 * \param idptr a pointer to store the type id, or NULL
 *
 * \returns 0 on success
 */
int corpus_filter_add_type(struct corpus_filter *f,
			   const struct utf8lite_text *typ, int *idptr);

/**
 * Add a type to the filter's drop list.
 *
 * \param f the filter
 * \param type the type to drop
 *
 * \returns 0 on success
 */
int corpus_filter_drop(struct corpus_filter *f,
		       const struct utf8lite_text *type);

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
			      const struct utf8lite_text *type);

/**
 * Start scanning a text.
 *
 * \param f the filter
 * \param text the text
 *
 * \returns 0 on success
 */
int corpus_filter_start(struct corpus_filter *f,
			const struct utf8lite_text *text);

/**
 * Advance a text to the next type.
 *
 * \param f the filter
 *
 * \returns 1 on success, 0 if at the end of the text or an error occurs,
 * 	in which case the `f->error` will be set to the error code
 */
int corpus_filter_advance(struct corpus_filter *f);

#endif /* CORPUS_FILTER_H */
