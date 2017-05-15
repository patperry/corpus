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
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include "../lib/libstemmer_c/include/libstemmer.h"
#include "private/stopwords.h"
#include "error.h"
#include "memory.h"
#include "text.h"
#include "unicode.h"
#include "token.h"


static void corpus_typemap_clear_kind(struct corpus_typemap *map);
static int corpus_typemap_set_kind(struct corpus_typemap *map, int kind);

static int corpus_typemap_reserve(struct corpus_typemap *map, size_t size);
static int corpus_typemap_set_ascii(struct corpus_typemap *map,
			     const struct corpus_text *tok);
static int corpus_typemap_set_utf32(struct corpus_typemap *map,
				    const uint32_t *ptr,
				    const uint32_t *end);
static int corpus_typemap_stem(struct corpus_typemap *map);


const char **corpus_stemmer_names(void)
{
	return sb_stemmer_list();
}


const uint8_t **corpus_stopwords(const char *name, int *lenptr)
{
	return stopwords(name, lenptr);
}


const char **corpus_stopword_names(void)
{
	return stopword_names();
}


int corpus_typemap_init(struct corpus_typemap *map, int kind,
			const char *stemmer)
{
	int err;

	if (stemmer) {
		errno = 0;
		map->stemmer = sb_stemmer_new(stemmer, "UTF_8");
		if (!map->stemmer) {
		       if (errno == ENOMEM) {
			       err = CORPUS_ERROR_NOMEM;
			       corpus_log(err, "failed allocating stemmer");
		       } else {
				err = CORPUS_ERROR_INVAL;
				corpus_log(err,
					   "unrecognized stemming algorithm"
					   " (%s)", stemmer);
		       }
		       goto out;
		}
	} else {
		map->stemmer = NULL;
	}

	map->type.ptr = NULL;
	map->codes = NULL;
	map->size_max = 0;

	corpus_typemap_clear_kind(map);
	err = corpus_typemap_set_kind(map, kind);
out:
	return err;
}


void corpus_typemap_destroy(struct corpus_typemap *map)
{
	corpus_free(map->codes);
	corpus_free(map->type.ptr);
	if (map->stemmer) {
		sb_stemmer_delete(map->stemmer);
	}
}


void corpus_typemap_clear_kind(struct corpus_typemap *map)
{
	uint_fast8_t ch;

	map->charmap_type = CORPUS_UDECOMP_NORMAL | CORPUS_UCASEFOLD_NONE;

	for (ch = 0; ch < 0x80; ch++) {
		map->ascii_map[ch] = (int8_t)ch;
	}

	map->kind = 0;
}


int corpus_typemap_set_kind(struct corpus_typemap *map, int kind)
{
	int_fast8_t ch;

	if (map->kind == kind) {
		return 0;
	}

	corpus_typemap_clear_kind(map);

	if (kind & CORPUS_TYPE_COMPAT) {
		map->charmap_type = CORPUS_UDECOMP_ALL;
	}

	if (kind & CORPUS_TYPE_CASEFOLD) {
		for (ch = 'A'; ch <= 'Z'; ch++) {
			map->ascii_map[ch] = ch + ('a' - 'A');
		}

		map->charmap_type |= CORPUS_UCASEFOLD_ALL;
	}

	if (kind & CORPUS_TYPE_QUOTFOLD) {
		map->ascii_map['"'] = '\'';
	}

	if (kind & CORPUS_TYPE_RMCC) {
		for (ch = 0x00; ch <= 0x08; ch++) {
			map->ascii_map[ch] = -1;
		}
		for (ch = 0x0E; ch <= 0x1F; ch++) {
			map->ascii_map[ch] = -1;
		}
		map->ascii_map[0x7F] = -1;
	}

	if (kind & CORPUS_TYPE_RMWS) {
		for (ch = 0x09; ch <= 0x0D; ch++) {
			map->ascii_map[ch] = -1;
		}
		map->ascii_map[' '] = -1;
	}

	map->kind = kind;

	return 0;
}


int corpus_typemap_reserve(struct corpus_typemap *map, size_t size)
{
	uint8_t *ptr = map->type.ptr;
	uint32_t *codes = map->codes;
	int err;

	if (map->size_max >= size) {
		return 0;
	}

	if (!(ptr = corpus_realloc(ptr, size))) {
		goto error_nomem;
	}
	map->type.ptr = ptr;

	if (!(codes = corpus_realloc(codes,
				     size * CORPUS_UNICODE_DECOMP_MAX))) {
		goto error_nomem;
	}
	map->codes = codes;

	map->size_max = size;
	return 0;

error_nomem:
	err = CORPUS_ERROR_NOMEM;
	corpus_log(err, "failed allocating type map buffer");
	return err;
}


