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

#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include "../lib/utf8lite/src/utf8lite.h"
#include "error.h"
#include "memory.h"
#include "text.h"

#define CORPUS_TEXT_CODE_NONE ((uint32_t)-1)


/* http://stackoverflow.com/a/11986885 */
#define hextoi(ch) ((ch > '9') ? (ch &~ 0x20) - 'A' + 10 : (ch - '0'))

static int assign_esc(struct corpus_text *text, const uint8_t *ptr,
		      size_t size);
static int assign_esc_unsafe(struct corpus_text *text, const uint8_t *ptr,
			     size_t size);
static int assign_raw(struct corpus_text *text, const uint8_t *ptr,
		      size_t size);
static int assign_raw_unsafe(struct corpus_text *text, const uint8_t *ptr,
			     size_t size);

static int decode_uescape(const uint8_t **inputptr, const uint8_t *end,
			  uint32_t *codeptr);
static void decode_valid_escape(const uint8_t **inputptr, uint32_t *codeptr);
static void decode_valid_uescape(const uint8_t **inputptr, uint32_t *codeptr);


static void iter_retreat_escaped(struct corpus_text_iter *it,
				 const uint8_t *begin);
static void iter_retreat_raw(struct corpus_text_iter *it);


int corpus_text_init_copy(struct corpus_text *text,
			  const struct corpus_text *other)
{
        size_t size = CORPUS_TEXT_SIZE(other);
        size_t attr = other->attr;
	int err;

	if (size) {
		if (!(text->ptr = corpus_malloc(size + 1))) {
			err = CORPUS_ERROR_NOMEM;
			corpus_log(err, "failed allocating text object");
			return err;
		}

		memcpy(text->ptr, other->ptr, size);
		text->ptr[size] = '\0';
	} else {
		text->ptr = NULL;
	}
        text->attr = attr;
        return 0;
}


void corpus_text_destroy(struct corpus_text *text)
{
        corpus_free(text->ptr);
}


int corpus_text_assign(struct corpus_text *text, const uint8_t *ptr,
		       size_t size, int flags)
{
	int err;

	if (flags & CORPUS_TEXT_NOESCAPE) {
		if (flags & CORPUS_TEXT_NOVALIDATE) {
			err = assign_raw_unsafe(text, ptr, size);
		} else {
			err = assign_raw(text, ptr, size);
		}
	} else {
		if (flags & CORPUS_TEXT_NOVALIDATE) {
			err = assign_esc_unsafe(text, ptr, size);
		} else {
			err = assign_esc(text, ptr, size);
		}
	}

	return err;
}



void corpus_text_iter_make(struct corpus_text_iter *it,
			   const struct corpus_text *text)
{
	it->ptr = text->ptr;
	it->end = it->ptr + CORPUS_TEXT_SIZE(text);
	it->text_attr = text->attr;
	it->attr = 0;
	it->current = CORPUS_TEXT_CODE_NONE;
}


int corpus_text_iter_can_advance(const struct corpus_text_iter *it)
{
	return (it->ptr != it->end);
}


int corpus_text_iter_advance(struct corpus_text_iter *it)
{
	const uint8_t *ptr = it->ptr;
	size_t text_attr = it->text_attr;
	uint32_t code;
	size_t attr;

	if (!corpus_text_iter_can_advance(it)) {
		goto at_end;
	}

	attr = 0;
	code = *ptr++;

	if (code == '\\' && (text_attr & CORPUS_TEXT_ESC_BIT)) {
		attr = CORPUS_TEXT_ESC_BIT;
		decode_valid_escape(&ptr, &code);
		if (code >= 0x80) {
			attr |= CORPUS_TEXT_UTF8_BIT;
		}
	} else if (code >= 0x80) {
		attr = CORPUS_TEXT_UTF8_BIT;
		ptr--;
		utf8lite_decode_utf8(&ptr, &code);
	}

	it->ptr = ptr;
	it->current = code;
	it->attr = attr;
	return 1;

at_end:
	it->current = CORPUS_TEXT_CODE_NONE;
	it->attr = 0;
	return 0;
}


void corpus_text_iter_reset(struct corpus_text_iter *it)
{
	const size_t size = (it->text_attr & CORPUS_TEXT_SIZE_MASK);
	const uint8_t *begin = it->end - size;

	it->ptr = begin;
	it->current = CORPUS_TEXT_CODE_NONE;
	it->attr = 0;
}


