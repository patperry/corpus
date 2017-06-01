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
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "array.h"
#include "error.h"
#include "memory.h"
#include "tree.h"


static int corpus_tree_grow(struct corpus_tree *t, int nadd);

static int node_init(struct corpus_tree_node *node);
static void node_destroy(struct corpus_tree_node *node);
static int node_has(const struct corpus_tree_node *node, int key,
		    int *indexptr);
static int node_insert(struct corpus_tree_node *node, int index, int key,
		       int id);
static int node_sort(struct corpus_tree_node *node);
static int node_grow(struct corpus_tree_node *node, int nadd);


int corpus_tree_init(struct corpus_tree *t)
{
	t->nodes = NULL;
	t->nnode = 0;
	t->nnode_max = 0;
	t->is_unsorted = 0;
	return 0;
}


void corpus_tree_destroy(struct corpus_tree *t)
{
	corpus_tree_clear(t);
	corpus_free(t->nodes);
}


void corpus_tree_clear(struct corpus_tree *t)
{
	int i = t->nnode;

	while (i-- > 0) {
		node_destroy(&t->nodes[i]);
	}

	t->nnode = 0;
	t->is_unsorted = 0;
}


int corpus_tree_root(struct corpus_tree *t)
{
	int err;

	if (t->nnode > 0) {
		return 0;
	}

	if ((err = corpus_tree_grow(t, 1))) {
		goto out;
	}

	if ((err = node_init(&t->nodes[0]))) {
		goto out;
	}

	t->nnode = 1;
	err = 0;

out:
	if (err) {
		corpus_log(err, "failed adding root to tree");
	}
	return err;
}


int corpus_tree_add(struct corpus_tree *t, int parent_id, int key, int *idptr)
{
	int err, i, id;

	// check whether the key exists already
	if (node_has(&t->nodes[parent_id], key, &i)) {
		id = t->nodes[parent_id].ids[i];
		err = 0;
		goto out;
	}

	// add a new node
	id = t->nnode;
	if (t->nnode == t->nnode_max) {
		if ((err = corpus_tree_grow(t, 1))) {
			goto out;
		}
	}
	if ((err = node_init(&t->nodes[id]))) {
		goto out;
	}
	t->nnode++;

	// add the node to its parent
	if ((err = node_insert(&t->nodes[parent_id], i, key, id))) {
		goto out;
	}

	// update the is_unsorted flag
	t->is_unsorted = 1;

out:
	if (err) {
		corpus_log(err, "failed adding node to tree");
		id = -1;
	}

	if (idptr) {
		*idptr = id;
	}

	return err;
}


int corpus_tree_has(const struct corpus_tree *t, int parent_id, int key,
		    int *idptr)
{
	int i, id, ret;

	if (node_has(&t->nodes[parent_id], key, &i)) {
		id = t->nodes[parent_id].ids[i];
		ret = 1;
	} else {
		id = -1;
		ret = 0;
	}

	if (idptr) {
		*idptr = id;
	}

	return ret;
}


int corpus_tree_sort(struct corpus_tree *t, void *base, size_t width)
{
	int i, n = t->nnode;
	int j, m;
	int qbegin, qend;
	int child_id, visit, *ids, *map;
	struct corpus_tree_node *nodebuf;
	char *buf = NULL;
	int err;

	if (n == 0 || !t->is_unsorted) {
		return 0;
	}

	/* sort all of the node key sets */
	for (i = 0; i < n; i++) {
		if ((err = node_sort(&t->nodes[i]))) {
			goto error_node_sort;
		}
	}

	if (!(ids = corpus_malloc(n * sizeof(*ids)))) {
		err = CORPUS_ERROR_NOMEM;
		goto error_ids;
	} else if (!(map = corpus_malloc(n * sizeof(*map)))) {
		err = CORPUS_ERROR_NOMEM;
		goto error_map;
	} else if (!(nodebuf = corpus_malloc(n * sizeof(*nodebuf)))) {
		err = CORPUS_ERROR_NOMEM;
		goto error_nodebuf;
	} else if (base && !(buf = corpus_malloc(n * width))) {
		err = CORPUS_ERROR_NOMEM;
		goto error_buf;
	}

	/* start with an empty queue */
	qbegin = 0;
	qend = 0;

	/* add the root to the queue */
	if (n > 0) {
		ids[qend++] = 0;
	}

	/* while the queue is not empty */
	while (qbegin < qend) {
		/* visit the first element of the queue, and remove it */
		visit = ids[qbegin++];

		/* add all children to the queue */
		m = t->nodes[visit].nitem;
		for (j = 0; j < m; j++) {
			ids[qend++] = t->nodes[visit].ids[j];
		}
	}
	assert(qend == n);

	/* construct the map from (old id) -> (new id) */
	for (i = 0; i < n; i++) {
		map[ids[i]] = i;
	}

	/* put the nodes into sorted order */
	for (i = 0; i < n; i++) {
		nodebuf[i] = t->nodes[ids[i]];
		if (buf) {
			memcpy(buf + i * width, (char *)base + ids[i] * width,
			       width);
		}

		// Not needed any more since we no longer store parent_id:
		//
		// /* fix the parent id */
		// if (nodebuf[i].parent_id >= 0) {
		//	nodebuf[i].parent_id = map[nodebuf[i].parent_id];
		// }

		/* fix the child ids */
		m = nodebuf[i].nitem;
		for (j = 0; j < m; j++) {
			child_id = nodebuf[i].ids[j];
			child_id = map[child_id];
			nodebuf[i].ids[j] = child_id;
		}
	}
	memcpy(t->nodes, nodebuf, n * sizeof(*t->nodes));
	if (base) {
		memcpy(base, buf, n * width);
	}

	t->is_unsorted = 0;
	err = 0;

	corpus_free(buf);
error_buf:
	corpus_free(nodebuf);
error_nodebuf:
	corpus_free(map);
error_map:
	corpus_free(ids);
error_ids:
error_node_sort:
	if (err) {
		corpus_log(err, "failed sorting tree");
	}
	return err;
}


