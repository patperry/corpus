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

#ifndef CORPUS_TREE_H
#define CORPUS_TREE_H

/**
 * \file tree.h
 *
 * N-ary tree, with integer keys.
 */

#include <stddef.h>

struct corpus_tree_node {
	int *keys;
	int *ids;
	int nitem;
};

struct corpus_tree {
	struct corpus_tree_node *nodes;
	int nnode, nnode_max;
	int is_unsorted;
};

int corpus_tree_init(struct corpus_tree *t);
void corpus_tree_destroy(struct corpus_tree *t);
void corpus_tree_clear(struct corpus_tree *t);

int corpus_tree_root(struct corpus_tree *t);
int corpus_tree_add(struct corpus_tree *t, int parent_id, int key, int *idptr);
int corpus_tree_has(const struct corpus_tree *t, int parent_id, int key,
		    int *idptr);

int corpus_tree_sort(struct corpus_tree *t, void *base, size_t width);


#endif /* CORPUS_TREE_H */
