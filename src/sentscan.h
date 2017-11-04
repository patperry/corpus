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

#ifndef CORPUS_SENTSCAN_H
#define CORPUS_SENTSCAN_H

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
enum corpus_sent_type {
	CORPUS_SENT_NONE = -1,	/**< break at the end of the text */
	CORPUS_SENT_PARASEP,	/**< break after a paragraph separator */
	CORPUS_SENT_ATERM,	/**< break after a full stop (.) or other
				  ambiguous terminator */
	CORPUS_SENT_STERM	/**< break after a sentence terminator
				  like a question or exclamation
				  mark (? or !) */
};

/**
 * Flags for controlling the sentence breaking.
 */
enum corpus_sentscan_type {
	CORPUS_SENTSCAN_STRICT = 0, /**< strict UAX 29 conformance */
	CORPUS_SENTSCAN_SPCRLF = (1 << 0) /**< treat CR and LF like spaces */
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
struct corpus_sentscan {
	struct utf8lite_text text;/**< the input text */
	int flags;		/**< the scan flags */

	int32_t code;		/**< next code point */
	size_t attr;		/**< next code's attributes */
	int prop;		/**< next code's sentence break property */
	const uint8_t *ptr;	/**< next code's start */

	struct utf8lite_text_iter iter;	/**< an iterator over the input,
					  positioned past next code */
	int iter_prop;		/**< iterator code's sentence break property */
	const uint8_t *iter_ptr;/**< iterator code's start */

	struct utf8lite_text current;	/**< the current sentence */
	enum corpus_sent_type type;	/**< the type of the current sentence */
	int at_end;		/**< whether the scanner is at the end of
				  the text */
};

/**
 * Create a sentence scanner over a text object.
 *
 * \param scan the scanner to initialize
 * \param text the text
 * \param flags a bit mask of #corpus_sentscan_type flags controlling
 * 	the scanning behavior
 */
void corpus_sentscan_make(struct corpus_sentscan *scan,
			  const struct utf8lite_text *text,
			  int flags);

/**
 * Advance a scanner to the next sentence.
 *
 * \param scan the scanner
 *
 * \returns nonzero on success, zero if at the end of the text
 */
int corpus_sentscan_advance(struct corpus_sentscan *scan);

/**
 * Reset a scanner to the beginning of the text.
 *
 * \param scan the scanner
 */
void corpus_sentscan_reset(struct corpus_sentscan *scan);

#endif /* CORPUS_SENTSCAN_H */
