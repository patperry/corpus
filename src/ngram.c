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
#include <stddef.h>
#include "error.h"
#include "memory.h"
#include "table.h"
#include "census.h"
#include "ngram.h"

static int terms_init(struct corpus_ngram_terms *terms, int width);
static void terms_destroy(struct corpus_ngram_terms *terms);
static void terms_clear(struct corpus_ngram_terms *terms);


int corpus_ngram_init(struct corpus_ngram *ng, int width)
{
	int err, k;

	if (width < 1) {
		err = CORPUS_ERROR_INVAL;
		corpus_log(err, "n-gram width is non-positive (%d)",
			   width);
		goto error_width;
	}
	ng->width = width;

	if (!(ng->terms = corpus_malloc(width * sizeof(*ng->terms)))) {
		err = CORPUS_ERROR_NOMEM;
		goto error_terms;
	}

	for (k = 0; k < width; k++) {
		if ((err = terms_init(&ng->terms[k], k + 1))) {
			goto error_terms_item;
		}
	}

	return 0;

error_terms_item:
	while (k-- > 0) {
		terms_destroy(&ng->terms[k]);
	}
	corpus_free(ng->terms);
error_terms:
error_width:
	corpus_log(err, "failed initializing n-gram counter");
	return err;
}


void corpus_ngram_destroy(struct corpus_ngram *ng)
{
	int k = ng->width;

	while (k-- > 0) {
		terms_destroy(&ng->terms[k]);
	}
	corpus_free(ng->terms);
}


void corpus_ngram_clear(struct corpus_ngram *ng)
{
	int k = ng->width;

	while (k-- > 0) {
		terms_clear(&ng->terms[k]);
	}
}


int corpus_ngram_count(struct corpus_ngram *ng, int width)
{
	if (width < 1 || width > ng->width) {
		return 0;
	}

	return ng->terms[width - 1].census.nitem;
}


int corpus_ngram_add(struct corpus_ngram *ng, int type_id, double weight)
{
	struct corpus_ngram_terms *terms;
	int err;

	terms = &ng->terms[0];
	if ((err = corpus_census_add(&terms->census, type_id, weight))) {
		goto out;
	}

out:
	if (err) {
		corpus_log(err, "failed adding to n-gram counts");
	}
	return err;
}


int corpus_ngram_break(struct corpus_ngram *ng)
{
	(void)ng;
	return 0;
}


int corpus_ngram_has(const struct corpus_ngram *ng, const int *type_ids,
		     int width, double *weightptr)
{
	const struct corpus_ngram_terms *terms;
	double weight;
	int has;

	if (width < 1 || width > ng->width) {
		has = 0;
		goto out;
	}

	terms = &ng->terms[0];
	has = corpus_census_has(&terms->census, type_ids[0], &weight);

out:
	if (!has) {
		weight = 0;
	}

	if (weightptr) {
		*weightptr = weight;
	}

	return has;
}


int corpus_ngram_iter_make(struct corpus_ngram_iter *it,
			   const struct corpus_ngram *ng, int width)
{
	if (width < 1 || width > ng->width) {
		it->terms = NULL;
		it->nterm = 0;
	} else {
		it->terms = &ng->terms[width - 1];
		it->nterm = it->terms->census.nitem;
	}

	it->width = width;
	it->type_ids = NULL;
	it->weight = 0;
	it->index = -1;
	return 0;
}


int corpus_ngram_iter_advance(struct corpus_ngram_iter *it)
{
	// already finished
	if (it->index == it->nterm) {
		return 0;
	}

	it->index++;
	if (it->index == it->nterm) {
		it->type_ids = NULL;
		it->weight = 0;
		return 0;
	}

	it->type_ids = &it->terms->census.items[it->index];
	it->weight = it->terms->census.weights[it->index];
	return 1;
}


int terms_init(struct corpus_ngram_terms *terms, int width)
{
	int err;
	assert(width > 0);

	if ((err = corpus_table_init(&terms->table))) {
		goto error_table;
	}

	if ((err = corpus_census_init(&terms->census))) {
		goto error_census;
	}

	terms->term_base = NULL;
	terms->term_width = width;
	terms->nterm = 0;
	terms->nterm_max = 0;
	return 0;

error_census:
	corpus_table_destroy(&terms->table);
error_table:
	corpus_log(err, "failed initializing n-gram terms");
	return err;
}


static void terms_destroy(struct corpus_ngram_terms *terms)
{
	corpus_free(terms->term_base);
	corpus_census_destroy(&terms->census);
	corpus_table_destroy(&terms->table);
}


void terms_clear(struct corpus_ngram_terms *terms)
{
	corpus_table_clear(&terms->table);
	corpus_census_clear(&terms->census);
	terms->nterm = 0;
}
