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

#ifndef CORPUS_TOKEN_H
#define CORPUS_TOKEN_H

/**
 * \file token.h
 *
 * Tokens and normalization to types.
 */

#include <stddef.h>
#include <stdint.h>

struct sb_stemmer;

/**
 * Type map descriptor. At a minimum, convert all tokens to
 * composed normal form (NFC). Optionally, apply compatibility maps for
 * NFKC normal and/or apply other transformations:
 *
 *  + #CORPUS_TYPE_COMPAT: apply all compatibility maps required for
 *  	[NFKC normal form](http://unicode.org/reports/tr15/#Norm_Forms)
 *
 *  + #CORPUS_TYPE_CASEFOLD: perform case folding, in most languages (including
 *  	English) mapping uppercase characters to their lowercase equivalents,
 *  	but also performing other normalizations like mapping the
 *  	German Eszett (&szlig;) to "ss"; see
 *  	_The Unicode Standard_ Sec. 5.18 "Case Mappings"
 *  	and the
 *  	[Case Mapping FAQ](http://unicode.org/faq/casemap_charprop.html)
 *  	for more information
 *
 *  + #CORPUS_TYPE_DASHFOLD: dash fold, replace em-dashes, negative signs, and
 *  	anything with the [Dash=Yes](http://unicode.org/reports/tr44/#Dash)
 *  	property with a dash (`-`)
 *
 *  + #CORPUS_TYPE_QUOTFOLD: quote fold, replace double quotes, apostrophes, and
 *      anything with the
 *  	[Quotation Mark=Yes](http://unicode.org/reports/tr44/#Quotation_Mark)
 *  	property with a single quote (`'`)
 *
 *  + #CORPUS_TYPE_RMCC: remove non-white-space control codes (Cc) like
 *  	non-printable ASCII codes; these are defined in
 *  	_The Unicode Standard_ Sec. 23.1 "Control Codes"
 *
 *  + #CORPUS_TYPE_RMDI: remove default ignorables (DI) like soft hyphens and
 *  	zero-width spaces, anything with the
 *  	[Default_Ignorable_Code_Point=Yes]
 *  	(http://www.unicode.org/reports/tr44/#Default_Ignorable_Code_Point)
 *  	property
 *
 *  + #CORPUS_TYPE_RMWS: remove white space (WS), anything with the
 *  	[White_Space=Yes](http://www.unicode.org/reports/tr44/#White_Space)
 *  	property
 */
enum corpus_type_kind {
	CORPUS_TYPE_NORMAL   = 0, /**< transform to composed normal form */
	CORPUS_TYPE_COMPAT   = (1 << 0), /**< apply compatibility mappings */
	CORPUS_TYPE_CASEFOLD = (1 << 1), /**< perform case folding */
	CORPUS_TYPE_DASHFOLD = (1 << 2), /**< replace dashes with `-` */
	CORPUS_TYPE_QUOTFOLD = (1 << 3), /**< replace quotes with `'` */
	CORPUS_TYPE_RMCC     = (1 << 4), /**< remove non-white-space control
					   characters */
	CORPUS_TYPE_RMDI     = (1 << 5), /**< remove default ignorables */
	CORPUS_TYPE_RMWS     = (1 << 6)  /**< remove white space */
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
const char **corpus_stemmer_list(void);

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
 * Compute a hash code from a token.
 *
 * \param tok the token
 *
 * \returns the hash code.
 */
unsigned corpus_token_hash(const struct corpus_text *tok);

/**
 * Test whether two tokens are equal (bitwise). Bitwise equality is more
 * stringent than decoding to the same value.
 *
 * \param tok1 the first token
 * \param tok2 the second token
 *
 * \returns non-zero if the tokens are equal, zero otherwise
 */
int corpus_token_equals(const struct corpus_text *tok1,
			const struct corpus_text *tok2);

/**
 * Compare two types.
 *
 * \param typ1 archetype for the first type
 * \param typ2 archetype for the second type
 *
 * \returns zero if the two archetypes are identical; a negative value
 * 	if the first value is less than the second; a positive value
 * 	if the first value is greater than the second
 */
int corpus_compare_type(const struct corpus_text *typ1,
			const struct corpus_text *typ2);

#endif /* CORPUS_TOKEN_H */