void corpus_text_iter_skip(struct corpus_text_iter *it)
{
	it->ptr = it->end;
	it->current = CORPUS_TEXT_CODE_NONE;
	it->attr = 0;
}


int corpus_text_iter_can_retreat(const struct corpus_text_iter *it)
{
	const size_t size = (it->text_attr & CORPUS_TEXT_SIZE_MASK);
	const uint8_t *begin = it->end - size;
	const uint8_t *ptr = it->ptr;
	uint32_t code = it->current;
	struct corpus_text_iter it2;

	if (ptr > begin + 12) {
		return 1;
	}

	if (ptr == begin) {
		return 0;
	}

	if (!(it->attr & CORPUS_TEXT_ESC_BIT)) {
		return (ptr != begin + UTF8LITE_UTF8_ENCODE_LEN(code));
	}

	it2 = *it;
	iter_retreat_escaped(&it2, begin);

	return (it2.ptr != begin);
}


int corpus_text_iter_retreat(struct corpus_text_iter *it)
{
	const size_t size = (it->text_attr & CORPUS_TEXT_SIZE_MASK);
	const uint8_t *begin = it->end - size;
	const uint8_t *ptr = it->ptr;
	const uint8_t *end = it->end;
	uint32_t code = it->current;

	if (ptr == begin) {
		return 0;
	}

	if (it->text_attr & CORPUS_TEXT_ESC_BIT) {
		iter_retreat_escaped(it, begin);
	} else {
		iter_retreat_raw(it);
	}

	// we were at the end of the text
	if (code == CORPUS_TEXT_CODE_NONE) {
		it->ptr = end;
		return 1;
	}

	// at this point, it->code == code, and it->ptr is the code start
	ptr = it->ptr;

	if (ptr == begin) {
		it->current = CORPUS_TEXT_CODE_NONE;
		it->attr = 0;
		return 0;
	}

	// read the previous code
	if (it->text_attr & CORPUS_TEXT_ESC_BIT) {
		iter_retreat_escaped(it, begin);
	} else {
		iter_retreat_raw(it);
	}

	// now, it->code is the previous code, and it->ptr is the start
	// of the previous code

	// set the pointer to the end of the previous code
	it->ptr = ptr;
	return 1;
}


void iter_retreat_raw(struct corpus_text_iter *it)
{
	const uint8_t *ptr = it->ptr;
	uint32_t code;

	code = *(--ptr);

	if (code < 0x80) {
		it->ptr = (uint8_t *)ptr;
		it->current = code;
		it->attr = 0;
	} else {
		// skip over continuation bytes
		do {
			ptr--;
		} while (*ptr < 0xC0);

		it->ptr = (uint8_t *)ptr;

		utf8lite_decode_utf8(&ptr, &it->current);
		it->attr = CORPUS_TEXT_UTF8_BIT;
	}
}


// we are at an escape if we are preceded by an odd number of
// backslash (\) characters
static int at_escape(const uint8_t *begin, const uint8_t *ptr)
{
	int at = 0;
	uint_fast8_t prev;

	while (begin < ptr) {
		prev = *(--ptr);

		if (prev != '\\') {
			goto out;
		}

		at = !at;
	}

out:
	return at;
}


void iter_retreat_escaped(struct corpus_text_iter *it, const uint8_t *begin)
{
	const uint8_t *ptr = it->ptr;
	uint32_t code, unesc, hi;
	size_t attr;
	int i;

	code = *(--ptr);
	attr = 0;

	// check for 2-byte escape
	switch (code) {
	case '"':
	case '\\':
	case '/':
		unesc = code;
		break;

	case 'b':
		unesc = '\b';
		break;

	case 'f':
		unesc = '\f';
		break;
	case 'n':
		unesc = '\n';
		break;

	case 'r':
		unesc = '\r';
		break;

	case 't':
		unesc = '\t';
		break;

	default:
		unesc = 0;
		break;
	}

	if (unesc) {
	       if (at_escape(begin, ptr)) {
		       ptr--;
		       code = unesc;
		       attr = CORPUS_TEXT_ESC_BIT;
	       }
	       goto out;
	}

	// check for 6-byte escape
	if (isxdigit((int)code)) {
		if (!(begin + 4 < ptr && ptr[-4] == 'u'
				&& at_escape(begin, ptr - 4))) {
			goto out;
		}
		attr = CORPUS_TEXT_ESC_BIT;

		code = 0;
		for (i = 0; i < 4; i++) {
			code = (code << 4) + hextoi(ptr[i - 3]);
		}
		ptr -= 5;

		if (code >= 0x80) {
			attr |= CORPUS_TEXT_UTF8_BIT;
		}

		if (UTF8LITE_IS_UTF16_LOW(code)) {
			hi = 0;
			for (i = 0; i < 4; i++) {
				hi = (hi << 4) + hextoi(ptr[i - 4]);
			}

			code = UTF8LITE_DECODE_UTF16_PAIR(hi, code);
			ptr -= 6;
		}

		goto out;
	}

	// check for ascii
	if (code < 0x80) {
		goto out;
	}

	// if we got here, then code is a continuation byte

	// skip over preceding continuation bytes
	do {
		ptr--;
	} while (*ptr < 0xC0);

	// decode the utf-8 value
	it->ptr = (uint8_t *)ptr;
	utf8lite_decode_utf8(&ptr, &it->current);
	it->attr = CORPUS_TEXT_UTF8_BIT;
	return;

out:
	it->ptr = (uint8_t *)ptr;
	it->current = code;
	it->attr = attr;
}




