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
#include "error.h"
#include "memory.h"
#include "unicode.h"
#include "text.h"

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


int corpus_text_init_copy(struct corpus_text *text,
			  const struct corpus_text *other)
{
        size_t size = CORPUS_TEXT_SIZE(other);
        size_t attr = other->attr;
	int err;

        if (!(text->ptr = corpus_malloc(size + 1))) {
                err = ERROR_NOMEM;
                logmsg(err, "failed allocating text object");
                return err;
        }

        memcpy(text->ptr, other->ptr, size);
        text->ptr[size] = '\0';
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
	it->current = (uint32_t)-1;
}


int corpus_text_iter_advance(struct corpus_text_iter *it)
{
	const uint8_t *ptr = it->ptr;
	size_t text_attr = it->text_attr;
	uint32_t code;
	size_t attr;


	if (ptr == it->end) {
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
	} else if ((text_attr & CORPUS_TEXT_UTF8_BIT) && code >= 0x80) {
		attr = CORPUS_TEXT_UTF8_BIT;
		ptr--;
		decode_utf8(&ptr, &code);
	}

	it->ptr = ptr;
	it->current = code;
	it->attr = attr;
	return 1;

at_end:
	it->current = 0;
	it->attr = 0;
	return 0;
}


void corpus_text_iter_reset(struct corpus_text_iter *it)
{
	it->ptr = it->end - (it->text_attr & CORPUS_TEXT_SIZE_MASK);
	it->current = (uint32_t)-1;
	it->attr = 0;
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
			if ((err = scan_utf8(&ptr, end))) {
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
	logmsg(err, "invalid UTF-8 byte (0x%02X)", (unsigned)*ptr);
	goto error_inval;

error_inval:
	logmsg(err, "error in text at byte %"PRIu64, (uint64_t)(ptr - input));
	goto error;

error_overflow:
	err = ERROR_OVERFLOW;
	logmsg(err, "text size (%"PRIu64" bytes)"
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
			ptr += UTF8_TAIL_LEN(ch);
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
	err = ERROR_OVERFLOW;
	logmsg(err, "text size (%"PRIu64" bytes)"
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
			if ((err = scan_utf8(&ptr, end))) {
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
	err = ERROR_INVAL;
	logmsg(err, "incomplete escape code (\\)");
	goto error_inval;

error_inval_esc:
	err = ERROR_INVAL;
	logmsg(err, "invalid escape code (\\%c)", ch);
	goto error_inval;

error_inval_uesc:
	goto error_inval;

error_inval_utf8:
	logmsg(err, "invalid UTF-8 byte (0x%02X)", (unsigned)*ptr);
	goto error_inval;

error_inval:
	logmsg(err, "error in text at byte %"PRIu64,
		(uint64_t)(ptr - input));
	goto error;

error_overflow:
	err = ERROR_OVERFLOW;
	logmsg(err, "text size (%"PRIu64" bytes)"
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
			ptr += UTF8_TAIL_LEN(ch);
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
	err = ERROR_OVERFLOW;
	logmsg(err, "text size (%"PRIu64" bytes)"
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

	if (IS_UTF16_HIGH(code)) {
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
		if (!IS_UTF16_LOW(low)) {
			ptr -= 6;
			goto error_inval_low;
		}
		code = DECODE_UTF16_PAIR(code, low);
	} else if (IS_UTF16_LOW(code)) {
		goto error_inval_nohigh;
	}

	err = 0;
	goto out;

error_inval_incomplete:
	err = ERROR_INVAL;
	logmsg(err, "incomplete escape code (\\u%.*s)",
	       (int)(end - input), input);
	goto error_inval;

error_inval_hex:
	err = ERROR_INVAL;
	logmsg(err, "invalid hex value in escape code (\\u%.*s)", 4, input);
	goto error_inval;

error_inval_nolow:
	err = ERROR_INVAL;
	logmsg(err,
	       "missing UTF-16 low surrogate after high surrogate escape code"
	       " (\\u%.*s)", 4, input);
	goto error_inval;

error_inval_low:
	err = ERROR_INVAL;
	logmsg(err,
		"invalid UTF-16 low surrogate (\\u%.*s)"
	        " after high surrogate escape code (\\u%.*s)",
		4, input, 4, input - 6);
	goto error_inval;

error_inval_nohigh:
	err = ERROR_INVAL;
	logmsg(err,
	       "missing UTF-16 high surrogate before low surrogate escape code"
	       " (\\u%.*s)", 4, input);
	goto error_inval;

error_inval:
	code = UNICODE_REPLACEMENT;

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

	if (IS_UTF16_HIGH(code)) {
		// skip over \u
		ptr += 2;

		low = 0;
		for (i = 0; i < 4; i++) {
			ch = *ptr++;
			low = (uint_fast16_t)(low << 4) + hextoi(ch);
		}

		code = DECODE_UTF16_PAIR(code, low);
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
