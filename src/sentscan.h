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

#ifndef SENTSCAN_H
#define SENTSCAN_H

/**
 * \file sentscan.h
 *
 * Unicode sentence segmentation.
 */

#include <stddef.h>
#include <stdint.h>

/**
 * A sentence break type.
 */
enum sent_type {
	SENT_NONE = -1,	/**< break at the end of the text */
	SENT_PARASEP,	/**< break after a paragraph separator */
	SENT_ATERM,	/**< break after a full stop (.) or other
			  ambiguous terminator */
	SENT_STERM	/**< break after a sentence terminator
			  like a question or exclamation
			  mark (? or !) */
};

/**
 * A sentence scanner, for iterating over the sentences in a text. Sentence
 * boundaries are determined according to
 * [UAX #29, Unicode Text Segmentation][uax29].
 * You can test the word boundary rules in an interactive
 * [online demo][demo].
 *
 * [demo]: http://unicode.org/cldr/utility/breaks.jsp
 * [uax29]: http://unicode.org/reports/tr29/
 */
struct sentscan {
	struct text text;	/**< the input text */
	size_t text_attr;	/**< the input text attributes */

	uint32_t code;		/**< next code point */
	size_t attr;		/**< next code's attributes */
	int prop;		/**< next code's sentence break property */
	const uint8_t *ptr;	/**< next code's start */

	struct text_iter iter;	/**< an iterator over the input,
				  positioned past next code */
	int iter_prop;		/**< iterator code's sentence break property */
	const uint8_t *iter_ptr;/**< iterator code's start */

	struct text current;	/**< the current word */
	enum sent_type type;	/**< the type of the current sentence */
};

/**
 * Create a sentence scanner over a text object.
 *
 * \param scan the scanner to initialize
 * \param text the text
 */
void sentscan_make(struct sentscan *scan, const struct text *text);

/**
 * Advance a scanner to the next sentence.
 *
 * \param scan the scanner
 *
 * \returns nonzero on success, zero if at the end of the text
 */
int sentscan_advance(struct sentscan *scan);

/**
 * Reset a scanner to the beginning of the text.
 *
 * \param scan the scanner
 */
void sentscan_reset(struct sentscan *scan);

#endif /* SENTSCAN_H */
