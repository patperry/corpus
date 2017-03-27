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

/**
 * An iterator over the decoded UTF-32 characters in a text.
 */
struct text_iter {
	const uint8_t *ptr;	/**< current position in the text buffer*/
	const uint8_t *end;	/**< end of the text buffer */
	size_t attr;		/**< text attributes */
	uint32_t current;	/**< current character (UTF-32) */
};

/** Whether the text contains a non-ASCII UTF-8 character */
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

/** Indicates whether the text definitely decodes to ASCII. For this to be true,
 *  the text must be encoded in ASCII and not have any escapes that decode to
 *  non-ASCII codepoints.
 */
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
 * Initialize a new text object by allocating space for and copying
 * the encoded characters from another text object.
 *
 * \param text the object to initialize
 * \param other the object to copy
 *
 * \returns 0 on success, or non-zero on memory allocation failure
 */
int text_init_copy(struct text *text, const struct text *other);

/**
 * Free the resources associated with a text object.
 *
 * \param text the text object
 */
void text_destroy(struct text *text);

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
 * Initialize a text iterator to start at the beginning of a text.
 *
 * \param it the iterator
 * \param text the text
 */
void text_iter_make(struct text_iter *it, const struct text *text);

/**
 * Advance to the next character in a text.
 *
 * \param it the text iterator
 *
 * \returns non-zero if the iterator successfully advanced; zero if
 * 	the iterator has passed the end of the text
 */
int text_iter_advance(struct text_iter *it);

/**
 * Reset an iterator to the beginning of the text.
 *
 * \param it the text iterator
 */
void text_iter_reset(struct text_iter *it);

#endif /* TEXT_H */
