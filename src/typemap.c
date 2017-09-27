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
#include <stdint.h>
#include <string.h>
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


const uint8_t **corpus_stopword_list(const char *name, int *lenptr)
{
	return stopword_list(name, lenptr);
}


const char **corpus_stopword_names(void)
{
	return stopword_names();
}


int corpus_typemap_init(struct corpus_typemap *map, int kind)
{
	int err;

	map->type.ptr = NULL;
	map->type.attr = 0;
	map->codes = NULL;
	map->size_max = 0;

	corpus_typemap_clear_kind(map);
	err = corpus_typemap_set_kind(map, kind);
	return err;
}


void corpus_typemap_destroy(struct corpus_typemap *map)
{
	corpus_free(map->codes);
	corpus_free(map->type.ptr);
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
		return corpus_typemap_set_ascii(map, tok);
	}

	// For most inputs, mapping to type reduces or preserves the size.
	// However, for U+0390 and U+03B0, case folding triples the size.
	// (You can verify this with util/compute-typelen.py)
	//
	// Add one for a trailing NUL.
	if (size > (SIZE_MAX - 1) / 3) {
		err = CORPUS_ERROR_OVERFLOW;
		corpus_log(err, "text size (%"PRIu64" bytes)"
			   " exceeds maximum normalization size"
			   " (%"PRIu64" bytes)",
			   (uint64_t)size,
			   (uint64_t)((SIZE_MAX - 1) / 3));
		goto error;
	}

	if ((err = corpus_typemap_reserve(map, 3 * size + 1))) {
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

	return err;

error:
	corpus_log(err, "failed normalizing token");
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
		} else {
			switch (code) {
			case 0x055A: // ARMENIAN APOSTROPHE
			case 0xFE10: // PRESENTATION FORM FOR VERTICAL COMMA
			case 0x2018: // LEFT SINGLE QUOTATION MARK
			case 0x2019: // RIGHT SINGLE QUOTATION MARK
			case 0x201B: // SINGLE HIGH-REVERSED-9 QUOTATION MARK
			case 0xFF07: // FULLWIDTH APOSTROPHE
				if (map_quote) {
					code = '\'';
				}
				break;

			case 0x201C: // LEFT DOUBLE QUOTATION MARK
			case 0x201D: // RIGHT DOUBLE QUOTATION MARK
				if (map_quote) {
					code = '"';
				}
				break;

			default:
				if (rm_di && corpus_unicode_isignorable(code)) {
					continue;
				}
				break;
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

	if (size == SIZE_MAX) {
		err = CORPUS_ERROR_OVERFLOW;
		corpus_log(err, "text size (%"PRIu64" bytes)"
			   " exceeds maximum normalization size"
			   " (%"PRIu64" bytes)",
			   (uint64_t)size,
			   (uint64_t)(SIZE_MAX - 1));
		goto error;
	}

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