int assign_raw(struct corpus_text *text, const uint8_t *ptr, size_t size)
{
	const uint8_t *input = ptr;
	const uint8_t *end = ptr + size;
	size_t attr = 0;
	uint_fast8_t ch;
	int err;

	text->ptr = (uint8_t *)ptr;

	while (ptr != end) {
		ch = *ptr++;
		if (ch & 0x80) {
			attr |= CORPUS_TEXT_UTF8_BIT;
			ptr--;
			if ((err = utf8lite_scan_utf8(&ptr, end))) {
				goto error_inval_utf8;
			}
		}
	}

	// validate size
	size = (size_t)(ptr - text->ptr);
	if (size > CORPUS_TEXT_SIZE_MAX) {
		goto error_overflow;
	}

	attr |= (size & CORPUS_TEXT_SIZE_MASK);
	text->attr = attr;
	return 0;

error_inval_utf8:
	corpus_log(err, "invalid UTF-8 byte (0x%02X)", (unsigned)*ptr);
	goto error_inval;

error_inval:
	corpus_log(err, "error in text at byte %"PRIu64,
		   (uint64_t)(ptr - input));
	goto error;

error_overflow:
	err = CORPUS_ERROR_OVERFLOW;
	corpus_log(err, "text size (%"PRIu64" bytes)"
		   " exceeds maximum (%"PRIu64" bytes)",
		   (uint64_t)size, (uint64_t)CORPUS_TEXT_SIZE_MAX);
	goto error;

error:
	text->ptr = NULL;
	text->attr = 0;
	return err;
}


int assign_raw_unsafe(struct corpus_text *text, const uint8_t *ptr, size_t size)
{
	const uint8_t *end = ptr + size;
	size_t attr = 0;
	uint_fast8_t ch;
	int err;

	text->ptr = (uint8_t *)ptr;

	while (ptr != end) {
		ch = *ptr++;
		if (ch & 0x80) {
			attr |= CORPUS_TEXT_UTF8_BIT;
			ptr += UTF8LITE_UTF8_TAIL_LEN(ch);
		}
	}

	// validate size
	size = (size_t)(ptr - text->ptr);
	if (size > CORPUS_TEXT_SIZE_MAX) {
		goto error_overflow;
	}

	attr |= (size & CORPUS_TEXT_SIZE_MASK);
	text->attr = attr;
	return 0;

error_overflow:
	err = CORPUS_ERROR_OVERFLOW;
	corpus_log(err, "text size (%"PRIu64" bytes)"
		   " exceeds maximum (%"PRIu64" bytes)",
		   (uint64_t)size, (uint64_t)CORPUS_TEXT_SIZE_MAX);
	goto error;

error:
	text->ptr = NULL;
	text->attr = 0;
	return err;
}


