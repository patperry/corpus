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

#ifndef TOKEN_H
#define TOKEN_H

/**
 * \file token.h
 *
 * Token and type utilities.
 */

#include <stddef.h>
#include <stdint.h>

/*
 * Type Normalization
 * ------------------
 *
 * 1. Decompose to NFD or NFKD normalization form.
 *
 *     Source: http://unicode.org/reports/tr15/
 *
 *     Defaults to NFKD; specify `TYPE_NOCOMPAT` for NFD
 *
 *
 * 2. Perform case folding.
 *
 * 	'A' -> 'a'
 * 	'B' -> 'b'
 * 	U+00DF -> 'ss'
 * 	etc.
 *
 * 	Source: UnicodeStandard-8.0, Sec. 5.18, p. 236
 * 	http://unicode.org/faq/casemap_charprop.html
 *
 *	Disabled with `TYPE_NOFOLD_CASE`
 *
 *
 * 3. Perform dash folding.
 *
 * 	Replace with U+002D (-):
 *
 *		U+2212 MINUS SIGN
 *		U+2E3A TWO-EM DASH
 *		U+2E3B THREE-EM DASH
 *		U+301C WAVE DASH
 *
 *		+ anything else with "Dash=Yes" property
 *
 *	Source: http://unicode.org/Public/UNIDATA/PropList.txt
 *		http://unicode.org/reports/tr44/#Dash
 *
 *	Disabled with `TYPE_NOFOLD_DASH`
 *
 *
 * 4. Perform quote folding.
 *
 * 	Replace with U+0027 ('):
 *
 *		U+0022 QUOTATION MARK (")
 *		U+00AB LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
 *		U+00BB RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
 * 		U+2018 LEFT SINGLE QUOTATION MARK
 * 		U+2019 RIGHT SINGLE QUOTATION MARK
 * 		U+201A SINGLE LOW-9 QUOTATION MARK
 *		U+201B SINGLE HIGH-REVERSED-9 QUOTATION MARK
 *		U+201C LEFT DOUBLE QUOTATION MARK
 *		U+201D RIGHT DOUBLE QUOTATION MARK
 *		U+201E DOUBLE LOW-9 QUOTATION MARK
 *		U+201F DOUBLE HIGH-REVERSED-9 QUOTATION MARK
 *
 * 		+ anything else with "Quotation Mark=Yes" property
 *
 *	Source: http://unicode.org/Public/UNIDATA/PropList.txt
 *		http://unicode.org/reports/tr44/#Quotation_Mark
 *
 *	Disabled with `TYPE_NOFOLD_QUOT`
 *
 *
 * 5. Remove non-whitespace control characters (Cc).
 *
 *	U+0000..U+0008	(C0)
 *	U+000E..U+001F	(C0)
 *	U+007F		(delete)
 *	U+0080..U+0084	(C1)
 *	U+0086..U+009F	(C1)
 *
 *	Source:
 *	UnicodeStandard-8.0, Sec. 23.1, p. 808
 *
 *	Disabled with `TYPE_KEEP_CC`
 *
 *
 * 6. Remove default ignorable code points (DI).
 *
 *	U+00AD	SOFT HYPHEN
 *	U+200B	ZERO WIDTH SPACE
 *	U+FEFF	ZERO WIDTH NO-BREAK SPACE
 *
 *	+ anything else with "Default_Ignorable_Code_Point=Yes" property
 *
 *	Source: http://unicode.org/Public/UNIDATA/DerivedCoreProperties.txt
 *	UnicodeStandard-8.0, Sec. 5.21, p. 253
 *	http://www.unicode.org/reports/tr44/#Default_Ignorable_Code_Point
 *
 *	Disabled with `TYPE_KEEP_DI`
 *
 *
 * 7. Remove white space (WS).
 *
 *	U+0009..U+000D	(control-0009>..<control-000D>)
 *	U+0020		SPACE
 *	U+0085		<control-0085>
 * 	U+00A0		NO-BREAK SPACE
 *	U+1680		OGHAM SPACE MARK
 *	U+2000..U+200A	EN QUAD..HAIR SPACE
 *	U+2028		LINE SEPARATOR
 *	U+2029		PARAGRAPH SEPARATOR
 *	U+202F		NARROW NO-BREAK SPACE
 *	U+205F		MEDIUM MATHEMATICAL SPACE
 *	U+3000		IDEOGRAPHIC SPACE
 *
 *	(anything with White_Space=Yes)
 *
 *	Source:
 *	http://www.unicode.org/reports/tr44/#White_Space
 *	http://unicode.org/Public/UNIDATA/PropList.txt
 *
 *	Disabled with `TYPE_KEEP_WS`
 */

enum type_kind {
	TYPE_NORMAL   = 0,        /**< apply decomposition mappings */
	TYPE_COMPAT   = (1 << 0), /**< apply compatibility mappings */
	TYPE_CASEFOLD = (1 << 1), /**< perform case folding */
	TYPE_DASHFOLD = (1 << 2), /**< replace dashes with `'-'` */
	TYPE_QUOTFOLD = (1 << 3), /**< replace quotes with `'` */
	TYPE_RMCC     = (1 << 4), /**< remove non-whitespace control characters */
	TYPE_RMDI     = (1 << 5), /**< remove default ignorables */
	TYPE_RMWS     = (1 << 6)  /**< remove whitespace */
};


struct typebuf {
	int8_t ascii_map[128];
	struct text text;
	uint32_t *code;
	size_t size_max;
	int kind;
	int map_type;
};

int typebuf_init(struct typebuf *buf, int kind);
void typebuf_destroy(struct typebuf *buf);
int typebuf_set(struct typebuf *buf, const struct text *tok);

uint64_t tok_hash(const struct text *tok);
int tok_equals(const struct text *tok1, const struct text *tok2);
int compare_typ(const struct text *typ1, const struct text *typ2);

#endif /* TOKEN_H */
