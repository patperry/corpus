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
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../lib/utf8lite/src/utf8lite.h"
#include "array.h"
#include "error.h"
#include "memory.h"
#include "render.h"


static void corpus_render_grow(struct corpus_render *r, int nadd)
{
	void *base = r->string;
	int size = r->length_max + 1;
	int err;

	if (r->error) {
		return;
	}

	if (nadd <= 0 || r->length_max - nadd > r->length) {
		return;
	}

	if ((err = corpus_array_grow(&base, &size, sizeof(*r->string),
				     r->length + 1, nadd))) {
		r->error = err;
		return;
	}

	r->string = base;
	r->length_max = size - 1;
}


int corpus_render_init(struct corpus_render *r, int escape_flags)
{
	int err;

	r->string = corpus_malloc(1);
	if (!r->string) {
		err = CORPUS_ERROR_NOMEM;
		corpus_log(err, "failed initializing render object");
		return err;
	}

	r->length = 0;
	r->length_max = 0;
	r->escape_flags = escape_flags;

	r->tab = "\t";
	r->tab_length = (int)strlen(r->tab);

	r->newline = "\n";
	r->newline_length = (int)strlen(r->newline);

	corpus_render_clear(r);

	return 0;
}


void corpus_render_destroy(struct corpus_render *r)
{
	corpus_free(r->string);
}


void corpus_render_clear(struct corpus_render *r)
{
	r->string[0] = '\0';
	r->length = 0;
	r->indent = 0;
	r->needs_indent = 1;
	r->error = 0;
}


int corpus_render_set_escape(struct corpus_render *r, int flags)
{
	int oldflags = r->escape_flags;
	r->escape_flags = flags;
	return oldflags;
}


const char *corpus_render_set_tab(struct corpus_render *r, const char *tab)
{
	const char *oldtab = r->tab;
	size_t len;
	assert(tab);

	if ((len = strlen(tab)) >= INT_MAX) {
		r->error = CORPUS_ERROR_OVERFLOW;
		corpus_log(r->error, "tab string length exceeds maximum (%d)",
			   INT_MAX - 1);

	} else {
		r->tab = tab;
		r->tab_length = (int)len;
	}

	return oldtab;
}


const char *corpus_render_set_newline(struct corpus_render *r,
				      const char *newline)
{
	const char *oldnewline = r->newline;
	size_t len;
	assert(newline);

	if ((len = strlen(newline)) >= INT_MAX) {
		r->error = CORPUS_ERROR_OVERFLOW;
		corpus_log(r->error, "newline string length exceeds maximum (%d)", INT_MAX - 1);
	} else {
		r->newline = newline;
		r->newline_length = (int)len;
	}

	return oldnewline;
}


void corpus_render_indent(struct corpus_render *r, int nlevel)
{
	r->indent += nlevel;
	assert(r->indent >= 0);
}


void corpus_render_newlines(struct corpus_render *r, int nline)
{
	char *end;
	int i;

	if (r->error) {
		return;
	}

	for (i = 0; i < nline; i++) {
		corpus_render_grow(r, r->newline_length);
		if (r->error) {
			return;
		}

		end = r->string + r->length;
		memcpy(end, r->newline, r->newline_length + 1); // include '\0'
		r->length += r->newline_length;
		r->needs_indent = 1;
	}
}


static void maybe_indent(struct corpus_render *r)
{
	int ntab = r->indent;
	char *end;
	int i;

	if (r->error) {
		return;
	}

	if (!r->needs_indent) {
		return;
	}

	for (i = 0; i < ntab; i++) {
		corpus_render_grow(r, r->tab_length);
		if (r->error) {
			return;
		}

		end = r->string + r->length;
		memcpy(end, r->tab, r->tab_length + 1); // include '\0'
		r->length += r->tab_length;
	}

	r->needs_indent = 0;
}


