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

#ifndef CORPUS_STEM_H
#define CORPUS_STEM_H

/**
 * \file stem.h
 *
 * Stemmers, for mapping normalized tokens to their types (stems).
 */

#include <stddef.h>
#include <stdint.h>

struct sb_stemmer;

/**
 * Generic stemming function. Takes pointer to NFC UTF-8 data and length;
 * outputs stem and length. Returns 0 on success, nonzero on failure.
 * Setting lenptr to a negative value is interpreted as no stem.
 */
typedef int (*corpus_stem_func)(const uint8_t *ptr, int len,
				const uint8_t **stemptr, int *lenptr,
				void *context);

/**
 * Stemmer.
 */
struct corpus_stem {
	struct corpus_textset excepts;
	corpus_stem_func stemmer;
	void *context;
	struct utf8lite_text type;
	int has_type;
};

/**
 * Snowball stemmer.
 */
struct corpus_stem_snowball {
	struct sb_stemmer *stemmer;
};

int corpus_stem_init(struct corpus_stem *stem, corpus_stem_func stemmer,
		     void *context);
void corpus_stem_destroy(struct corpus_stem *stem);
int corpus_stem_set(struct corpus_stem *stem, const struct utf8lite_text *tok);
int corpus_stem_except(struct corpus_stem *stem, const struct utf8lite_text *tok);


/**
 * Get a list of the Snowball stemmer algorithms (canonical names, not aliases).
 *
 * \returns a NULL-terminated array of algorithm names
 */
const char **corpus_stem_snowball_names(void);

int corpus_stem_snowball_init(struct corpus_stem_snowball *stem,
			      const char *alg);
void corpus_stem_snowball_destroy(struct corpus_stem_snowball *stem);
int corpus_stem_snowball(const uint8_t *ptr, int len,
			 const uint8_t **stemptr, int *lenptr, void *ctx);

#endif /* CORPUS_STEM_H */
