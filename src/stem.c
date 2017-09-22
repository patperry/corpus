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
#include "error.h"
#include "memory.h"
#include "table.h"
#include "text.h"
#include "textset.h"
#include "wordscan.h"
#include "stem.h"


const char **corpus_stemmer_names(void)
{
	return sb_stemmer_list();
}


int corpus_stem_init(struct corpus_stem *stem, const char *alg)
{
	int err;

	if ((err = corpus_textset_init(&stem->excepts))) {
		corpus_log(err, "failed initializing stem exception set");
		goto out;
	}

	if (alg) {
		errno = 0;
		stem->stemmer = sb_stemmer_new(alg, "UTF_8");
		if (!stem->stemmer) {
		       if (errno == ENOMEM) {
			       err = CORPUS_ERROR_NOMEM;
			       corpus_log(err, "failed allocating stemmer");
		       } else {
				err = CORPUS_ERROR_INVAL;
				corpus_log(err,
					   "unrecognized stemming algorithm"
					   " (%s)", alg);
		       }
		       goto out;
		}
	} else {
		stem->stemmer = NULL;
	}

	stem->type.ptr = NULL;
	stem->type.attr = 0;
out:
	return err;
}


void corpus_stem_destroy(struct corpus_stem *stem)
{
	if (stem->stemmer) {
		sb_stemmer_delete(stem->stemmer);
	}
	corpus_textset_destroy(&stem->excepts);
}


static int classify(const struct corpus_text *text, int *lenptr)
{
	struct corpus_wordscan scan;
	int kind, len;

	len = 0;
	kind = CORPUS_WORD_NONE;
	corpus_wordscan_make(&scan, text);

	// get the kind from the first word
	if (corpus_wordscan_advance(&scan)) {
		kind = scan.type;
		len++;
	}

	while (corpus_wordscan_advance(&scan)) {
		len++;
	}

	if (lenptr) {
		*lenptr = len;
	}
	return kind;
}


int corpus_stem_set(struct corpus_stem *stem, const struct corpus_text *tok)
{
	const uint8_t *buf;
	size_t size;
	int err, kind, nword0, nword;

	assert(!CORPUS_TEXT_HAS_ESC(tok));

	if (!stem->stemmer || corpus_textset_has(&stem->excepts, tok, NULL)) {
		stem->type = *tok;
		return 0;
	}

	size = CORPUS_TEXT_SIZE(tok);
	if (size >= INT_MAX) {
		err = CORPUS_ERROR_OVERFLOW;
		corpus_log(err, "token size (%"PRIu64" bytes)"
			   " exceeds maximum (%d)",
			   (uint64_t)size, INT_MAX - 1);
		goto out;
	}
	
	kind = classify(tok, &nword0);

	// only stem letter words
	if (kind != CORPUS_WORD_LETTER) {
		stem->type = *tok;
		return 0;
	}

	buf = (const uint8_t *)sb_stemmer_stem(stem->stemmer, tok->ptr,
					       (int)size);
	if (buf == NULL) {
		err = CORPUS_ERROR_NOMEM;
		corpus_log(err, "failed allocating memory to stem word"
			   " of size %"PRIu64" bytes", (uint64_t)size);
		goto out;
	}

	// keep old utf8 bit, but update to new size
	size = (size_t)sb_stemmer_length(stem->stemmer);
	stem->type.ptr = (uint8_t *)buf;
	stem->type.attr = (tok->attr & ~CORPUS_TEXT_SIZE_MASK) | size;
	classify(&stem->type, &nword);
	
	// only stem the token if the number of words doesn't change; this
	// protects against turning inner punctuation like 'u.s' to
	// outer punctuation like 'u.'
	if (nword != nword0) {
		stem->type = *tok;
	}
	err = 0;
out:
	return err;
}


int corpus_stem_except(struct corpus_stem *stem,
		       const struct corpus_text *tok)
{
	int err;

	assert(!CORPUS_TEXT_HAS_ESC(tok));

	if ((err = corpus_textset_add(&stem->excepts, tok, NULL))) {
		corpus_log(err, "failed adding token to stem exception set");
	}

	return err;
}