int corpus_typemap_set(struct corpus_typemap *map,
		       const struct corpus_text *tok)
{
	struct corpus_text_iter it;
	size_t size = CORPUS_TEXT_SIZE(tok);
	uint32_t *dst;
	int err;

	if (CORPUS_TEXT_IS_ASCII(tok)) {
		if ((err = corpus_typemap_set_ascii(map, tok))) {
			goto error;
		}
		goto stem;
	}

	if ((err = corpus_typemap_reserve(map, size + 1))) {
		goto error;
	}

	dst = map->codes;
	corpus_text_iter_make(&it, tok);
	while (corpus_text_iter_advance(&it)) {
		corpus_unicode_map(map->charmap_type, it.current, &dst);
	}

	size = (size_t)(dst - map->codes);
	corpus_unicode_order(map->codes, size);
	corpus_unicode_compose(map->codes, &size);

	if ((err = corpus_typemap_set_utf32(map, map->codes,
					    map->codes + size))) {
		goto error;
	}

stem:
	err = corpus_typemap_stem(map);
	return err;

error:
	corpus_log(err, "failed normalizing token");
	return err;
}


int corpus_typemap_stem(struct corpus_typemap *map)
{
	size_t size;
	const uint8_t *buf;
	int err;

	if (!map->stemmer) {
		return 0;
	}

	size = CORPUS_TEXT_SIZE(&map->type);

	if (size >= INT_MAX) {
		err = CORPUS_ERROR_OVERFLOW;
		corpus_log(err, "type size (%"PRIu64" bytes)"
			   " exceeds maximum (%d)",
			   (uint64_t)size, INT_MAX - 1);
		goto out;
	}

	buf = (const uint8_t *)sb_stemmer_stem(map->stemmer, map->type.ptr,
					       (int)size);
	if (buf == NULL) {
		err = CORPUS_ERROR_NOMEM;
		corpus_log(err, "failed allocating memory to stem word"
			   " of size %"PRIu64" bytes", (uint64_t)size);
		goto out;
	}

	size = (size_t)sb_stemmer_length(map->stemmer);

	memcpy(map->type.ptr, buf, size);
	map->type.ptr[size] = '\0';

	// keep old utf8 bit, but update to new size
	map->type.attr &= ~CORPUS_TEXT_SIZE_MASK;
	map->type.attr |= CORPUS_TEXT_SIZE_MASK & size;
	err = 0;

out:
	return err;
}


int corpus_typemap_set_utf32(struct corpus_typemap *map, const uint32_t *ptr,
			     const uint32_t *end)
{
	bool fold_dash = map->kind & CORPUS_TYPE_DASHFOLD;
	bool fold_quot = map->kind & CORPUS_TYPE_QUOTFOLD;
	bool rm_cc = map->kind & CORPUS_TYPE_RMCC;
	bool rm_di = map->kind & CORPUS_TYPE_RMDI;
	bool rm_ws = map->kind & CORPUS_TYPE_RMWS;
	uint8_t *dst = map->type.ptr;
	uint32_t code;
	int8_t ch;
	bool utf8 = false;

	while (ptr != end) {
		code = *ptr++;

		if (code <= 0x7F) {
			ch = map->ascii_map[code];
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
		corpus_encode_utf8(code, &dst);
	}

	*dst = '\0'; // not necessary, but helps with debugging
	map->type.attr = (CORPUS_TEXT_SIZE_MASK
			  & ((size_t)(dst - map->type.ptr)));
	if (utf8) {
		map->type.attr |= CORPUS_TEXT_UTF8_BIT;
	}

	return 0;
}


int corpus_typemap_set_ascii(struct corpus_typemap *map,
			     const struct corpus_text *tok)
{
	struct corpus_text_iter it;
	size_t size = CORPUS_TEXT_SIZE(tok);
	int8_t ch;
	uint8_t *dst;
	int err;

	assert(CORPUS_TEXT_IS_ASCII(tok));

	if ((err = corpus_typemap_reserve(map, size + 1))) {
		goto error;
	}

	dst = map->type.ptr;

	corpus_text_iter_make(&it, tok);
	while (corpus_text_iter_advance(&it)) {
		ch = map->ascii_map[it.current];
		if (ch >= 0) {
			*dst++ = (uint8_t)ch;
		}
	}

	*dst = '\0'; // not necessary, but helps with debugging
	map->type.attr = (CORPUS_TEXT_SIZE_MASK
			  & ((size_t)(dst - map->type.ptr)));
	return 0;

error:
	corpus_log(err, "failed normalizing token");
	return err;
}


// Dan Bernstein's djb2 XOR hash: http://www.cse.yorku.ca/~oz/hash.html
unsigned corpus_token_hash(const struct corpus_text *tok)
{
	const uint8_t *ptr = tok->ptr;
	const uint8_t *end = ptr + CORPUS_TEXT_SIZE(tok);
	unsigned hash = 5381;
	uint_fast8_t ch;

	while (ptr != end) {
		ch = *ptr++;
		hash = ((hash << 5) + hash) ^ ch;
	}

	return hash;
}


int corpus_token_equals(const struct corpus_text *t1,
			const struct corpus_text *t2)
{
	return ((t1->attr & ~CORPUS_TEXT_UTF8_BIT)
			== (t2->attr & ~CORPUS_TEXT_UTF8_BIT)
		&& !memcmp(t1->ptr, t2->ptr, CORPUS_TEXT_SIZE(t2)));
}


int corpus_compare_type(const struct corpus_text *typ1,
			const struct corpus_text *typ2)
{
	size_t n1 = CORPUS_TEXT_SIZE(typ1);
	size_t n2 = CORPUS_TEXT_SIZE(typ2);
	size_t n = (n1 < n2) ? n1 : n2;
	return memcmp(typ1->ptr, typ2->ptr, n);
}
