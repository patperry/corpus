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

#ifndef CORPUS_SENTFILTER_H
#define CORPUS_SENTFILTER_H

#include <stdint.h>

/**
 * \file sentfilter.h
 *
 * Sentence filter, for segmenting a text into a blocks of sentences.
 */

struct corpus_sentfilter {
	struct corpus_tree suppress;
	int *suppress_rules;
	struct corpus_sentscan scan;
	struct corpus_text current;
	int has_scan;
	int error;
};


const char **corpus_sentsuppress_names(void);
const uint8_t **corpus_sentsuppress_list(const char *name, int *lenptr);

int corpus_sentfilter_init(struct corpus_sentfilter *f);
void corpus_sentfilter_destroy(struct corpus_sentfilter *f);

int corpus_sentfilter_suppress(struct corpus_sentfilter *f,
			       const struct corpus_text *pattern);

int corpus_sentfilter_start(struct corpus_sentfilter *f,
			    const struct corpus_text *text);
int corpus_sentfilter_advance(struct corpus_sentfilter *f);

#endif /* CORPUS_SENTFILTER_H */
