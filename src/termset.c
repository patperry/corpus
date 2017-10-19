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
#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include "array.h"
#include "error.h"
#include "memory.h"
#include "table.h"
#include "tree.h"
#include "termset.h"

static int corpus_termset_grow_items(struct corpus_termset *set, int nadd);
static int corpus_termset_grow_buffer(struct corpus_termset *set, size_t nadd);

#define CHECK_ERROR(value) \
	do { \
		if (set->error) { \
			corpus_log(CORPUS_ERROR_INVAL, "an error occurred" \
				   " during a prior term set operation"); \
			return (value); \
		} \
	} while (0)

int corpus_termset_init(struct corpus_termset *set)
{
	int err;

	if ((err = corpus_tree_init(&set->prefix))) {
		corpus_log(err, "failed allocating term prefix tree");
		goto error_prefix;
	}
	assert(set->prefix.nnode_max == 0);
	set->term_ids = NULL;
	set->items = NULL;
	set->nitem = 0;
	set->nitem_max = 0;

	set->buffer = NULL;
	set->nbuf = 0;
	set->nbuf_max = 0;
	set->error = 0;

	return 0;

error_prefix:
	corpus_log(err, "failed initializing term set");
	return err;
}


void corpus_termset_destroy(struct corpus_termset *set)
{
	corpus_free(set->items);
	corpus_free(set->buffer);
	corpus_free(set->term_ids);
	corpus_tree_destroy(&set->prefix);
}


void corpus_termset_clear(struct corpus_termset *set)
{
	corpus_tree_clear(&set->prefix);
	set->nitem = 0;
	set->nbuf = 0;
}


int corpus_termset_add(struct corpus_termset *set, const int *type_ids,
		       int length, int *idptr)
{
	int *term_ids;
	int id, k, parent_id, n, n0, size, size0, type_id, term_id;
	int err;

	assert(length >= 1);

	CHECK_ERROR(CORPUS_ERROR_INVAL);
	err = 0;
	term_id = -1;

	// add the term prefixes
	n0 = set->prefix.nnode;
	size0 = set->prefix.nnode_max;
	id = CORPUS_TREE_NONE;
	for (k = 0; k < length; k++) {
		type_id = type_ids[k];
		parent_id = id;
		if ((err = corpus_tree_add(&set->prefix, parent_id,
					   type_id, &id))) {
			goto out;
		}
	}

	// at this point 'id' is the tree node ID for the term

	// grow the term ID array if the tree grew
	n = set->prefix.nnode;
	size = set->prefix.nnode_max;
	if (size0 < size) {
		if (!(term_ids = corpus_realloc(set->term_ids,
						(size_t)size
						* sizeof(*term_ids)))) {
			err = CORPUS_ERROR_NOMEM;
			goto out;
		}
		set->term_ids = term_ids;
	}
	while (n0 < n) {
		set->term_ids[n0] = -1;
		n0++;
	}
	term_id = set->term_ids[id];

	if (term_id >= 0) {
		// term already exists
		goto out;
	}

	// create a new term
	term_id = set->nitem;

	// grow the items array if necessary
	if (term_id == set->nitem_max) {
		if ((err = corpus_termset_grow_items(set, 1))) {
			goto out;
		}
	}

	// grow the buffer if necessary
	if ((size_t)length > SIZE_MAX - set->nbuf_max) {
		err = CORPUS_ERROR_OVERFLOW;
		corpus_log(err, "term set data size exceeds maximum"
			   " (%"PRIu64" type IDs)", (uint64_t)SIZE_MAX);
		goto out;
	}
	if (set->nbuf + (size_t)length > set->nbuf_max) {
		if ((err = corpus_termset_grow_buffer(set, (size_t)length))) {
			goto out;
		}
	}

	// initialize the new item
	memcpy(set->buffer + set->nbuf, type_ids,
	       (size_t)length * sizeof(*type_ids));
	set->items[term_id].type_ids = set->buffer + set->nbuf;
	set->items[term_id].length = length;
	set->nbuf += (size_t)length;

	// update the item count
	set->nitem++;

	// set the prefix node
	set->term_ids[id] = term_id;
	err = 0;

out:
	if (err) {
		corpus_log(err, "failed adding item to term set");
		set->error = err;
		term_id = -1;
	}

	if (idptr) {
		*idptr = term_id;
	}

	return err;
}


int corpus_termset_has(const struct corpus_termset *set, const int *type_ids,
		       int length, int *idptr)
{
	int k, id, parent_id, type_id, term_id;

	CHECK_ERROR(0);

	assert(length >= 1);

	term_id = -1;
	id = CORPUS_TREE_NONE;

	for (k = 0; k < length; k++) {
		type_id = type_ids[k];
		parent_id = id;
		if (!corpus_tree_has(&set->prefix, parent_id, type_id, &id)) {
			goto out;
		}
	}

	term_id = set->term_ids[id];

out:
	if (idptr) {
		*idptr = term_id;
	}

	return term_id >= 0 ? 1 : 0;
}


int corpus_termset_grow_items(struct corpus_termset *set, int nadd)
{
	void *base = set->items;
	int size = set->nitem_max;
	int err;

	if ((err = corpus_array_grow(&base, &size, sizeof(*set->items),
				     set->nitem, nadd))) {
		corpus_log(err, "failed allocating item array");
		return err;
	}

	set->items = base;
	set->nitem_max = size;
	return 0;
}


int corpus_termset_grow_buffer(struct corpus_termset *set, size_t nadd)
{
	int *buffer0 = set->buffer;
	void *base = buffer0;
	const int *type_ids;
	size_t size = set->nbuf_max;
	int err, i, n;

	if ((err = corpus_bigarray_grow(&base, &size, sizeof(*set->buffer),
					set->nbuf, nadd))) {
		corpus_log(err, "failed allocating term data buffer");
		return err;
	}
	set->buffer = base;
	set->nbuf_max = size;

	// update the items
	if (set->buffer != buffer0) {
		n = set->nitem;
		for (i = 0; i < n; i++) {
			type_ids = set->items[i].type_ids;
			set->items[i].type_ids = (set->buffer
						  + (type_ids - buffer0));
		}
	}

	return 0;
}
