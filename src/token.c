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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "errcode.h"
#include "text.h"
#include "unicode.h"
#include "xalloc.h"
#include "token.h"


static void typebuf_clear_kind(struct typebuf *buf);
static int typebuf_set_kind(struct typebuf *buf, int kind);

static int typebuf_reserve(struct typebuf *buf, size_t size);
static int typebuf_set_ascii(struct typebuf *buf, const struct text *tok);
static int typebuf_set_utf32(struct typebuf *buf, const uint32_t *ptr,
			    const uint32_t *end);

int typebuf_init(struct typebuf *buf, int kind)
{
	int err;

	buf->text.ptr = NULL;
	buf->code = NULL;
	buf->size_max = 0;

	typebuf_clear_kind(buf);
	err = typebuf_set_kind(buf, kind);

	return err;
}


void typebuf_destroy(struct typebuf *buf)
{
	free(buf->code);
	free(buf->text.ptr);
}


void typebuf_clear_kind(struct typebuf *buf)
{
	uint_fast8_t ch;

	buf->map_type = UDECOMP_NORMAL | UCASEFOLD_NONE;

	for (ch = 0; ch < 0x80; ch++) {
		buf->ascii_map[ch] = ch;
	}

	buf->kind = 0;
}


int typebuf_set_kind(struct typebuf *buf, int kind)
{
	int_fast8_t ch;

	if (buf->kind == kind) {
		return 0;
	}

	typebuf_clear_kind(buf);

	if (kind & TYPE_COMPAT) {
		buf->map_type = UDECOMP_ALL;
	}

	if (kind & TYPE_CASEFOLD) {
		for (ch = 'A'; ch <= 'Z'; ch++) {
			buf->ascii_map[ch] = ch + ('a' - 'A');
		}

		buf->map_type |= UCASEFOLD_ALL;
	}

	if (kind & TYPE_QUOTFOLD) {
		buf->ascii_map['"'] = '\'';
	}

	if (kind & TYPE_RMCC) {
		for (ch = 0x00; ch <= 0x08; ch++) {
			buf->ascii_map[ch] = -1;
		}
		for (ch = 0x0E; ch <= 0x1F; ch++) {
			buf->ascii_map[ch] = -1;
		}
		buf->ascii_map[0x7F] = -1;
	}

	if (kind & TYPE_RMWS) {
		for (ch = 0x09; ch <= 0x0D; ch++) {
			buf->ascii_map[ch] = -1;
		}
		buf->ascii_map[' '] = -1;
	}

	buf->kind = kind;

	return 0;
}


int typebuf_reserve(struct typebuf *buf, size_t size)
{
	uint8_t *ptr = buf->text.ptr;
	uint32_t *code = buf->code;

	if (buf->size_max >= size) {
		return 0;
	}

	if (!(ptr = xrealloc(ptr, size))) {
		goto error_nomem;
	}
	buf->text.ptr = ptr;

	if (!(code = xrealloc(code, size * UNICODE_DECOMP_MAX))) {
		goto error_nomem;
	}
	buf->code = code;

	buf->size_max = size;
	return 0;

error_nomem:
	syslog(LOG_ERR, "failed allocating buffer");
	return ERROR_NOMEM;
}



int typebuf_set(struct typebuf *buf, const struct text *tok)
{
	struct text_iter it;
	size_t size = TEXT_SIZE(tok);
	uint32_t *dst;
	int err;

	if (TEXT_IS_ASCII(tok)) {
		err = typebuf_set_ascii(buf, tok);
		return err;
	}

	if ((err = typebuf_reserve(buf, size + 1))) {
		goto error;
	}

	dst = buf->code;
	text_iter_make(&it, tok);
	while (text_iter_advance(&it)) {
		unicode_map(buf->map_type, it.current, &dst);
	}
	unicode_order(buf->code, dst - buf->code);

	err = typebuf_set_utf32(buf, buf->code, dst);
	return err;

error:
	syslog(LOG_ERR, "failed normalizing token");
	return err;
}