void corpus_render_char(struct corpus_render *r, uint32_t ch)
{
	char *end;
	uint8_t *uend;
	unsigned lo, hi;

	if (r->error) {
		return;
	}

	maybe_indent(r);
	if (r->error) {
		return;
	}

	// maximum character expansion:
	// \uXXXX\uXXXX
	// 123456789012
	corpus_render_grow(r, 12);
	if (r->error) {
		return;
	}

	end = r->string + r->length;
	if (UTF8LITE_IS_ASCII(ch)) {
		if ((ch <= 0x1F || ch == 0x7F)
				&& (r->escape_flags & CORPUS_ESCAPE_CONTROL)) {
			switch (ch) {
			case '\b':
				end[0] = '\\'; end[1] = 'b'; end[2] = '\0';
				r->length += 2;
				break;
			case '\f':
				end[0] = '\\'; end[1] = 'f'; end[2] = '\0';
				r->length += 2;
				break;
			case '\n':
				end[0] = '\\'; end[1] = 'n'; end[2] = '\0';
				r->length += 2;
				break;
			case '\r':
				end[0] = '\\'; end[1] = 'r'; end[2] = '\0';
				r->length += 2;
				break;
			case '\t':
				end[0] = '\\'; end[1] = 't'; end[2] = '\0';
				r->length += 2;
				break;
			default:
				sprintf(end, "\\u%04x", ch);
				r->length += 6;
				break;
			}
		} else {
			end[0] = (char)ch;
			end[1] = '\0';
			r->length++;
		}
	} else if (ch <= 0x9F && (r->escape_flags & CORPUS_ESCAPE_CONTROL)) {
		sprintf(end, "\\u%04x", ch);
		r->length += 6;
	} else if (r->escape_flags & CORPUS_ESCAPE_UTF8) {
		if (UTF8LITE_UTF16_ENCODE_LEN(ch) == 1) {
			sprintf(end, "\\u%04x", ch);
			r->length += 6;
		} else {
			hi = UTF8LITE_UTF16_HIGH(ch);
			lo = UTF8LITE_UTF16_LOW(ch);
			sprintf(end, "\\u%04x\\u%04x", hi, lo);
			r->length += 12;
		}
	} else {
		uend = (uint8_t *)end;
		utf8lite_encode_utf8(ch, &uend);
		*uend = '\0';
		r->length += UTF8LITE_UTF8_ENCODE_LEN(ch);
	}
}


void corpus_render_string(struct corpus_render *r, const char *str)
{
	const uint8_t *ptr = (const uint8_t *)str;
	int32_t ch;

	if (r->error) {
		return;
	}

	while (1) {
		utf8lite_decode_utf8(&ptr, &ch);
		if (ch == 0) {
			return;
		}
		corpus_render_char(r, ch);
		if (r->error) {
			return;
		}
	}
}


void corpus_render_printf(struct corpus_render *r, const char *format, ...)
{
	va_list ap, ap2;
	int err, len;

	if (r->error) {
		return;
	}

	va_start(ap, format);
	va_copy(ap2, ap);

	len = vsnprintf(NULL, 0, format, ap);
	if (len < 0) {
		err = CORPUS_ERROR_OS;
		corpus_log(err, "printf formatting error: %s",
			   strerror(errno));
		r->error = err;
		goto out;
	}

	corpus_render_grow(r, len);
	if (r->error) {
		goto out;
	}

	vsprintf(r->string + r->length, format, ap2);
	r->length += len;

out:
	va_end(ap);
	va_end(ap2);
}


void corpus_render_text(struct corpus_render *r,
			const struct utf8lite_text *text)
{
	struct utf8lite_text_iter it;

	if (r->error) {
		return;
	}

	utf8lite_text_iter_make(&it, text);
	while (utf8lite_text_iter_advance(&it)) {
		corpus_render_char(r, it.current);
		if (r->error) {
			return;
		}
	}
}
