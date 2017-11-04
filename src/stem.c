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
#include <limits.h>
#include <stddef.h>
#include "../lib/libstemmer_c/include/libstemmer.h"
#include "../lib/utf8lite/src/utf8lite.h"
#include "error.h"
#include "memory.h"
#include "table.h"
#include "textset.h"
#include "wordscan.h"
#include "stem.h"

static int needs_stem(const struct utf8lite_text *text);


int corpus_stem_init(struct corpus_stem *stem, corpus_stem_func stemmer,
		     void *context)
{
	int err;

	if ((err = corpus_textset_init(&stem->excepts))) {
		corpus_log(err, "failed initializing stem exception set");
		goto out;
	}

	stem->stemmer = stemmer;
	stem->context = context;
	stem->type.ptr = NULL;
	stem->type.attr = 0;
	stem->has_type = 0;

out:
	return err;
}


void corpus_stem_destroy(struct corpus_stem *stem)
{
	corpus_textset_destroy(&stem->excepts);
}


static int needs_stem(const struct utf8lite_text *text)
{
	struct corpus_wordscan scan;
	int needs = 0;

	corpus_wordscan_make(&scan, text);

	// only stem if first word is letter
	if (corpus_wordscan_advance(&scan)) {
		if (scan.type == CORPUS_WORD_LETTER) {
			needs = 1;
		}
	}

	// if there is a second word, don't stem
	if (corpus_wordscan_advance(&scan)) {
		needs = 0;
	}

	return needs;
}


int corpus_stem_set(struct corpus_stem *stem, const struct utf8lite_text *tok)
{
	struct utf8lite_message msg;
	size_t size;
	const uint8_t *ptr;
	int err, len;

	assert(!UTF8LITE_TEXT_HAS_ESC(tok));

	if (!stem->stemmer || corpus_textset_has(&stem->excepts, tok, NULL)) {
		stem->type = *tok;
		stem->has_type = 1;
		return 0;
	}

	size = UTF8LITE_TEXT_SIZE(tok);
	if (size >= INT_MAX) {
		err = CORPUS_ERROR_OVERFLOW;
		corpus_log(err, "token size (%"PRIu64" bytes)"
			   " exceeds maximum (%d)",
			   (uint64_t)size, INT_MAX - 1);
		goto out;
	}

	if ((err = (stem->stemmer)(tok->ptr, (int)size, &ptr, &len,
				   stem->context))) {
		goto out;
	}

	if (len < 0) {
		stem->has_type = 0;
		goto out;
	}

	if ((err = utf8lite_text_assign(&stem->type, ptr, (size_t)len,
					UTF8LITE_TEXT_UNKNOWN, &msg))) {
		corpus_log(err, "stemmer returned invalid type: %s",
			   msg.string);
		goto out;
	}

	stem->has_type = 1;
	err = 0;
out:
	if (err) {
		corpus_log(err, "failed stemming token");
		stem->has_type = 0;
	}
	return err;
}


int corpus_stem_except(struct corpus_stem *stem,
		       const struct utf8lite_text *tok)
{
	int err;

	assert(!UTF8LITE_TEXT_HAS_ESC(tok));

	if ((err = corpus_textset_add(&stem->excepts, tok, NULL))) {
		corpus_log(err, "failed adding token to stem exception set");
	}

	return err;
}


const char **corpus_stem_snowball_names(void)
{
	return sb_stemmer_list();
}


int corpus_stem_snowball_init(struct corpus_stem_snowball *stem,
			      const char *alg)
{
	int err = 0;

	if (!alg) {
		stem->stemmer = NULL;
		goto out;
	}

	errno = 0;
	stem->stemmer = sb_stemmer_new(alg, "UTF_8");
	if (!stem->stemmer) {
	       err = ((errno == ENOMEM) ? CORPUS_ERROR_NOMEM
					: CORPUS_ERROR_INVAL);
	}

out:
	switch (err) {
	case CORPUS_ERROR_NOMEM:
		corpus_log(err, "failed allocating Snowball stemmer");
		break;

	case CORPUS_ERROR_INVAL:
		corpus_log(err, "unrecognized Snowball stemming algorithm"
			   " (\"%s\")", alg);
		break;

	default:
		break;
	}

	return err;
}


void corpus_stem_snowball_destroy(struct corpus_stem_snowball *stem)
{
	if (stem->stemmer) {
		sb_stemmer_delete(stem->stemmer);
	}
}


int corpus_stem_snowball(const uint8_t *ptr, int len,
			 const uint8_t **stemptr, int *lenptr, void *ctx)
{
	struct utf8lite_message msg;
	struct corpus_stem_snowball *sb = ctx;
	struct utf8lite_text tok, typ;
	const uint8_t *stem, *buf;
	int err, stemlen, size;

	stem = ptr;
	stemlen = len;
	err = 0;

	if (!sb->stemmer || len < 0) {
		goto out;
	}

	assert((size_t)len <= UTF8LITE_TEXT_SIZE_MAX);

	tok.ptr = (uint8_t *)ptr;
	tok.attr = (size_t)len;

	if (!needs_stem(&tok)) {
		goto out;
	}

	buf = (const uint8_t *)sb_stemmer_stem(sb->stemmer, ptr, len);
	if (buf == NULL) {
		err = CORPUS_ERROR_NOMEM;
		corpus_log(err, "failed allocating memory to stem word"
			   " of size %"PRIu64" bytes", (uint64_t)len);
		goto out;
	}

	size = sb_stemmer_length(sb->stemmer);
	assert(size >= 0);

	if ((err = utf8lite_text_assign(&typ, buf, (size_t)size,
				      UTF8LITE_TEXT_UNKNOWN, &msg))) {
		err = CORPUS_ERROR_INTERNAL;
		corpus_log(err,
			   "Snowball stemmer returned invalid UTF-8 text: %s",
			   msg.string);
		goto out;
	}

	stem = buf;
	stemlen = size;

out:
	if (err) {
		stem = NULL;
		stemlen = -1;
	}
	if (stemptr) {
		*stemptr = stem;
	}
	if (lenptr) {
		*lenptr = stemlen;
	}

	return err;
}
