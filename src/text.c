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
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "errcode.h"
#include "unicode.h"
#include "xalloc.h"
#include "text.h"

/* http://stackoverflow.com/a/11986885 */
#define hextoi(ch) ((ch > '9') ? (ch &~ 0x20) - 'A' + 10 : (ch - '0'))

static int assign_esc(struct text *text, const uint8_t *ptr, size_t size);
static int assign_esc_unsafe(struct text *text, const uint8_t *ptr, size_t size);
static int assign_raw(struct text *text, const uint8_t *ptr, size_t size);
static int assign_raw_unsafe(struct text *text, const uint8_t *ptr, size_t size);

static int decode_uescape(const uint8_t **inputptr, const uint8_t *end,
			  uint32_t *codeptr);
static void decode_valid_escape(const uint8_t **inputptr, uint32_t *codeptr);
static void decode_valid_uescape(const uint8_t **inputptr, uint32_t *codeptr);


int text_init_copy(struct text *text, const struct text *other)
{
        size_t size = TEXT_SIZE(other);
        size_t attr = other->attr;

        if (!(text->ptr = xmalloc(size + 1))) {
                syslog(LOG_ERR, "failed allocating text object");
                return ERROR_NOMEM;
        }

        memcpy(text->ptr, other->ptr, size);
        text->ptr[size] = '\0';
        text->attr = attr;
        return 0;
}


void text_destroy(struct text *text)
{
        free(text->ptr);
}


int text_assign(struct text *text, const uint8_t *ptr, size_t size, int flags)
{
	int err;

	if (flags & TEXT_NOESCAPE) {
		if (flags & TEXT_NOVALIDATE) {
			err = assign_raw_unsafe(text, ptr, size);
		} else {
			err = assign_raw(text, ptr, size);
		}
	} else {
		if (flags & TEXT_NOVALIDATE) {
			err = assign_esc_unsafe(text, ptr, size);
		} else {
			err = assign_esc(text, ptr, size);
		}
	}

	return err;
}



void text_iter_make(struct text_iter *it, const struct text *text)
{
	it->ptr = text->ptr;
	it->end = it->ptr + TEXT_SIZE(text);
	it->attr = text->attr;
	it->current = -1;
}


int text_iter_advance(struct text_iter *it)
{
	const uint8_t *ptr = it->ptr;
	size_t attr = it->attr;
	uint32_t code;

	if (ptr == it->end) {
		goto at_end;
	}

	code = *ptr++;
	if (code == '\\' && (attr & TEXT_ESC_BIT)) {
		decode_valid_escape(&ptr, &code);
	} else if ((attr & TEXT_UTF8_BIT) && code >= 0x80) {
		ptr--;
		decode_utf8(&ptr, &code);
	}

	it->ptr = (uint8_t *)ptr;
	it->current = code;

	return 1;

at_end:
	return 0;
}


void text_iter_reset(struct text_iter *it)
{
	it->ptr = it->end - (it->attr & TEXT_SIZE_MASK);
	it->current = -1;
}


int assign_raw(struct text *text, const uint8_t *ptr, size_t size)
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
			attr |= TEXT_UTF8_BIT;
			ptr--;
			if ((err = scan_utf8(&ptr, end))) {
				goto error_inval_utf8;
			}
		}
	}

	// validate size
	size = ptr - text->ptr;
	if (size > TEXT_SIZE_MAX) {
		goto error_overflow;
	}

	attr |= (size & TEXT_SIZE_MASK);
	text->attr = attr;
	return 0;

error_inval_utf8:
	syslog(LOG_ERR, "invalid UTF-8 byte (0x%02X)", (unsigned)*ptr);
	goto error_inval;

error_inval:
	syslog(LOG_ERR, "error in text at byte %zu", ptr - input);
	err = ERROR_INVAL;
	goto error;

error_overflow:
	syslog(LOG_ERR, "text size (%zu bytes) exceeds maximum (%zu bytes)",
	       size, TEXT_SIZE_MAX);
	err = ERROR_OVERFLOW;
	goto error;

error:
	text->ptr = NULL;
	text->attr = 0;
	return err;
}


