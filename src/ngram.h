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

#ifndef CORPUS_NGRAM_H
#define CORPUS_NGRAM_H

/**
 * \file ngram.h
 *
 * N-gram frequency counter.
 */


struct corpus_ngram_terms {
	struct corpus_table table;
	struct corpus_census census;
	int *term_base;
	int term_width;
	int nterm;
	int nterm_max;
};

struct corpus_ngram_iter {
	const struct corpus_ngram_terms *terms;
	int nterm;
	int width;

	const int *type_ids;
	double weight;
	int index;
};

struct corpus_ngram {
	struct corpus_ngram_terms *terms;
	int width;
};

int corpus_ngram_init(struct corpus_ngram *ng, int width);
void corpus_ngram_destroy(struct corpus_ngram *ng);
void corpus_ngram_clear(struct corpus_ngram *ng);

int corpus_ngram_count(struct corpus_ngram *ng, int width);

int corpus_ngram_add(struct corpus_ngram *ng, int type_id, double weight);
int corpus_ngram_break(struct corpus_ngram *ng);

int corpus_ngram_has(const struct corpus_ngram *ng, const int *type_ids,
		     int width, double *weightptr);


int corpus_ngram_iter_make(struct corpus_ngram_iter *it,
			   const struct corpus_ngram *ng, int width);
int corpus_ngram_iter_advance(struct corpus_ngram_iter *it);

#endif /* CORPUS_NGRAM_H */
