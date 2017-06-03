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

static int corpus_ngram_add_tree(struct corpus_ngram *ng, int type_id,
				 int *idptr);
static int corpus_ngram_has_tree(const struct corpus_ngram *ng, int type_id,
				 int *idptr);
static void corpus_ngram_rehash(struct corpus_ngram *ng);
static int corpus_ngram_grow(struct corpus_ngram *ng, int nadd);


static int tree_init(struct corpus_ngram_tree *tree, int type_id);
static void tree_destroy(struct corpus_ngram_tree *tree);
static int tree_count(const struct corpus_ngram_tree *tree);
static int tree_root(struct corpus_ngram_tree *tree);
static int tree_add(struct corpus_ngram_tree *tree, const int *type_ids,
		    int length, double weight);
static int tree_has(const struct corpus_ngram_tree *tree,
		     const int *type_ids, int length, int *idptr);


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

	n = CORPUS_NGRAM_BUFFER_INIT;
	if (!(ng->buffer = corpus_malloc(n * sizeof(*ng->buffer)))) {
		err = CORPUS_ERROR_NOMEM;
		goto error_buffer;
	}
	ng->nbuffer_max = n;
	ng->nbuffer = 0;

	if ((err = corpus_table_init(&ng->table))) {
		goto error_table;
	}

	ng->trees = NULL;
	ng->ntree = 0;
	ng->ntree_max = 0;
	return 0;

error_table:
	corpus_free(ng->buffer);
error_buffer:
error_length:
	corpus_log(err, "failed initializing n-gram counter");
	return err;
}


void corpus_ngram_destroy(struct corpus_ngram *ng)
{
	int k = ng->ntree;

	corpus_free(ng->buffer);
	while (k-- > 0) {
		tree_destroy(&ng->trees[k]);
	}
	corpus_free(ng->trees);
	corpus_table_destroy(&ng->table);
}


void corpus_ngram_clear(struct corpus_ngram *ng)
{
	int k = ng->ntree;

	corpus_table_clear(&ng->table);
	while (k-- > 0) {
		tree_destroy(&ng->trees[k]);
	}
	ng->ntree = 0;
	ng->nbuffer = 0;
}


int corpus_ngram_count(const struct corpus_ngram *ng, int *countptr)
{
	int i, n, c, count, err;

	err = 0;

	count = ng->ntree;
	if (ng->length == 1) {
		goto out;
	}

	n = ng->ntree;
	for (i = 0; i < n; i++) {
		c = tree_count(&ng->trees[i]);
		if (c > INT_MAX - count) {
			err = CORPUS_ERROR_OVERFLOW;
			corpus_log(err,
				   "number of n-grams exceeds maximum (%d)",
				   INT_MAX);
			goto out;
		}
		count += c;
	}

out:
	if (err) {
		count = -1;
	}
	if (countptr) {
		*countptr = count;
	}
	return err;
}


