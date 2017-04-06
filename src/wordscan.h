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

#ifndef WORDSCAN_H
#define WORDSCAN_H

/**
 * \file wordscan.h
 *
 * Unicode word segmentation.
 */

#include <stdint.h>

/**
 * A word type.
 */
enum word_type {
	WORD_NONE = -1,	/**< at start or end of text */
	WORD_NEWLINE = 0,
	WORD_ZWJ, /**< emoji zwj sequence */
	WORD_EBASE, /**< emoji modifier base */
	WORD_ALETTER,
	WORD_NUMERIC,
	WORD_EXTEND,
	WORD_HEBREW,
	WORD_KATAKANA,
	WORD_REGIONAL,
	WORD_OTHER
};

/**
 * A word scanner, for iterating over the words in a text. Word boundaries
 * are determined according to [UAX #29, Unicode Text Segmentation][uax29].
 *
 * [uax29]: http://unicode.org/reports/tr29/
 */
struct wordscan {
	struct text text;	/**< the input text */
	size_t text_attr;	/**< the input text attributes */

	uint32_t code;		/**< next code point */
	size_t attr;		/**< next code's attributes */
	int prop;		/**< next code's word break property */
	const uint8_t *ptr;	/**< next code's start */

	struct text_iter iter;	/**< an iterator over the input,
				  positioned past next code */
	int iter_prop;		/**< iterator code's word break property */
	const uint8_t *iter_ptr;/**< iterator code's start */

	struct text current;	/**< the current word */
	enum word_type type;	/**< the type of the current word */
};

/**
 * Create a word scanner over a text object.
 *
 * \param scan the scanner to initialize
 * \param text the text
 */
void wordscan_make(struct wordscan *scan, const struct text *text);

/**
 * Advance a scanner to the next word.
 *
 * \param scan the scanner
 *
 * \returns nonzero on success, zero if at the end of the text
 */
int wordscan_advance(struct wordscan *scan);

/**
 * Reset a scanner to the beginning of the text.
 *
 * \param scan the scanner
 */
void wordscan_reset(struct wordscan *scan);

#endif /* WORDSCAN_H */
