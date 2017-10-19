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
#include <stdlib.h>
#include <string.h>
#include "array.h"
#include "error.h"
#include "memory.h"
#include "table.h"
#include "tree.h"


static int corpus_tree_grow(struct corpus_tree *t, int nadd);

static int node_init(struct corpus_tree_node *node, int parent_id, int key);
static void node_destroy(struct corpus_tree_node *node);
static int node_has(const struct corpus_tree_node *node, int key,
		    int *indexptr, const struct corpus_tree *tree);

static int node_insert(struct corpus_tree_node *node, int index, int id);
static int node_sort(struct corpus_tree_node *node,
		     const struct corpus_tree *tree);
static int node_grow(struct corpus_tree_node *node, int nadd);

static int root_init(struct corpus_tree_root *root);
static void root_destroy(struct corpus_tree_root *root);
static void root_clear(struct corpus_tree_root *root);
static int root_has(const struct corpus_tree_root *root, int key,
		    int *idptr, const struct corpus_tree *tree);
static int root_insert(struct corpus_tree_root *root, int index, int id,
		       const struct corpus_tree *tree);
static int root_sort(struct corpus_tree_root *root,
		     const struct corpus_tree *tree);
static int root_grow(struct corpus_tree_root *root, int nadd);
static void root_rehash(struct corpus_tree_root *root,
			const struct corpus_tree *tree);


int corpus_tree_init(struct corpus_tree *t)
{
	int err;

	t->nodes = NULL;
	t->nnode = 0;
	t->nnode_max = 0;

	if ((err = root_init(&t->root))) {
		goto error_root;
	}

	return 0;

error_root:
	corpus_log(err, "failed initializing tree");
	return err;
}


void corpus_tree_destroy(struct corpus_tree *t)
{
	corpus_tree_clear(t);
	corpus_free(t->nodes);
	root_destroy(&t->root);
}


void corpus_tree_clear(struct corpus_tree *t)
{
	int i = t->nnode;

	while (i-- > 0) {
		node_destroy(&t->nodes[i]);
	}

	t->nnode = 0;
	root_clear(&t->root);
}


int corpus_tree_add(struct corpus_tree *t, int parent_id, int key, int *idptr)
{
	struct corpus_tree_node *parent;
	int err, i, id;

	err = 0;

	// check whether the key exists already
	if (parent_id < 0) {
		parent = NULL;
		if (root_has(&t->root, key, &i, t)) {
			id = t->root.child_ids[i];
			goto out;
		}
	} else {
		parent = &t->nodes[parent_id];
		if (node_has(&t->nodes[parent_id], key, &i, t)) {
			id = parent->child_ids[i];
			goto out;
		}
	}

	// add a new node
	id = t->nnode;
	if (t->nnode == t->nnode_max) {
		if ((err = corpus_tree_grow(t, 1))) {
			goto out;
		}
		
		// re-set parent; tree_grow may have moved the nodes array
		if (parent_id >= 0) {
			parent = &t->nodes[parent_id];
		}
	}
	if ((err = node_init(&t->nodes[id], parent_id, key))) {
		goto out;
	}
	t->nnode++;

	// add the node to its parent
	if (parent_id < 0) {
		if ((err = root_insert(&t->root, i, id, t))) {
			goto out;
		}
	} else {
		if ((err = node_insert(parent, i, id))) {
			goto out;
		}
	}

out:
	if (err) {
		corpus_log(err, "failed adding node to tree");
		id = CORPUS_TREE_NONE;
	}

	if (idptr) {
		*idptr = id;
	}

	return err;
}


int corpus_tree_has(const struct corpus_tree *t, int parent_id, int key,
		    int *idptr)
{
	const struct corpus_tree_node *parent;
	int i, id, has;

	id = CORPUS_TREE_NONE;

	if (parent_id < 0) {
		if ((has = root_has(&t->root, key, &i, t))) {
			id = t->root.child_ids[i];
		}
	} else {
		parent = &t->nodes[parent_id];
		if ((has = node_has(parent, key, &i, t))) {
			id = parent->child_ids[i];
		}
	}

	if (idptr) {
		*idptr = id;
	}

	return has;
}


