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

#include <assert.h>
#include <stdint.h>
#include "unicode/casefold.h"
#include "unicode/combining.h"
#include "unicode/decompose.h"
#include "errcode.h"
#include "unicode.h"

/*
  Source:
   http://www.unicode.org/versions/Unicode7.0.0/UnicodeStandard-7.0.pdf
   page 124, 3.9 "Unicode Encoding Forms", "UTF-8"

  Table 3-7. Well-Formed UTF-8 Byte Sequences
  -----------------------------------------------------------------------------
  |  Code Points        | First Byte | Second Byte | Third Byte | Fourth Byte |
  |  U+0000..U+007F     |     00..7F |             |            |             |
  |  U+0080..U+07FF     |     C2..DF |      80..BF |            |             |
  |  U+0800..U+0FFF     |         E0 |      A0..BF |     80..BF |             |
  |  U+1000..U+CFFF     |     E1..EC |      80..BF |     80..BF |             |
  |  U+D000..U+D7FF     |         ED |      80..9F |     80..BF |             |
  |  U+E000..U+FFFF     |     EE..EF |      80..BF |     80..BF |             |
  |  U+10000..U+3FFFF   |         F0 |      90..BF |     80..BF |      80..BF |
  |  U+40000..U+FFFFF   |     F1..F3 |      80..BF |     80..BF |      80..BF |
  |  U+100000..U+10FFFF |         F4 |      80..8F |     80..BF |      80..BF |
  -----------------------------------------------------------------------------

  (courtesy of https://github.com/JulienPalard/is_utf8 )
*/

// 80: 1000 0000
// 8F: 1000 1111
// 90: 1001 0000
// 9F: 1001 1111
// A0: 1010 0000
// BF: 1011 1111
// E0: 1110 0000

int scan_utf8(const uint8_t **bufptr, const uint8_t *end)
{
	const uint8_t *ptr = *bufptr;
	uint_fast8_t ch, ch1;
	unsigned nc;
	int err;

	if (*bufptr >= end)
		return ERROR_INVAL;

	// determine number of bytes
	ch1 = *ptr++;
	if ((ch1 & 0x80) == 0) {
		goto success;
	} else if ((ch1 & 0xE2) == 0xC2) {
		nc = 1;
	} else if ((ch1 & 0xF0) == 0xE0) {
		nc = 2;
	} else if ((ch1 & 0xFC) == 0xF0 || ch1 == 0xF4) {
		nc = 3;
	} else {
		// expecting bytes in the following ranges: 00..7F C2..F4
		goto backtrack;
	}

	// ensure string is long enough
	if (ptr + nc > end) {
		// expecting another continuation byte
		goto backtrack;
	}

	// validate the first continuation byte
	ch = *ptr++;
	switch (ch1) {
	case 0xE0:
		if ((ch & 0xE0) != 0xA0) {
			// expecting a byte between A0 and BF
			goto backtrack;
		}
		break;
	case 0xED:
		if ((ch & 0xE0) != 0x80) {
			// expecting a byte between A0 and 9F
			goto backtrack;
		}
		break;
	case 0xF0:
		if ((ch & 0xE0) != 0xA0 && (ch & 0xF0) != 0x90) {
			// expecting a byte between 90 and BF
			goto backtrack;
		}
		break;
	case 0xF4:
		if ((ch & 0xF0) != 0x80) {
			// expecting a byte between 80 and 8F
			goto backtrack;
		}
	default:
		if ((ch & 0xC0) != 0x80) {
			// expecting a byte between 80 and BF
			goto backtrack;
		}
		break;
	}
	nc--;

	// validate the trailing continuation bytes
	while (nc-- > 0) {
		ch = *ptr++;
		if ((ch & 0xC0) != 0x80) {
			// expecting a byte between 80 and BF
			goto backtrack;
		}
	}

success:
	err = 0;
	goto out;
backtrack:
	ptr--;
	err = ERROR_INVAL;
out:
	*bufptr = ptr;
	return err;
}


void decode_utf8(const uint8_t **bufptr, uint32_t *codeptr)
{
	const uint8_t *ptr = *bufptr;
	uint32_t code;
	uint_fast8_t ch;
	unsigned nc;

	ch = *ptr++;
	if (!(ch & 0x80)) {
		code = ch;
		nc = 0;
	} else if (!(ch & 0x20)) {
		code = ch & 0x1F;
		nc = 1;
	} else if (!(ch & 0x10)) {
		code = ch & 0x0F;
		nc = 2;
	} else {
		code = ch & 0x07;
		nc = 3;
	}

	while (nc-- > 0) {
		ch = *ptr++;
		code = (code << 6) + (ch & 0x3F);
	}

	*bufptr = ptr;
	*codeptr = code;
}


// http://www.fileformat.info/info/unicode/utf8.htm
void encode_utf8(uint32_t code, uint8_t **bufptr)
{
	uint8_t *ptr = *bufptr;
	uint32_t x = code;

	if (x <= 0x7F) {
		*ptr++ = x;
	} else if (x <= 0x07FF) {
		*ptr++ = 0xC0 | (x >> 6);
		*ptr++ = 0x80 | (x & 0x3F);
	} else if (x <= 0xFFFF) {
		*ptr++ = 0xE0 | (x >> 12);
		*ptr++ = 0x80 | ((x >> 6) & 0x3F);
		*ptr++ = 0x80 | (x & 0x3F);
	} else {
		*ptr++ = 0xF0 | (x >> 18);
		*ptr++ = 0x80 | ((x >> 12) & 0x3F);
		*ptr++ = 0x80 | ((x >> 6) & 0x3F);
		*ptr++ = 0x80 | (x & 0x3F);
	}

	*bufptr = ptr;
}