int assign_raw_unsafe(struct text *text, const uint8_t *ptr, size_t size)
{
	const uint8_t *end = ptr + size;
	size_t attr = 0;
	uint_fast8_t ch;
	int err;

	text->ptr = (uint8_t *)ptr;

	while (ptr != end) {
		ch = *ptr++;
		if (ch & 0x80) {
			attr |= TEXT_UTF8_BIT;
			ptr += UTF8_TAIL_LEN(ch);
		}
	}

	// validate size
	size = ptr - text->ptr;
	if (size > TEXT_SIZE_MAX) {
		goto error_overflow;
	}

	attr |= (size & TEXT_SIZE_MASK);
	text->attr = attr;
	return 0;

error_overflow:
	syslog(LOG_ERR, "text size (%zu bytes) exceeds maximum (%zu bytes)",
	       size, TEXT_SIZE_MAX);
	err = ERROR_OVERFLOW;
	goto error;

error:
	text->ptr = NULL;
	text->attr = 0;
	return err;
}


int assign_esc(struct text *text, const uint8_t *ptr, size_t size)
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
			attr |= TEXT_ESC_BIT;

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
					attr |= TEXT_UTF8_BIT;
				}
				break;
			default:
				goto error_inval_esc;
			}
		} else if (ch & 0x80) {
			attr |= TEXT_UTF8_BIT;
			ptr--;
			if ((err = scan_utf8(&ptr, end))) {
				goto error_inval_utf8;
			}
		}
	}

	// validate size
	size = ptr - text->ptr;
	if (size > TEXT_SIZE_MAX) {
		goto error_overflow;
	}

	attr |= (size & TEXT_SIZE_MASK);
	text->attr = attr;
	return 0;

error_inval_incomplete:
	syslog(LOG_ERR, "incomplete escape code (\\)");
	goto error_inval;

error_inval_esc:
	syslog(LOG_ERR, "invalid escape code (\\%c)", ch);
	goto error_inval;

error_inval_uesc:
	goto error_inval;

error_inval_utf8:
	syslog(LOG_ERR, "invalid UTF-8 byte (0x%02X)", (unsigned)*ptr);
	goto error_inval;

error_inval:
	syslog(LOG_ERR, "error in text at byte %zu", ptr - input);
	err = ERROR_INVAL;
	goto error;

error_overflow:
	syslog(LOG_ERR, "text size (%zu bytes) exceeds maximum (%zu bytes)",
	       size, TEXT_SIZE_MAX);
	err = ERROR_OVERFLOW;
	goto error;

error:
	text->ptr = NULL;
	text->attr = 0;
	return err;
}


int assign_esc_unsafe(struct text *text, const uint8_t *ptr, size_t size)
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
			attr |= TEXT_ESC_BIT;
			ch = *ptr++;

			switch (ch) {
			case 'u':
				decode_valid_uescape(&ptr, &code);
				if (code >= 0x80) {
					attr |= TEXT_UTF8_BIT;
				}
				break;
			default:
				break;
			}
		} else if (ch & 0x80) {
			attr |= TEXT_UTF8_BIT;
			ptr += UTF8_TAIL_LEN(ch);
		}
	}

	// validate size
	size = ptr - text->ptr;
	if (size > TEXT_SIZE_MAX) {
		goto error_overflow;
	}

	attr |= (size & TEXT_SIZE_MASK);
	text->attr = attr;
	return 0;

error_overflow:
	syslog(LOG_ERR, "text size (%zu bytes) exceeds maximum (%zu bytes)",
	       size, TEXT_SIZE_MAX);
	err = ERROR_OVERFLOW;
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
	syslog(LOG_ERR, "incomplete escape code (\\u%.*s)",
	       (int)(end - input), input);
	goto error_inval;

error_inval_hex:
	syslog(LOG_ERR, "invalid hex value in escape code (\\u%.*s)", 4, input);
	goto error_inval;

error_inval_nolow:
	syslog(LOG_ERR,
	       "missing UTF-16 low surrogate after high surrogate escape code"
	       " (\\u%.*s)", 4, input);
	goto error_inval;

error_inval_low:
	syslog(LOG_ERR,
		"invalid UTF-16 low surrogate (\\u%.*s)"
	        " after high surrogate escape code (\\u%.*s)",
		4, input, 4, input - 6);
	goto error_inval;

error_inval_nohigh:
	syslog(LOG_ERR,
	       "missing UTF-16 high surrogate before low surrogate escape code"
	       " (\\u%.*s)", 4, input);
	goto error_inval;

error_inval:
	code = UNICODE_REPLACEMENT;
	err = ERROR_INVAL;

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
			low = (low << 4) + hextoi(ch);
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