int corpus_ngram_add(struct corpus_ngram *ng, int type_id, double weight)
{
	struct corpus_ngram_tree *tree;
	int length = ng->length;
	int id, n, nmax, err;

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

	// find the unigram tree
	if (!corpus_ngram_has_tree(ng, type_id, &id)) {
		if ((err = corpus_ngram_add_tree(ng, type_id, &id))) {
			goto out;
		}
	}
	tree = &ng->trees[id];

	// update the weights
	if (n < length) {
		length = n;
	}
	if ((err = tree_add(tree, ng->buffer + n - length, length - 1,
			    weight))) {
		goto out;
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
	const struct corpus_ngram_tree *tree;
	double weight = 0;
	int i, j, has;

	if (length < 1 || length > ng->length) {
		has = 0;
		goto out;
	}

	// find the unigram tree
	if (!corpus_ngram_has_tree(ng, type_ids[length - 1], &i)) {
		has = 0;
		goto out;
	}

	tree = &ng->trees[i];
	if (length == 1) {
		has = 1;
		weight = tree->weight;
	} else {
		if ((has = tree_has(tree, type_ids, length - 1, &j))) {
			weight = tree->weights[j];
		}
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


int corpus_ngram_add_tree(struct corpus_ngram *ng, int type_id, int *idptr)
{
	int id, err, pos, rehash;

	if (corpus_ngram_has_tree(ng, type_id, &id)) {
		err = 0;
		goto out;
	}

	pos = id;
	id = ng->ntree;
	rehash = 0;

	if (ng->ntree == ng->ntree_max) {
		if ((err = corpus_ngram_grow(ng, 1))) {
			goto out;
		}
	}

	if (ng->ntree == ng->table.capacity) {
		if ((err = corpus_table_reinit(&ng->table, ng->ntree + 1))) {
			goto out;
		}
		rehash = 1;
	}

	if ((err = tree_init(&ng->trees[id], type_id))) {
		if (rehash) {
			corpus_ngram_rehash(ng);
		}
		goto  out;
	}
	ng->ntree++;

	if (rehash) {
		corpus_ngram_rehash(ng);
	} else {
		ng->table.items[pos] = id;
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


int corpus_ngram_has_tree(const struct corpus_ngram *ng, int type_id,
			  int *idptr)
{
	struct corpus_table_probe probe;
	int found, id;
	unsigned hash;

	hash = (unsigned)type_id;
	id = -1;
	found = 0;

	corpus_table_probe_make(&probe, &ng->table, hash);
	while (corpus_table_probe_advance(&probe)) {
		id = probe.current;
		if (ng->trees[id].type_id == type_id) {
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


int corpus_ngram_grow(struct corpus_ngram *ng, int nadd)
{
	void *base = ng->trees;
	int size = ng->ntree_max;
	int n = ng->ntree;
	size_t width = sizeof(*ng->trees);
	int err;

	if ((err = corpus_array_grow(&base, &size, width, n, nadd))) {
		corpus_log(err, "failed allocating n-gram tree array");
		return err;
	}
	ng->trees = base;
	ng->ntree_max = size;
	return 0;
}


void corpus_ngram_rehash(struct corpus_ngram *ng)
{
	int i, n = ng->ntree;
	unsigned hash;

	corpus_table_clear(&ng->table);

	for (i = 0; i < n; i++) {
		hash = (unsigned)ng->trees[i].type_id;
		corpus_table_add(&ng->table, hash, i);
	}
}

#if 0

void corpus_ngram_iter_make(struct corpus_ngram_iter *it,
			    const struct corpus_ngram *ng, int length)
{
	if (length < 1 || length > ng->length) {
		it->tree = NULL;
	} else {
		it->tree = &ng->tree[length - 1];
	}

	it->type_ids = NULL;
	it->weight = 0;
	it->index = -1;
}


int corpus_ngram_iter_advance(struct corpus_ngram_iter *it)
{
	const int *base;
	int length;

	// already finished
	if (it->tree == NULL || it->index == it->tree->nitem) {
		return 0;
	}

	it->index++;
	if (it->index == it->tree->nitem) {
		it->type_ids = NULL;
		it->weight = 0;
		return 0;
	}

	base = it->tree->type_ids;
	length = it->tree->length;
	it->type_ids = base + it->index * length;
	it->weight = it->tree->weights[it->index];
	return 1;
}

#endif


int tree_init(struct corpus_ngram_tree *tree, int type_id)
{
	int err;

	if ((err = corpus_tree_init(&tree->prefix))) {
		goto error_prefix;
	}

	tree->weights = NULL;
	tree->weight = 0;
	tree->type_id = type_id;
	return 0;

error_prefix:
	corpus_log(err, "failed initializing n-gram tree");
	return err;
}


static void tree_destroy(struct corpus_ngram_tree *tree)
{
	corpus_free(tree->weights);
	corpus_tree_destroy(&tree->prefix);
}


int tree_count(const struct corpus_ngram_tree *tree)
{
	if (tree->prefix.nnode > 0) {
		return tree->prefix.nnode - 1; // don't count the root
	} else {
		return 0;
	}
}


int tree_add(struct corpus_ngram_tree *tree, const int *type_ids,
	     int length, double weight)
{
	double *weights;
	int err, id, parent_id, nnode, nnode0, size, size0;

	tree->weight += weight;
	if (length == 0) {
		return 0;
	}

	if ((err = tree_root(tree))) {
		goto out;
	}

	id = 0;
	while (length-- > 0) {
		parent_id = id;
		nnode0 = tree->prefix.nnode;
		size0 = tree->prefix.nnode_max;
		if ((err = corpus_tree_add(&tree->prefix, parent_id,
					   type_ids[length], &id))) {
			goto out;
		}
		nnode = tree->prefix.nnode;

		// check whether a new node got added
		if (nnode0 < nnode) {
			// expand the weights array if necessary
			size = tree->prefix.nnode_max;
			if (size0 < size) {
				weights = tree->weights;
				weights = corpus_realloc(weights,
						size * sizeof(*weights));
				if (!weights) {
					err = CORPUS_ERROR_NOMEM;
					goto out;
				}
				tree->weights = weights;
			}

			// set the new weight to 0
			tree->weights[id] = 0;
		}

		// update the weight
		tree->weights[id] += weight;
	}
	err = 0;
out:
	if (err) {
		corpus_log(err, "failed adding to n-gram tree");
	}
	return err;
}


int tree_root(struct corpus_ngram_tree *tree)
{
	double *weights;
	int err, size;

	if (tree->prefix.nnode > 0) {
		return 0;
	}

	if ((err = corpus_tree_root(&tree->prefix))) {
		goto out;
	}

	size = tree->prefix.nnode_max;
	assert(size > 0);

	if (!(weights = corpus_malloc(size * sizeof(*weights)))) {
		err = CORPUS_ERROR_NOMEM;
		goto out;
	}
	weights[0] = 0;
	tree->weights = weights;
out:
	if (err) {
		corpus_log(err, "failed adding root to n-gram tree");
	}

	return 0;
}


int tree_has(const struct corpus_ngram_tree *tree, const int *type_ids,
	     int length, int *idptr)
{
	int has, id, parent_id;

	assert(length > 0);

	has = 0;
	if (tree->prefix.nnode == 0) {
		goto out;
	}

	id = 0;

	while (length-- > 0) {
		parent_id = id;
		if (!corpus_tree_has(&tree->prefix, parent_id,
				     type_ids[length], &id)) {
			goto out;
		}
	}

	has = 1;

out:
	if (!has) {
		id = -1;
	}
	if (idptr) {
		*idptr = id;
	}
	return has;
}


