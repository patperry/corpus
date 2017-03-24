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

#ifndef UNICODE_H
#define UNICODE_H

/**
 * \file
 *
 * Unicode handling.
 */

#include <stddef.h>
#include <stdint.h>

/** Unicode replacement character */
#define UNICODE_REPLACEMENT	0xFFFD

/** Last valid unicode codepoint */
#define UNICODE_MAX 0x10FFFF

/** Number of bytes in the UTF-8 encoding of a valid unicode codepoint. */
#define UTF8_ENCODE_LEN(u) \
	((u) <= 0x7F     ? 1 : \
	 (u) <= 0x07FF   ? 2 : \
	 (u) <= 0xFFFF   ? 3 : 4)

/** Number of 16-bit code units in the UTF-16 encoding of a valid unicode
 *  codepoint */
#define UTF16_ENCODE_LEN(u) \
	((u) <= 0xFFFF ? 1 : 2)

/** Indicates whether a 16-bit code unit is a UTF-16 high surrogate.
 *  High surrogates are in the range 0xD800 `(1101 1000 0000 0000)`
 *  to 0xDBFF `(1101 1011 1111 1111)`. */
#define IS_UTF16_HIGH(x) (((x) & 0xFC00) == 0xD800)

/** Indicates whether a 16-bit code unit is a UTF-16 low surrogate.
 *  Low surrogates are in the range 0xDC00 `(1101 1100 0000 0000)`
 *  to 0xDFFF `(1101 1111 1111 1111)`. */
#define IS_UTF16_LOW(x) (((x) & 0xFC00) == 0xDC00)

/** Given the high and low UTF-16 surrogates, compute the unicode codepoint. */
#define DECODE_UTF16_PAIR(h, l) \
	(((((h) & 0x3FF) << 10) | ((l) & 0x3FF)) + 0x10000)

/** Given the first byte in a valid UTF-8 byte sequence, determine the number of
 *  continuation bytes */
#define UTF8_TAIL_LEN(x) \
	(  ((x) & 0x80) == 0x00 ? 0 \
	 : ((x) & 0xE0) == 0xC0 ? 1 \
	 : ((x) & 0xF0) == 0xE0 ? 2 : 3)

/** Maximum number of UTF-8 continuation bytes in a valid encoded character */
#define UTF8_TAIL_MAX 3

/** Indicates whether a given unsigned integer is a valid unicode codepoint */
#define IS_UNICODE(x) \
	(((x) <= UNICODE_MAX) && !IS_UTF16_HIGH(x) && !IS_UTF16_LOW(x))

/**
 * Validate the first character in a UTF-8 character buffer.
 *
 * \param bufptr a pointer to the input buffer; on exit, a pointer to
 * 	the end of the first valid UTF-8 character, or the first invalid
 * 	byte in the encoding
 * \param end the end of the input buffer
 *
 * \returns 0 on success
 */
int scan_utf8(const uint8_t **bufptr, const uint8_t *end);

/**
 * Decode the first codepoint from a UTF-8 character buffer.
 * 
 * \param bufptr on input, a pointer to the start of the character buffer;
 * 	on exit, a pointer to the end of the first UTF-8 character in
 * 	the buffer
 * \param on exit, the first codepoint in the buffer
 */
void decode_utf8(const uint8_t **bufptr, uint32_t *codeptr);

/**
 * Encode a codepoint into a UTF-8 character buffer. The codepoint must
 * be a valid unicode character (according to #IS_UNICODE) and the buffer
 * must have space for at least #UTF8_ENCODE_LEN bytes.
 *
 * \param code the codepoint
 * \param bufptr on input, a pointer to the start of the buffer;
 * 	on exit, a pointer to the end of the encoded codepoint
 */
void encode_utf8(uint32_t code, uint8_t **bufptr);

/**
 * Unicode character decomposition mappings. The compatibility mappings are
 * defined in TR44 Sec. 5.7.3 Table 14.
 */
enum udecomp_type {
	UDECOMP_NORMAL = 0,          /**< normalization (required for NFD) */
	UDECOMP_FONT = (1 << 0),     /**< font variant */
	UDECOMP_NOBREAK = (1 << 1),  /**< no-break version of a space or hyphen */
	UDECOMP_INITIAL = (1 << 2),  /**< initial presentation form (Arabic) */
	UDECOMP_MEDIAL = (1 << 3),   /**< medial presentation form (Arabic) */
	UDECOMP_FINAL = (1 << 4),    /**< final presentation form (Arabic) */
	UDECOMP_ISOLATED = (1 << 5), /**< isolated presentation form (Arabic) */
	UDECOMP_CIRCLE = (1 << 6),   /**< encircled form */
	UDECOMP_SUPER = (1 << 7),    /**< superscript form */
	UDECOMP_SUB = (1 << 8),      /**< subscript form */
	UDECOMP_VERTICAL = (1 << 9), /**< vertical layout presentation form */
	UDECOMP_WIDE = (1 << 10),    /**< wide (or zenkaku) compatibility */
	UDECOMP_NARROW = (1 << 11),  /**< narrow (or hankaku) compatibility */
	UDECOMP_SMALL = (1 << 12),   /**< small variant form (CNS compatibility) */
	UDECOMP_SQUARE = (1 << 13),  /**< CJK squared font variant */
	UDECOMP_FRACTION = (1 << 14),/**< vulgar fraction form */
	UDECOMP_COMPAT = (1 << 15),  /**< unspecified compatibility */

	UDECOMP_ALL = ((1 << 16) - 1)/**< all decompositions (required for NFKD) */
};

/**
 * Unicode case folding. These are defined in TR44 Sec. 5.6.
 */
enum ucasefold_type {
	UCASEFOLD_NONE = 0,		/**< do not perform any case folding */
	UCASEFOLD_ALL = (1 << 16)	/**< perform case folding */
};

/**
 * Maximum size (in codepoints) of a single code point's decomposition.
 *
 * From TR44 Sec. 5.7.3: "Compatibility mappings are guaranteed to be no longer
 * than 18 characters, although most consist of just a few characters."
 */
#define UNICODE_DECOMP_MAX 18

void utf32_decompose(int type, uint32_t code, uint32_t **bufp);
void utf32_reorder(uint32_t *ptr, uint32_t *end);
void utf32_compose(uint32_t *buf, size_t *lenp);


#endif /* UNICODE_H */