/* From Unicode-8.0 Section 3.12 Conjoining Jamo Behavior */
#define HANGUL_SBASE 0xAC00
#define HANGUL_LBASE 0x1100
#define HANGUL_VBASE 0x1161
#define HANGUL_TBASE 0x11A7
#define HANGUL_LCOUNT 19
#define HANGUL_VCOUNT 21
#define HANGUL_TCOUNT 28
#define HANGUL_NCOUNT (HANGUL_VCOUNT * HANGUL_TCOUNT)
#define HANTUL_SCOUNT (HANGUL_LCOUNT * HANGUL_NCOUNT)


static void hangul_decompose(uint32_t code, uint32_t **bufp)
{
	uint32_t *dst = *bufp;
	uint32_t sindex = code - HANGUL_SBASE;
	uint32_t lindex = sindex / HANGUL_NCOUNT;
	uint32_t vindex = (sindex % HANGUL_NCOUNT) / HANGUL_TCOUNT;
	uint32_t tindex = sindex % HANGUL_TCOUNT;
	uint32_t lpart = HANGUL_LBASE + lindex;
	uint32_t vpart = HANGUL_VBASE + vindex;
	uint32_t tpart = HANGUL_TBASE + tindex;

	*dst++ = lpart;
	*dst++ = vpart;
	if (tindex > 0) {
		*dst++ = tpart;
	}

	*bufp = dst;
}


static void casefold(int type, uint32_t code, uint32_t **bufp)
{
	const uint32_t block_size = CASEFOLD_BLOCK_SIZE;
	unsigned i = casefold_stage1[code / block_size];
	struct casefold c = casefold_stage2[i][code % block_size];
	unsigned length = c.length;
	const uint32_t *src;
	uint32_t *dst;

	if (length == 0) {
		dst = *bufp;
		*dst++ = code;
		*bufp = dst;
	} else if (length == 1) {
		utf32_decompose(type, c.data, bufp);
	} else {
		src = &casefold_mapping[c.data];
		while (length-- > 0) {
			utf32_decompose(type, *src, bufp);
			src++;
		}
	}
}


void utf32_decompose(int type, uint32_t code, uint32_t **bufp)
{
	const uint32_t block_size = DECOMPOSITION_BLOCK_SIZE;
	unsigned i = decomposition_stage1[code / block_size];
	struct decomposition d = decomposition_stage2[i][code % block_size];
	unsigned length = d.length;
	const uint32_t *src;
	uint32_t *dst;

	if (length == 0 || (d.type > 0 && !(type & (1 << (d.type - 1))))) {
		if (type & UCASEFOLD_ALL) {
			casefold(type, code, bufp);
		} else {
			dst = *bufp;
			*dst++ = code;
			*bufp = dst;
		}
	} else if (length == 1) {
		utf32_decompose(type, d.data, bufp);
	} else if (d.type >= 0) {
		src = &decomposition_mapping[d.data];
		while (length-- > 0) {
			utf32_decompose(type, *src, bufp);
			src++;
		}
	} else {
		hangul_decompose(code, bufp);
	}
}


void utf32_reorder(uint32_t *ptr, uint32_t *end)
{
	uint32_t *c_begin, *c_end, *c_tail, *c_ptr;
	uint32_t code, code_prev;
	uint32_t cl, cl_prev;

	assert(ptr <= end);

	while (ptr != end) {
		c_begin = ptr;
		code = *ptr++;
		cl = combining_class(code);

		// skip to the next combining mark
		if (cl == 0) {
			continue;
		}

		// mark the start of the combining mark sequence (c_begin)
		// encode the combining class in the high 8 bits
		*c_begin = code | (cl << 24);

		// the combining mark sequence ends at the first starter
		// (c_end)
		c_end = ptr;
		while (c_end != end) {
			// until we hit a non-starter, encode the combining
			// class in the high 8 bits of the code
			code = *ptr++;
			cl = combining_class(code);
			if (cl == 0) {
				break;
			}

			*c_end = code | (cl << 24);
			c_end++;
		}

		// sort the combining marks, using insertion sort (stable)
		for (c_tail = c_begin + 1; c_tail != c_end; c_tail++) {
			c_ptr = c_tail;
			code = *c_ptr;
			cl = code & (0xFF << 24);

			while (c_ptr != c_begin) {
				code_prev = c_ptr[-1];
				cl_prev = code_prev & (0xFF << 24);

				if (cl_prev <= cl) {
					break;
				}

				// swap with previous item
				c_ptr[0] = code_prev;

				// move down
				c_ptr--;
			}

			// complete the final swap
			*c_ptr = code;
		}

		// remove the combining mark annotations
		while (c_begin != c_end) {
			code = *c_begin;
			*c_begin = code & (~(0xFF << 24));
			c_begin++;
		}
	}
}
