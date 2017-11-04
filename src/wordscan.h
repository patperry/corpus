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

#ifndef CORPUS_WORDSCAN_H
#define CORPUS_WORDSCAN_H

/**
 * \file wordscan.h
 *
 * Unicode word segmentation.
 */

#include <stddef.h>
#include <stdint.h>

/**
 * The word type as determined by the first character.
 */
enum corpus_word_type {
	CORPUS_WORD_NONE = -1,	/**< EOT, white space, control, mark */
	CORPUS_WORD_LETTER,	/**< word that contains letters */
	CORPUS_WORD_NUMBER,	/**< word that appears to be a number */
	CORPUS_WORD_PUNCT,	/**< punctuation */
	CORPUS_WORD_SYMBOL	/**< symbol */
};

/**
 * A word scanner, for iterating over the words in a text. Word boundaries
 * are determined according to [UAX #29, Unicode Text Segmentation][uax29].
 * You can test the word boundary rules in an interactive
 * [online demo][demo].
 *
 * [demo]: http://unicode.org/cldr/utility/breaks.jsp
 * [uax29]: http://unicode.org/reports/tr29/
 */
struct corpus_wordscan {
	int32_t code;		/**< next code point */
	size_t attr;		/**< next code's attributes */
	int prop;		/**< next code's word break property */
	const uint8_t *ptr;	/**< next code's start */

	struct utf8lite_text_iter iter;	/**< an iterator over the input,
				  positioned past next code */
	int iter_prop;		/**< iterator code's word break property */
	const uint8_t *iter_ptr;/**< iterator code's start */

	struct utf8lite_text current;	/**< the current word */
	enum corpus_word_type type;	/**< the type of the current word */
};

/**
 * Create a word scanner over a text object.
 *
 * \param scan the scanner to initialize
 * \param text the text
 */
void corpus_wordscan_make(struct corpus_wordscan *scan,
			  const struct utf8lite_text *text);

/**
 * Advance a scanner to the next word.
 *
 * \param scan the scanner
 *
 * \returns nonzero on success, zero if at the end of the text
 */
int corpus_wordscan_advance(struct corpus_wordscan *scan);

/**
 * Reset a scanner to the beginning of the text.
 *
 * \param scan the scanner
 */
void corpus_wordscan_reset(struct corpus_wordscan *scan);

#endif /* CORPUS_WORDSCAN_H */