int typebuf_set_utf32(struct typebuf *buf, const uint32_t *ptr,
		     const uint32_t *end)
{
	bool fold_dash = buf->kind & TYPE_DASHFOLD;
	bool fold_quot = buf->kind & TYPE_QUOTFOLD;
	bool rm_cc = buf->kind & TYPE_RMCC;
	bool rm_di = buf->kind & TYPE_RMDI;
	bool rm_ws = buf->kind & TYPE_RMWS;
	uint8_t *dst = buf->text.ptr;
	uint32_t code;
	int8_t ch;
	bool utf8 = false;

	while (ptr != end) {
		code = *ptr++;

		if (code <= 0x7F) {
			ch = buf->ascii_map[code];
			if (ch >= 0) {
				*dst++ = (uint8_t)ch;
			}
			continue;
		} else if (code <= 0x9F) {
			if (code == 0x85) {
				if (rm_ws) {
					continue;
				}
			} else {
				// remove C1 control chars
				if (rm_cc) {
					continue;
				}
			}
		} else if (code <= 0xDFFFF) {
			switch (code) {
			// Dash = Yes
			case 0x058A:
			case 0x05BE:
			case 0x1400:
			case 0x1806:
			case 0x2010:
			case 0x2011:
			case 0x2012:
			case 0x2013:
			case 0x2014:
			case 0x2015:
			case 0x2053:
			case 0x207B:
			case 0x208B:
			case 0x2212:
			case 0x2E17:
			case 0x2E1A:
			case 0x2E3A:
			case 0x2E3B:
			case 0x2E40:
			case 0x301C:
			case 0x3030:
			case 0x30A0:
			case 0xFE31:
			case 0xFE32:
			case 0xFE58:
			case 0xFE63:
			case 0xFF0D:
				if (fold_dash) {
					code = 0x002D;
				}
				break;

			// Quotation_Mark = Yes
			case 0x00AB:
			case 0x00BB:
			case 0x2018:
			case 0x2019:
			case 0x201A:
			case 0x201B:
			case 0x201C:
			case 0x201D:
			case 0x201E:
			case 0x201F:
			case 0x2039:
			case 0x203A:
			case 0x2E42:
			case 0x300C:
			case 0x300D:
			case 0x300E:
			case 0x300F:
			case 0x301D:
			case 0x301E:
			case 0x301F:
			case 0xFE41:
			case 0xFE42:
			case 0xFE43:
			case 0xFE44:
			case 0xFF02:
			case 0xFF07:
			case 0xFF62:
			case 0xFF63:
				if (fold_quot) {
					code = 0x0027;
				}
				break;

			// White_Space = Yes
			//case 0x0085: handled above
			case 0x00A0:
			case 0x1680:
			case 0x2000:
			case 0x2001:
			case 0x2002:
			case 0x2003:
			case 0x2004:
			case 0x2005:
			case 0x2006:
			case 0x2007:
			case 0x2008:
			case 0x2009:
			case 0x200A:
			case 0x2028:
			case 0x2029:
			case 0x202F:
			case 0x205F:
			case 0x3000:
				if (rm_ws) {
					continue;
				}
				break;

			// Default_Ignorable_Code_Point = Yes
			case 0x00AD:
			case 0x034F:
			case 0x061C:
			case 0x115F:
			case 0x1160:
			case 0x17B4:
			case 0x17B5:
			case 0x180B:
			case 0x180C:
			case 0x180D:
			case 0x180E:
			case 0x200B:
			case 0x200C:
			case 0x200D:
			case 0x200E:
			case 0x200F:
			case 0x202A:
			case 0x202B:
			case 0x202C:
			case 0x202D:
			case 0x202E:
			case 0x2060:
			case 0x2061:
			case 0x2062:
			case 0x2063:
			case 0x2064:
			case 0x2065:
			case 0x2066:
			case 0x2067:
			case 0x2068:
			case 0x2069:
			case 0x206A:
			case 0x206B:
			case 0x206C:
			case 0x206D:
			case 0x206E:
			case 0x206F:
			case 0x3164:
			case 0xFE00:
			case 0xFE01:
			case 0xFE02:
			case 0xFE03:
			case 0xFE04:
			case 0xFE05:
			case 0xFE06:
			case 0xFE07:
			case 0xFE08:
			case 0xFE09:
			case 0xFE0A:
			case 0xFE0B:
			case 0xFE0C:
			case 0xFE0D:
			case 0xFE0E:
			case 0xFE0F:
			case 0xFEFF:
			case 0xFFA0:
			case 0xFFF0:
			case 0xFFF1:
			case 0xFFF2:
			case 0xFFF3:
			case 0xFFF4:
			case 0xFFF5:
			case 0xFFF6:
			case 0xFFF7:
			case 0xFFF8:
			case 0x1BCA0:
			case 0x1BCA1:
			case 0x1BCA2:
			case 0x1BCA3:
			case 0x1D173:
			case 0x1D174:
			case 0x1D175:
			case 0x1D176:
			case 0x1D177:
			case 0x1D178:
			case 0x1D179:
			case 0x1D17A:
				if (rm_di) {
					continue;
				}
				break;

			default:
				break;
			}
		} else if (code <= 0xE0FFF) {
			if (rm_di) {
				continue;
			}
		}
		if (code >= 0x80) {
			utf8 = true;
		}
		encode_utf8(code, &dst);
	}

	*dst = '\0'; // not necessary, but helps with debugging
	buf->text.attr = TEXT_SIZE_MASK & (dst - buf->text.ptr);
	if (utf8) {
		buf->text.attr |= TEXT_UTF8_BIT;
	}

	return 0;
}