int assign_esc(struct corpus_text *text, const uint8_t *ptr, size_t size)
{
	const uint8_t *input = ptr;
	const uint8_t *end = ptr + size;
	size_t attr = 0;
	uint32_t code;
	uint_fast8_t ch;
	int err;

	text->ptr = (uint8_t *)ptr;

	while (ptr != end) {
		ch = *ptr++;
		if (ch == '\\') {
			attr |= CORPUS_TEXT_ESC_BIT;

			if (ptr == end) {
				goto error_inval_incomplete;
			}
			ch = *ptr++;

			switch (ch) {
			case '"':
			case '\\':
			case '/':
			case 'b':
			case 'f':
			case 'n':
			case 'r':
			case 't':
				break;
			case 'u':
				if ((err = decode_uescape(&ptr, end, &code))) {
					goto error_inval_uesc;
				}
				if (code >= 0x80) {
					attr |= CORPUS_TEXT_UTF8_BIT;
				}
				break;
			default:
				goto error_inval_esc;
			}
		} else if (ch & 0x80) {
			attr |= CORPUS_TEXT_UTF8_BIT;
			ptr--;
			if ((err = utf8lite_scan_utf8(&ptr, end))) {
				goto error_inval_utf8;
			}
		}
	}

	// validate size
	size = (size_t)(ptr - text->ptr);
	if (size > CORPUS_TEXT_SIZE_MAX) {
		goto error_overflow;
	}

	attr |= (size & CORPUS_TEXT_SIZE_MASK);
	text->attr = attr;
	return 0;

error_inval_incomplete:
	err = CORPUS_ERROR_INVAL;
	corpus_log(err, "incomplete escape code (\\)");
	goto error_inval;

error_inval_esc:
	err = CORPUS_ERROR_INVAL;
	corpus_log(err, "invalid escape code (\\%c)", ch);
	goto error_inval;

error_inval_uesc:
	goto error_inval;

error_inval_utf8:
	corpus_log(err, "invalid UTF-8 byte (0x%02X)", (unsigned)*ptr);
	goto error_inval;

error_inval:
	corpus_log(err, "error in text at byte %"PRIu64,
		   (uint64_t)(ptr - input));
	goto error;

error_overflow:
	err = CORPUS_ERROR_OVERFLOW;
	corpus_log(err, "text size (%"PRIu64" bytes)"
		   " exceeds maximum (%"PRIu64" bytes)",
		   (uint64_t)size, (uint64_t)CORPUS_TEXT_SIZE_MAX);
	goto error;

error:
	text->ptr = NULL;
	text->attr = 0;
	return err;
}


int assign_esc_unsafe(struct corpus_text *text, const uint8_t *ptr, size_t size)
{
	const uint8_t *end = ptr + size;
	size_t attr = 0;
	uint32_t code;
	uint_fast8_t ch;
	int err;

	text->ptr = (uint8_t *)ptr;

	while (ptr != end) {
		ch = *ptr++;
		if (ch == '\\') {
			attr |= CORPUS_TEXT_ESC_BIT;
			ch = *ptr++;

			switch (ch) {
			case 'u':
				decode_valid_uescape(&ptr, &code);
				if (code >= 0x80) {
					attr |= CORPUS_TEXT_UTF8_BIT;
				}
				break;
			default:
				break;
			}
		} else if (ch & 0x80) {
			attr |= CORPUS_TEXT_UTF8_BIT;
			ptr += UTF8LITE_UTF8_TAIL_LEN(ch);
		}
	}

	// validate size
	size = (size_t)(ptr - text->ptr);
	if (size > CORPUS_TEXT_SIZE_MAX) {
		goto error_overflow;
	}

	attr |= (size & CORPUS_TEXT_SIZE_MASK);
	text->attr = attr;
	return 0;

error_overflow:
	err = CORPUS_ERROR_OVERFLOW;
	corpus_log(err, "text size (%"PRIu64" bytes)"
		   " exceeds maximum (%"PRIu64" bytes)",
		   (uint64_t)size, (uint64_t)CORPUS_TEXT_SIZE_MAX);
	goto error;

error:
	text->ptr = NULL;
	text->attr = 0;
	return err;
}


