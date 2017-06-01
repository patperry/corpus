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
#include <limits.h>
#include <stddef.h>
#include <string.h>
#include "array.h"
#include "error.h"
#include "memory.h"
#include "table.h"
#include "census.h"
#include "ngram.h"

#define CORPUS_NGRAM_BUFFER_INIT (2 * (CORPUS_NGRAM_MAX + 1))

static int terms_init(struct corpus_ngram_terms *terms, int width);
static void terms_destroy(struct corpus_ngram_terms *terms);
static void terms_clear(struct corpus_ngram_terms *terms);
static int terms_add(struct corpus_ngram_terms *terms, const int *type_ids,
		     int *idptr);
static int terms_has(const struct corpus_ngram_terms *terms,
		     const int *type_ids, int *idptr);
static int terms_grow(struct corpus_ngram_terms *terms, int nadd);
static void terms_rehash(struct corpus_ngram_terms *terms);

static int term_equals(const int *type_ids1, const int *type_ids2, int width);
static unsigned term_hash(const int *type_ids, int width);
static unsigned hash_combine(unsigned seed, unsigned hash);


int corpus_ngram_init(struct corpus_ngram *ng, int width)
{
	int err, k, n;

	if (width < 1) {
		err = CORPUS_ERROR_INVAL;
		corpus_log(err, "n-gram width is non-positive (%d)",
			   width);
		goto error_width;
	} else if (width > CORPUS_NGRAM_MAX) {
		err = CORPUS_ERROR_INVAL;
		corpus_log(err, "n-gram width exceeds maximum (%d)",
			   CORPUS_NGRAM_MAX);
		goto error_width;
	}
	ng->width = width;

	n = CORPUS_NGRAM_BUFFER_INIT;
	if (!(ng->buffer = corpus_malloc(n * sizeof(*ng->buffer)))) {
		err = CORPUS_ERROR_NOMEM;
		goto error_buffer;
	}
	ng->nbuffer_max = n;
	ng->nbuffer = 0;

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
	corpus_free(ng->buffer);
error_buffer:
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
	corpus_free(ng->buffer);
}


void corpus_ngram_clear(struct corpus_ngram *ng)
{
	int k = ng->width;

	while (k-- > 0) {
		terms_clear(&ng->terms[k]);
	}

	ng->nbuffer = 0;
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
	const int *type_ids;
	int k, width = ng->width;
	int id, n, nmax, err;

	n = ng->nbuffer;
	nmax = ng->nbuffer_max;
	if (n == nmax) {
		memmove(ng->buffer, ng->buffer + (nmax + 1 - width),
			(width - 1) * sizeof(*ng->buffer));
		n = width - 1;
	}
	ng->buffer[n] = type_id;
	n++;
	ng->nbuffer = n;

	for (k = 0; k < width && k < n; k++) {
		terms = &ng->terms[k];
		type_ids = ng->buffer + n - k - 1;

		if ((err = terms_add(terms, type_ids, &id))) {
			goto out;
		}

		if ((err = corpus_census_add(&terms->census, id, weight))) {
			goto out;
		}
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
	int id, has;

	if (width < 1 || width > ng->width) {
		has = 0;
		goto out;
	}

	terms = &ng->terms[width - 1];
	if (terms_has(terms, type_ids, &id)) {
		has = corpus_census_has(&terms->census, id, &weight);
	} else {
		has = 0;
	}

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


int terms_add(struct corpus_ngram_terms *terms, const int *type_ids,
	      int *idptr)
{
	int id, err, pos, rehash, width;

	if (terms_has(terms, type_ids, &id)) {
		err = 0;
		goto out;
	}

	pos = id;
	id = terms->nterm;
	rehash = 0;

	if (terms->nterm == terms->nterm_max) {
		if ((err = terms_grow(terms, 1))) {
			goto out;
		}
	}

	if (terms->nterm == terms->table.capacity) {
		if ((err = corpus_table_reinit(&terms->table,
					       terms->nterm + 1))) {
			goto out;
		}
		rehash = 1;
	}

	width = terms->term_width;
	memcpy(terms->term_base + id * width, type_ids,
	       width * sizeof(*type_ids));
	terms->nterm++;

	if (rehash) {
		terms_rehash(terms);
	} else {
		terms->table.items[pos] = id;
	}

	err = 0;
out:
	if (err) {
		corpus_log(err, "failed adding n-gram term");
		id = -1;
	}

	if (idptr) {
		*idptr = id;
	}

	return err;
}


int terms_has(const struct corpus_ngram_terms *terms, const int *type_ids,
	      int *idptr)
{
	struct corpus_table_probe probe;
	const int *base;
	int found, id, width;
	unsigned hash;

	width = terms->term_width;

	if (width == 1) {
		// no need to create a new ID for unigrams; just use the
		// type ID
		found = 1;
		id = type_ids[0];
		goto out;
	}

	base = terms->term_base;
	hash = term_hash(type_ids, width);
	id = -1;
	found = 0;

	corpus_table_probe_make(&probe, &terms->table, hash);
	while (corpus_table_probe_advance(&probe)) {
		id = probe.current;
		if (term_equals(type_ids, base + id * width, width)) {
			found = 1;
			goto out;
		}
	}
out:
	if (idptr) {
		*idptr = found ? id : probe.index;
	}
	return found;
}


int terms_grow(struct corpus_ngram_terms *terms, int nadd)
{
	void *base = terms->term_base;
	int size = terms->nterm_max;
	int n = terms->nterm;
	int width = terms->term_width * sizeof(*terms->term_base);
	int err;

	if ((err = corpus_array_grow(&base, &size, width, n, nadd))) {
		corpus_log(err, "failed allocating terms array");
		return err;
	}

	terms->term_base = base;
	terms->nterm_max = size;
	return 0;
}


void terms_rehash(struct corpus_ngram_terms *terms)
{
	const int *base = terms->term_base;
	const int *type_ids;
	int width = terms->term_width;
	int i, n = terms->nterm;
	unsigned hash;

	corpus_table_clear(&terms->table);

	for (i = 0; i < n; i++) {
		type_ids = base + i * width;
		hash = term_hash(type_ids, width);
		corpus_table_add(&terms->table, hash, i);
	}
}


int term_equals(const int *type_ids1, const int *type_ids2, int width)
{
	if (memcmp(type_ids1, type_ids2, width * sizeof(*type_ids1)) == 0) {
		return 1;
	} else {
		return 0;
	}
}


unsigned term_hash(const int *type_ids, int width)
{
	int i;
	unsigned hash = 0;

	for (i = 0; i < width; i++) {
		hash = hash_combine(hash, type_ids[i]);
	}

	return hash;
}

/* adapted from boost/functional/hash/hash.hpp (Boost License, Version 1.0) */
unsigned hash_combine(unsigned seed, unsigned hash)
{
	seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	return seed;
}
