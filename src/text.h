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

#ifndef TEXT_H
#define TEXT_H

/**
 * \file text.h
 *
 * Text data type and utility functions.
 */

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

/**
 * UTF-8 encoded text, possibly containing JSON-compatible backslash (`\`)
 * escape codes which should be interpreted as such. The client assumes
 * all responsibility for managing the memory for the underlying UTF8-data.
 */
struct text {
	uint8_t *ptr;	/**< pointer to valid UTF-8 data */
	size_t attr;	/**< text attributes */
};

/** Whether the text decodes to UTF-8. Need not be set for ASCII text. */
#define TEXT_UTF8_BIT	((size_t)1 << (CHAR_BIT * sizeof(size_t) - 1))

/** Whether the text contains a backslash (`\`) that should be interpreted
 *  as an escape */
#define TEXT_ESC_BIT	((size_t)1 << (CHAR_BIT * sizeof(size_t) - 2))

/** Size of the encoded text, in bytes; (decoded size) <= (encoded size) */
#define TEXT_SIZE_MASK	((size_t)SIZE_MAX >> 2)

/** Maximum size of encode text, in bytes. */
#define TEXT_SIZE_MAX	TEXT_SIZE_MASK

/** The encoded size of the text, in bytes */
#define TEXT_SIZE(text)		((text)->attr & TEXT_SIZE_MASK)

/** Indicates whether the text decodes to ASCII. For this to be true,
 *  the text must be encoded in ASCII and not have any escapes that
 *  decode to non-ASCII codepoints. */
#define TEXT_IS_ASCII(text)	(((text)->attr & TEXT_UTF8_BIT) ? 0 : 1)

/** Indicates whether the text contains a backslash (`\`) that should
 *  be interpreted as an escape code */
#define TEXT_HAS_ESC(text)	(((text)->attr & TEXT_ESC_BIT) ? 1 : 0)

/**
 * Flags for text_assign().
 */
enum text_flag {
	/** do not interpret backslash (`\`) as an escape */
	TEXT_NOESCAPE = (1 << 0),

	/** do not perform any validation on the input */
	TEXT_NOVALIDATE = (1 << 1)
};

/**
 * Assign a text value to point to data in the specified memory location
 * after validating the input data.
 *
 * \param text the text value
 * \param ptr a pointer to the underlying memory buffer
 * \param size the number of bytes in the underlying memory buffer
 * \param flags #text_flag bitmask specifying input type
 *
 * \returns 0 on success
 */
int text_assign(struct text *text, const uint8_t *ptr, size_t size, int flags);

/**
 * Write the raw bytes in a text value to the given buffer, decoding
 * escape sequences if necessary.
 *
 * \param text the text value
 * \param buf a pointer to the destination buffer; must have at least
 *        #TEXT_SIZE(`text`) bytes available
 * \param sizeptr on exit, a pointer to the size (in bytes) of the decoded
 *        text; this will be less than or equal to #TEXT_SIZE(`text`)
 *
 * \returns 0 on success
 */
int text_unescape(const struct text *text, uint8_t *buf, size_t *sizeptr);

#endif /* TEXT_H */