int decode_uescape(const uint8_t **inputptr, const uint8_t *end,
		   uint32_t *codeptr)
{
	const uint8_t *input = *inputptr;
	const uint8_t *ptr = input;
	uint32_t code, low;
	uint_fast8_t ch;
	unsigned i;
	int err;

	if (ptr + 4 > end) {
		goto error_inval_incomplete;
	}

	code = 0;
	for (i = 0; i < 4; i++) {
		ch = *ptr++;
		if (!isxdigit(ch)) {
			goto error_inval_hex;
		}
		code = (code << 4) + hextoi(ch);
	}

	if (UTF8LITE_IS_UTF16_HIGH(code)) {
		if (ptr + 6 > end || ptr[0] != '\\' || ptr[1] != 'u') {
			goto error_inval_nolow;
		}
		ptr += 2;
		input = ptr;

		low = 0;
		for (i = 0; i < 4; i++) {
			ch = *ptr++;
			if (!isxdigit(ch)) {
				goto error_inval_hex;
			}
			low = (low << 4) + hextoi(ch);
		}
		if (!UTF8LITE_IS_UTF16_LOW(low)) {
			ptr -= 6;
			goto error_inval_low;
		}
		code = UTF8LITE_DECODE_UTF16_PAIR(code, low);
	} else if (UTF8LITE_IS_UTF16_LOW(code)) {
		goto error_inval_nohigh;
	}

	err = 0;
	goto out;

error_inval_incomplete:
	err = CORPUS_ERROR_INVAL;
	corpus_log(err, "incomplete escape code (\\u%.*s)",
		   (int)(end - input), input);
	goto error_inval;

error_inval_hex:
	err = CORPUS_ERROR_INVAL;
	corpus_log(err, "invalid hex value in escape code (\\u%.*s)",
		   4, input);
	goto error_inval;

error_inval_nolow:
	err = CORPUS_ERROR_INVAL;
	corpus_log(err, "missing UTF-16 low surrogate"
		   " after high surrogate escape code (\\u%.*s)", 4, input);
	goto error_inval;

error_inval_low:
	err = CORPUS_ERROR_INVAL;
	corpus_log(err, "invalid UTF-16 low surrogate (\\u%.*s)"
		   " after high surrogate escape code (\\u%.*s)",
		   4, input, 4, input - 6);
	goto error_inval;

error_inval_nohigh:
	err = CORPUS_ERROR_INVAL;
	corpus_log(err, "missing UTF-16 high surrogate"
		   " before low surrogate escape code (\\u%.*s)", 4, input);
	goto error_inval;

error_inval:
	code = UTF8LITE_REPLACEMENT;

out:
	*codeptr = code;
	*inputptr = ptr;
	return err;
}


void decode_valid_uescape(const uint8_t **inputptr, uint32_t *codeptr)
{
	const uint8_t *ptr = *inputptr;
	uint32_t code;
	uint_fast16_t low;
	uint_fast8_t ch;
	unsigned i;

	code = 0;
	for (i = 0; i < 4; i++) {
		ch = *ptr++;
		code = (code << 4) + hextoi(ch);
	}

	if (UTF8LITE_IS_UTF16_HIGH(code)) {
		// skip over \u
		ptr += 2;

		low = 0;
		for (i = 0; i < 4; i++) {
			ch = *ptr++;
			low = (uint_fast16_t)(low << 4) + hextoi(ch);
		}

		code = UTF8LITE_DECODE_UTF16_PAIR(code, low);
	}

	*codeptr = code;
	*inputptr = ptr;
}


void decode_valid_escape(const uint8_t **inputptr, uint32_t *codeptr)
{
	const uint8_t *ptr = *inputptr;
	uint32_t code;

	code = *ptr++;

	switch (code) {
	case 'b':
		code = '\b';
		break;
	case 'f':
		code = '\f';
		break;
	case 'n':
		code = '\n';
		break;
	case 'r':
		code = '\r';
		break;
	case 't':
		code = '\t';
		break;
	case 'u':
		*inputptr = ptr;
		decode_valid_uescape(inputptr, codeptr);
		return;
	default:
		break;
	}

	*inputptr = ptr;
	*codeptr = code;
}


// Dan Bernstein's djb2 XOR hash: http://www.cse.yorku.ca/~oz/hash.html
unsigned corpus_text_hash(const struct corpus_text *text)
{
	const uint8_t *ptr = text->ptr;
	const uint8_t *end = ptr + CORPUS_TEXT_SIZE(text);
	unsigned hash = 5381;
	uint_fast8_t ch;

	while (ptr != end) {
		ch = *ptr++;
		hash = ((hash << 5) + hash) ^ ch;
	}

	return hash;
}


int corpus_text_equals(const struct corpus_text *text1,
		       const struct corpus_text *text2)
{
	return ((text1->attr & ~CORPUS_TEXT_UTF8_BIT)
			== (text2->attr & ~CORPUS_TEXT_UTF8_BIT)
		&& !memcmp(text1->ptr, text2->ptr, CORPUS_TEXT_SIZE(text2)));
}


int corpus_compare_text(const struct corpus_text *text1,
			const struct corpus_text *text2)
{
	size_t n1 = CORPUS_TEXT_SIZE(text1);
	size_t n2 = CORPUS_TEXT_SIZE(text2);
	size_t n = (n1 < n2) ? n1 : n2;
	return memcmp(text1->ptr, text2->ptr, n);
}
