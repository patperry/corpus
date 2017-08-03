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

#ifndef CORPUS_TYPEMAP_H
#define CORPUS_TYPEMAP_H

/**
 * \file typemap.h
 *
 * Normalization map from tokens to types.
 */

#include <stddef.h>
#include <stdint.h>

struct sb_stemmer;

/**
 * Type map descriptor. At a minimum, convert all tokens to
 * composed normal form (NFC). Optionally, apply compatibility maps for
 * NFKC normal and/or apply other transformations:
 *
 *  + #CORPUS_TYPE_MAPCASE: perform case folding, in most languages (including
 *  	English) mapping uppercase characters to their lowercase equivalents,
 *  	but also performing other normalizations like mapping the
 *  	German Eszett (&szlig;) to "ss"; see
 *  	_The Unicode Standard_ Sec. 5.18 "Case Mappings"
 *  	and the
 *  	[Case Mapping FAQ](http://unicode.org/faq/casemap_charprop.html)
 *  	for more information
 *
 *  + #CORPUS_TYPE_MAPCOMPAT: apply all compatibility maps required for
 *  	[NFKC normal form](http://unicode.org/reports/tr15/#Norm_Forms)
 *
 *  + #CORPUS_TYPE_MAPQUOTE: quote fold, replace curly quotes with straight
 *      quotes
 *
 *  + #CORPUS_TYPE_RMDI: remove default ignorables (DI) like soft hyphens and
 *  	zero-width spaces, anything with the
 *  	[Default_Ignorable_Code_Point=Yes]
 *  	(http://www.unicode.org/reports/tr44/#Default_Ignorable_Code_Point)
 *  	property
 */
enum corpus_type_kind {
	CORPUS_TYPE_NORMAL    = 0, /**< transform to composed normal form */
	CORPUS_TYPE_MAPCASE   = (1 << 0), /**< perform case folding */
	CORPUS_TYPE_MAPCOMPAT = (1 << 1), /**< apply compatibility mappings */
	CORPUS_TYPE_MAPQUOTE  = (1 << 2), /**< replace quotes with `'` */
	CORPUS_TYPE_RMDI      = (1 << 3)  /**< remove default ignorables */
};

/**
 * Type map, for normalizing tokens to types.
 */
struct corpus_typemap {
	struct corpus_text type;/**< type of the token given to the most
				  recent typemap_set() call */
	int8_t ascii_map[128];	/**< a lookup table for the mappings of ASCII
				  characters; -1 indicates deletion */
	struct sb_stemmer *stemmer;
				/**< the stemmer (NULL if none) */
	struct corpus_textset excepts;
				/**< types to exempt from stemming */
	uint32_t *codes;	/**< buffer for intermediate UTF-32 decoding */
	size_t size_max;	/**< token size maximum; normalizing a larger
				 	token will force a reallocation */
	int kind;		/**< the type map kind descriptor, a bit mask
				  of #corpus_type_kind values */
	int charmap_type;	/**< the unicode map type, a bit mask of
				  #corpus_udecomp_type and
				  #corpus_ucasefold_type values */
};

/**
 * Get a list of the stemmer algorithms (canonical names, not aliases).
 *
 * \returns a NULL-terminated array of algorithm names
 */
const char **corpus_stemmer_names(void);

/**
 * Get a list of stop words (common function words) encoded as
 * NULL-terminated UTF-8 strings.
 *
 * \param name the stop word list name
 * \param lenptr if non-NULL, a location to store the word list length
 *
 * \returns a NULL-terminated list of stop words for the given language,
 * 	or NULL if no stop word list exists for the given language
 */
const uint8_t **corpus_stopword_list(const char *name, int *lenptr);

/**
 * Get a list of the stop word list names.
 *
 * \returns a NULL-terminated array of list names
 */
const char **corpus_stopword_names(void);

/**
 * Initialize a new type map of the specified kind.
 *
 * \param map the type map
 * \param kind a bitmask of #corpus_type_kind values, specifying the map type
 * \param stemmer the stemming algorithm name, or NULL to disable stemming
 *
 * \returns 0 on success
 */
int corpus_typemap_init(struct corpus_typemap *map, int kind,
			const char *stemmer);

/**
 * Release the resources associated with a type map.
 * 
 * \param map the type map
 */
void corpus_typemap_destroy(struct corpus_typemap *map);

/**
 * Given a token, set a map to the corresponding type.
 *
 * \param map the type map
 * \param tok the token
 *
 * \returns 0 on success
 */
int corpus_typemap_set(struct corpus_typemap *map,
		       const struct corpus_text *tok);

/**
 * Add a type to the stem exception list. When a normalized token
 * matches anything on this list, it does not get stemmed.
 *
 * \param map the type map
 * \param typ the normalized, unstemmed, type
 *
 * \returns 0 on success
 */
int corpus_typemap_stem_except(struct corpus_typemap *map,
			       const struct corpus_text *typ);

#endif /* CORPUS_TYPEMAP_H */
