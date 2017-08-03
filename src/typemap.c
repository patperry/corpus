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
#include <string.h>
#include "../lib/libstemmer_c/include/libstemmer.h"
#include "private/stopwords.h"
#include "error.h"
#include "memory.h"
#include "table.h"
#include "text.h"
#include "textset.h"
#include "unicode.h"
#include "wordscan.h"
#include "typemap.h"


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


const uint8_t **corpus_stopword_list(const char *name, int *lenptr)
{
	return stopword_list(name, lenptr);
}


const char **corpus_stopword_names(void)
{
	return stopword_names();
}


int corpus_typemap_init(struct corpus_typemap *map, int kind,
			const char *stemmer)
{
	int err;

	if ((err = corpus_textset_init(&map->excepts))) {
		corpus_log(err, "failed initializing stem exception set");
		goto out;
	}

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
	corpus_textset_destroy(&map->excepts);
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

	if (kind & CORPUS_TYPE_MAPCASE) {
		for (ch = 'A'; ch <= 'Z'; ch++) {
			map->ascii_map[ch] = ch + ('a' - 'A');
		}

		map->charmap_type |= CORPUS_UCASEFOLD_ALL;
	}

	if (kind & CORPUS_TYPE_MAPCOMPAT) {
		map->charmap_type = CORPUS_UDECOMP_ALL;
	}

	if (kind & CORPUS_TYPE_MAPQUOTE) {
		map->ascii_map['"'] = '\'';
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


int corpus_typemap_stem_except(struct corpus_typemap *map,
			       const struct corpus_text *typ)
{
	int err;

	if ((err = corpus_textset_add(&map->excepts, typ, NULL))) {
		corpus_log(err, "failed adding type to stem exception set");
	}

	return err;
}

static int count_words(const struct corpus_text *text, int *kind_ptr)
{
	struct corpus_wordscan scan;
	int nword, kind;

	nword = 0;
	kind = CORPUS_WORD_NONE;
	corpus_wordscan_make(&scan, text);
	while (corpus_wordscan_advance(&scan)) {
		if (nword == 0) {
			kind = scan.type;
		}
		nword++;
	}
	*kind_ptr = kind;
	return nword;
}


int corpus_typemap_stem(struct corpus_typemap *map)
{
	struct corpus_text stem;
	size_t size;
	const uint8_t *buf;
	int err, kind0, kind, nword0, nword;

	if (!map->stemmer) {
		return 0;
	}

	nword0 = count_words(&map->type, &kind0);
	if (kind0 != CORPUS_WORD_LETTER) {
		return 0;
	}

	if (corpus_textset_has(&map->excepts, &map->type, NULL)) {
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

	// keep old utf8 bit, but update to new size
	size = (size_t)sb_stemmer_length(map->stemmer);
	stem.ptr = (uint8_t *)buf;
	stem.attr = (map->type.attr & ~CORPUS_TEXT_SIZE_MASK) | size;
	nword = count_words(&stem, &kind);

	// only stem types if the number of words doesn't change; this
	// protects against turning inner punctuation like 'u.s' to
	// outer punctuation like 'u.'
	if (nword == nword0) {
		memcpy(map->type.ptr, buf, size);
		map->type.ptr[size] = '\0';
		map->type.attr = stem.attr;
	}
	err = 0;

out:
	return err;
}


int corpus_typemap_set_utf32(struct corpus_typemap *map, const uint32_t *ptr,
			     const uint32_t *end)
{
	int map_quote = map->kind & CORPUS_TYPE_MAPQUOTE;
	int rm_di = map->kind & CORPUS_TYPE_RMDI;
	uint8_t *dst = map->type.ptr;
	uint32_t code;
	int8_t ch;
	int is_utf8 = 0;

	while (ptr != end) {
		code = *ptr++;

		if (code <= 0x7F) {
			ch = map->ascii_map[code];
			if (ch >= 0) {
				*dst++ = (uint8_t)ch;
			}
			continue;
		} else if (code <= 0xDFFFF) {
			switch (code) {
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
				if (map_quote) {
					code = 0x0027;
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
			is_utf8 = 1;
		}
		corpus_encode_utf8(code, &dst);
	}

	*dst = '\0'; // not necessary, but helps with debugging
	map->type.attr = (CORPUS_TEXT_SIZE_MASK
			  & ((size_t)(dst - map->type.ptr)));
	if (is_utf8) {
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
