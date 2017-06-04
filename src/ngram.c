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
#include "tree.h"
#include "ngram.h"

#define CORPUS_NGRAM_BUFFER_INIT (2 * (CORPUS_NGRAM_WIDTH_MAX + 1))


int corpus_ngram_init(struct corpus_ngram *ng, int length)
{
	int err, n;

	if (length < 1) {
		err = CORPUS_ERROR_INVAL;
		corpus_log(err, "n-gram length is non-positive (%d)",
			   length);
		goto error_length;
	} else if (length > CORPUS_NGRAM_WIDTH_MAX) {
		err = CORPUS_ERROR_INVAL;
		corpus_log(err, "n-gram length exceeds maximum (%d)",
			   CORPUS_NGRAM_WIDTH_MAX);
		goto error_length;
	}
	ng->length = length;

	if ((err = corpus_tree_init(&ng->terms))) {
		goto error_terms;
	}
	ng->weights = NULL;

	n = CORPUS_NGRAM_BUFFER_INIT;
	if (!(ng->buffer = corpus_malloc(n * sizeof(*ng->buffer)))) {
		err = CORPUS_ERROR_NOMEM;
		goto error_buffer;
	}
	ng->nbuffer_max = n;
	ng->nbuffer = 0;
	return 0;

error_buffer:
	corpus_tree_destroy(&ng->terms);
error_terms:
error_length:
	corpus_log(err, "failed initializing n-gram counter");
	return err;
}


void corpus_ngram_destroy(struct corpus_ngram *ng)
{
	corpus_free(ng->buffer);
	corpus_free(ng->weights);
	corpus_tree_destroy(&ng->terms);
}


void corpus_ngram_clear(struct corpus_ngram *ng)
{
	corpus_tree_clear(&ng->terms);
	ng->nbuffer = 0;
}


int corpus_ngram_add(struct corpus_ngram *ng, int type_id, double weight)
{
	double *weights;
	const int *type_ids;
	int key, length, id, parent_id, n, nmax, nnode, nnode0, size, size0;
	int err;

	length = ng->length;

	// update the input buffer
	n = ng->nbuffer;
	nmax = ng->nbuffer_max;
	if (n == nmax) {
		memmove(ng->buffer, ng->buffer + (nmax + 1 - length),
			(length - 1) * sizeof(*ng->buffer));
		n = length - 1;
	}
	ng->buffer[n] = type_id;
	n++;
	ng->nbuffer = n;

	// update the weights
	id = CORPUS_TREE_NONE;
	if (n < length) {
		length = n;
	}
	type_ids = ng->buffer + ng->nbuffer - length;

	while (length-- > 0) {
		key = type_ids[length];
		parent_id = id;
		nnode0 = ng->terms.nnode;
		size0 = ng->terms.nnode_max;
		if ((err = corpus_tree_add(&ng->terms, parent_id, key, &id))) {
			goto out;
		}
		nnode = ng->terms.nnode;
		size = ng->terms.nnode_max;

		// check whether a new node got added
		if (nnode0 < nnode) {
			// expand the weights array if necessary
			size = ng->terms.nnode_max;
			if (size0 < size) {
				weights = ng->weights;
				weights = corpus_realloc(weights,
						size * sizeof(*weights));
				if (!weights) {
					err = CORPUS_ERROR_NOMEM;
					goto out;
				}
				ng->weights = weights;
			}

			// set the new weight to 0
			ng->weights[id] = 0;
		}

		// update the weight
		ng->weights[id] += weight;
	}
	err = 0;

out:
	if (err) {
		corpus_log(err, "failed adding to n-gram counts");
	}
	return err;
}


int corpus_ngram_break(struct corpus_ngram *ng)
{
	ng->nbuffer = 0;
	return 0;
}


int corpus_ngram_has(const struct corpus_ngram *ng, const int *type_ids,
		     int length, double *weightptr)
{
	double weight = 0;
	int has, id, parent_id;

	has = 0;
	id = CORPUS_TREE_NONE;

	if (length < 1 || length > ng->length) {
		has = 0;
		goto out;
	}

	while (length-- > 0) {
		parent_id = id;
		if (!corpus_tree_has(&ng->terms, parent_id,
				     type_ids[length], &id)) {
			goto out;
		}
	}

	has = 1;

out:
	if (!has) {
		weight = 0;
	} else {
		weight = ng->weights[id];
	}

	if (weightptr) {
		*weightptr = weight;
	}

	return has;
}