int corpus_tree_sort(struct corpus_tree *t, void *base, size_t width)
{
	size_t i, n = (size_t)t->nnode;
	int j, m;
	int qbegin, qend;
	int child_id, visit, *ids, *map;
	struct corpus_tree_node *nodebuf;
	char *buf = NULL;
	int err;

	if (n == 0) {
		return 0;
	}

	/* sort all of the node key sets */
	if ((err = root_sort(&t->root, t))) {
		goto error_root_sort;
	}
	for (i = 0; i < n; i++) {
		if ((err = node_sort(&t->nodes[i], t))) {
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

	/* add the root's children to the queue */
	for (i = 0; i < (size_t)t->root.nchild; i++) {
		ids[qend++] = t->root.child_ids[i];
	}

	/* while the queue is not empty */
	while (qbegin < qend) {
		/* visit the first element of the queue, and remove it */
		visit = ids[qbegin++];

		/* add all children to the queue */
		m = t->nodes[visit].nchild;
		for (j = 0; j < m; j++) {
			ids[qend++] = t->nodes[visit].child_ids[j];
		}
	}
	assert(qend == (int)n);

	/* construct the map from (old id) -> (new id) */
	for (i = 0; i < n; i++) {
		map[ids[i]] = (int)i;
	}

	/* put the nodes into sorted order */
	for (i = 0; i < n; i++) {
		nodebuf[i] = t->nodes[ids[i]];
		if (buf) {
			memcpy(buf + i * width,
			       (char *)base + (size_t)ids[i] * width,
			       width);
		}

		/* fix the parent id */
		if (nodebuf[i].parent_id >= 0) {
			nodebuf[i].parent_id = map[nodebuf[i].parent_id];
		}

		/* fix the child ids */
		m = nodebuf[i].nchild;
		for (j = 0; j < m; j++) {
			child_id = nodebuf[i].child_ids[j];
			child_id = map[child_id];
			nodebuf[i].child_ids[j] = child_id;
		}
	}

	/* fix the root's child ids */
	m = t->root.nchild;
	for (j = 0; j < m; j++) {
		child_id = t->root.child_ids[j];
		child_id = map[child_id];
		t->root.child_ids[j] = child_id;
	}

	/* copy the buffer data */
	memcpy(t->nodes, nodebuf, n * sizeof(*t->nodes));
	if (base) {
		memcpy(base, buf, n * width);
	}

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
error_root_sort:
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


int node_init(struct corpus_tree_node *node, int parent_id, int key)
{
	node->parent_id = parent_id;
	node->key = key;
	node->child_ids = NULL;
	node->nchild = 0;
	return 0;
}


void node_destroy(struct corpus_tree_node *node)
{
	corpus_free(node->child_ids);
}


int node_has(const struct corpus_tree_node *node, int key, int *indexptr,
	     const struct corpus_tree *tree)
{
	const int *ptr, *base = node->child_ids;
	int child_id, child_key;
	int i, n = node->nchild;

	while (n != 0) {
		i = n >> 1;
		ptr = base + i;
		child_id = *ptr;
		child_key = tree->nodes[child_id].key;
		if (child_key == key) {
			*indexptr = (int)(ptr - node->child_ids);
			return 1;
		} else if (child_key < key) {
			base = ptr + 1;
			n = n - i - 1;
		} else {
			n = i;
		}
	}

	*indexptr = (int)(base - node->child_ids);
	return 0;
}


int node_insert(struct corpus_tree_node *node, int index, int id)
{
	int err, ntail;

	if ((err = node_grow(node, 1))) {
		goto out;
	}

	ntail = node->nchild - index;
	memmove(node->child_ids + index + 1, node->child_ids + index,
		(size_t)ntail * sizeof(*node->child_ids));

	node->child_ids[index] = id;
	node->nchild++;

	err = 0;
out:
	if (err) {
		corpus_log(err, "failed adding child to tree node");
	}

	return err;
}


int node_sort(struct corpus_tree_node *node, const struct corpus_tree *tree)
{
	// no-op; nodes are already sorted
	(void)node;
	(void)tree;
	return 0;
}


int node_grow(struct corpus_tree_node *node, int nadd)
{
	int err, n;
	int *child_ids;

	if (nadd <= 0) {
		return 0;
	}

	if (node->nchild > INT_MAX - nadd) {
		err = CORPUS_ERROR_OVERFLOW;
		corpus_log(err, "number of tree node children (%d + %d)"
			   " exceeds maximum (%d)", node->nchild, nadd,
			   INT_MAX);
			   
		goto out;
	}

	n = node->nchild + nadd;
	if ((size_t)n > SIZE_MAX / sizeof(*node->child_ids)) {
		err = CORPUS_ERROR_OVERFLOW;
		corpus_log(err, "number of tree node children (%d)"
			   " exceeds maximum (%"PRIu64")", n,
			   (uint64_t)(SIZE_MAX / sizeof(*node->child_ids)));

		err = CORPUS_ERROR_OVERFLOW;
		goto out;
	}

	child_ids = corpus_realloc(node->child_ids,
				   (size_t)n * sizeof(*child_ids));
	if (!child_ids) {
		err = CORPUS_ERROR_NOMEM;
		goto out;
	}
	node->child_ids = child_ids;

	err = 0;

out:
	if (err) {
		corpus_log(err, "failed adding child to tree node");
	}

	return err;
}


int root_init(struct corpus_tree_root *root)
{
	int err;

	if ((err = corpus_table_init(&root->table))) {
		corpus_log(err, "failed initializing tree root");
		return err;
	}

	root->child_ids = NULL;
	root->nchild = 0;
	root->nchild_max = 0;
	return 0;
}


void root_destroy(struct corpus_tree_root *root)
{
	corpus_free(root->child_ids);
	corpus_table_destroy(&root->table);
}


void root_clear(struct corpus_tree_root *root)
{
	corpus_table_clear(&root->table);
	root->nchild = 0;
}


int root_has(const struct corpus_tree_root *root, int key,
	     int *idptr, const struct corpus_tree *tree)
{
	struct corpus_table_probe probe;
	int found, id, child_id;
	unsigned hash;

	hash = (unsigned)key;
	id = -1;
	found = 0;

	corpus_table_probe_make(&probe, &root->table, hash);
	while (corpus_table_probe_advance(&probe)) {
		id = probe.current;
		child_id = root->child_ids[id];
		if (tree->nodes[child_id].key == key) {
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


int root_insert(struct corpus_tree_root *root, int index, int id,
		const struct corpus_tree *tree)
{
	int err, pos, rehash;

	pos = index;
	index = root->nchild;
	rehash = 0;

	if (root->nchild == root->nchild_max) {
		if ((err = root_grow(root, 1))) {
			goto out;
		}
	}

	if (root->nchild == root->table.capacity) {
		if ((err = corpus_table_reinit(&root->table,
					       root->nchild + 1))) {
			goto out;
		}
		rehash = 1;
	}

	root->child_ids[index] = id;
	root->nchild++;

	if (rehash) {
		root_rehash(root, tree);
	} else {
		root->table.items[pos] = index;
	}

	err = 0;
out:
	if (err) {
		corpus_log(err, "failed adding child to tree root");
	}

	return err;

}


static int key_cmp(const void *x1, const void *x2)
{
	int key1 = *(const int *)x1;
	int key2 = *(const int *)x2;

	if (key1 < key2) {
		return -1;
	} else if (key1 > key2) {
		return +1;
	} else {
		return 0;
	}
}


int root_sort(struct corpus_tree_root *root, const struct corpus_tree *tree)
{
	struct { int key; int id; } *buffer = NULL;
	int err, child_id, i, n = root->nchild;

	if (n == 0) {
		return 0;
	}

	if (!(buffer = corpus_malloc((unsigned)n * sizeof(*buffer)))) {
		err = CORPUS_ERROR_NOMEM;
		goto out;
	}

	for (i = 0; i < n; i++) {
		child_id = root->child_ids[i];
		buffer[i].key = tree->nodes[child_id].key;
		buffer[i].id = child_id;
	}

	qsort(buffer, (unsigned)n, sizeof(*buffer), key_cmp);

	for (i = 0; i < n; i++) {
		root->child_ids[i] = buffer[i].id;
	}

	root_rehash(root, tree);
	err = 0;

out:
	if (err) {
		corpus_log(err, "failed sorting tree root children");
	}
	corpus_free(buffer);
	return err;
}


int root_grow(struct corpus_tree_root *root, int nadd)
{
	void *base = root->child_ids;
	int size = root->nchild_max;
	int n = root->nchild;
	size_t width = sizeof(*root->child_ids);
	int err;

	if ((err = corpus_array_grow(&base, &size, width, n, nadd))) {
		corpus_log(err, "failed allocating tree root child array");
		return err;
	}
	root->child_ids = base;
	root->nchild_max = size;
	return 0;

}


void root_rehash(struct corpus_tree_root *root,
		 const struct corpus_tree *tree)
{
	int i, n = root->nchild, child_id;
	unsigned hash;

	corpus_table_clear(&root->table);

	for (i = 0; i < n; i++) {
		child_id = root->child_ids[i];
		hash = (unsigned)tree->nodes[child_id].key;
		corpus_table_add(&root->table, hash, i);
	}
}