int typebuf_set_ascii(struct typebuf *buf, const struct text *tok)
{
	struct text_iter it;
	size_t size = TEXT_SIZE(tok);
	int8_t ch;
	uint8_t *dst;
	int err;

	assert(TEXT_IS_ASCII(tok));

	if ((err = typebuf_reserve(buf, size + 1))) {
		goto error;
	}

	dst = buf->text.ptr;

	text_iter_make(&it, tok);
	while (text_iter_advance(&it)) {
		ch = buf->ascii_map[it.current];
		if (ch >= 0) {
			*dst++ = (uint8_t)ch;
		}
	}

	*dst = '\0'; // not necessary, but helps with debugging
	buf->text.attr = TEXT_SIZE_MASK & (dst - buf->text.ptr);
	return 0;

error:
	syslog(LOG_ERR, "failed normalizing token");
	return err;
}


// Dan Bernstein's djb2 XOR hash: http://www.cse.yorku.ca/~oz/hash.html
uint64_t tok_hash(const struct text *tok)
{
	const uint8_t *ptr = tok->ptr;
	const uint8_t *end = ptr + TEXT_SIZE(tok);
	uint64_t hash = 5381;
	uint_fast8_t ch;

	while (ptr != end) {
		ch = *ptr++;
		hash = ((hash << 5) + hash) ^ ch;
	}

	return hash;
}


int tok_equals(const struct text *t1, const struct text *t2)
{
	return ((t1->attr & ~TEXT_UTF8_BIT) == (t2->attr & ~TEXT_UTF8_BIT)
			&& !memcmp(t1->ptr, t2->ptr, TEXT_SIZE(t2)));
}


int compare_typ(const struct text *typ1, const struct text *typ2)
{
	size_t n1 = TEXT_SIZE(typ1);
	size_t n2 = TEXT_SIZE(typ2);
	size_t n = (n1 < n2) ? n1 : n2;
	return memcmp(typ1->ptr, typ2->ptr, n);
}