int corpus_tree_grow(struct corpus_tree *t, int nadd)
{
	void *base = t->nodes;
	int size = t->nnode_max;
	int err;

	if ((err = corpus_array_grow(&base, &size, sizeof(*t->nodes),
				     t->nnode, nadd))) {
		corpus_log(err, "failed allocating node array");
		return err;
	}

	t->nodes = base;
	t->nnode_max = size;
	return 0;
}


int node_init(struct corpus_tree_node *node)
{
	node->keys = NULL;
	node->ids = NULL;
	node->nitem = 0;
	return 0;
}


void node_destroy(struct corpus_tree_node *node)
{
	corpus_free(node->ids);
	corpus_free(node->keys);
}


int node_has(const struct corpus_tree_node *node, int key, int *indexptr)
{
	int i, n = node->nitem;

	for (i = 0; i < n; i++) {
		if (node->keys[i] == key) {
			*indexptr = i;
			return 1;
		} else if (node->keys[i] > key) {
			break;
		}
	}
	*indexptr = i;

	return 0;
}


int node_insert(struct corpus_tree_node *node, int index, int key, int id)
{
	int err, ntail;

	if ((err = node_grow(node, 1))) {
		goto out;
	}

	ntail = node->nitem - index;
	memmove(node->keys + index + 1, node->keys + index,
		ntail * sizeof(*node->keys));
	memmove(node->ids + index + 1, node->ids + index,
		ntail * sizeof(*node->ids));

	node->keys[index] = key;
	node->ids[index] = id;
	node->nitem++;

	err = 0;
out:
	if (err) {
		corpus_log(err, "failed adding child to tree node");
	}

	return err;
}


int node_sort(struct corpus_tree_node *node)
{
	// no-op; nodes are already sorted
	(void)node;
	return 0;
}


int node_grow(struct corpus_tree_node *node, int nadd)
{
	int err, n;
	int *keys, *ids;

	if (nadd <= 0) {
		return 0;
	}

	if (node->nitem > INT_MAX - nadd) {
		err = CORPUS_ERROR_OVERFLOW;
		corpus_log(err, "number of tree node children (%d + %d)"
			   " exceeds maximum (%d)", node->nitem, nadd,
			   INT_MAX);
			   
		goto out;
	}

	n = node->nitem + nadd;
	if ((size_t)n > SIZE_MAX / sizeof(*node->keys)) {
		err = CORPUS_ERROR_OVERFLOW;
		corpus_log(err, "number of tree node children (%d)"
			   " exceeds maximum (%"PRIu64")", n,
			   (uint64_t)(SIZE_MAX / sizeof(*node->keys)));

		err = CORPUS_ERROR_OVERFLOW;
		goto out;
	}

	if ((size_t)n > SIZE_MAX / sizeof(*node->ids)) {
		err = CORPUS_ERROR_OVERFLOW;
		corpus_log(err, "number of tree node children (%d)"
			   " exceeds maximum (%"PRIu64")", n,
			   (uint64_t)(SIZE_MAX / sizeof(*node->ids)));

		err = CORPUS_ERROR_OVERFLOW;
		goto out;
	}

	keys = corpus_realloc(node->keys, n * sizeof(*keys));
	if (!keys) {
		err = CORPUS_ERROR_NOMEM;
		goto out;
	}
	node->keys = keys;

	ids = corpus_realloc(node->ids, n * sizeof(*ids));
	if (!ids) {
		err = CORPUS_ERROR_NOMEM;
		goto out;
	}
	node->ids = ids;

	err = 0;

out:
	if (err) {
		corpus_log(err, "failed adding child to tree node");
	}

	return err;
}
