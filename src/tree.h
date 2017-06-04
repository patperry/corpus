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
 * Rooted N-ary tree, with integer keys.
 */

#include <stddef.h>

/**
 * Code for missing ID, or the root.
 */
#define CORPUS_TREE_NONE	(-1)

/**
 * N-ary tree node.
 */
struct corpus_tree_node {
	int parent_id;	/**< parent ID (-1 for root) */
	int key;	/**< node key */
	int *child_ids;	/**< array of child IDs */
	int nchild;	/**< number of children */
};

/**
 * N-ary tree root.
 */
struct corpus_tree_root {
	struct corpus_table table; /**< child ID hash table */
	int *child_ids;		/**< array of child IDs */
	int nchild;		/**< number of children */
	int nchild_max;		/**< child array capacity */
};

/**
 * Rooted N-ary tree.
 */
struct corpus_tree {
	struct corpus_tree_node *nodes;	/**< array of tree nodes */
	struct corpus_tree_root root;	/**< root */
	int nnode;	/**< node array length */
	int nnode_max;	/**< node array capacity */	
};

/**
 * Initialize a new tree.
 *
 * \param t the tree
 *
 * \returns 0 on success
 */
int corpus_tree_init(struct corpus_tree *t);

/**
 * Release a tree's resources.
 *
 * \param t the tree
 */
void corpus_tree_destroy(struct corpus_tree *t);

/**
 * Remove all nodes from a tree.
 *
 * \param t the tree
 */
void corpus_tree_clear(struct corpus_tree *t);

/**
 * Add a child node to a tree node if it does not have one for the given key.
 *
 * \param t the tree
 * \param parent_id the parent node ID
 * \param key the key
 * \param idptr if non-NULL, a location to store the child node ID
 *
 * \returns 0 on success.
 */
int corpus_tree_add(struct corpus_tree *t, int parent_id, int key, int *idptr);

/**
 * Test whether a tree node has a child for the given key.
 *
 * \param t the tree
 * \param parent_id the parent node ID
 * \param key the key
 * \param idptr if non-NULL, a location to store the child node ID if it
 * 	exists
 *
 * \returns 0 if no child exists for the given key, nonzero otherwise
 */
int corpus_tree_has(const struct corpus_tree *t, int parent_id, int key,
		    int *idptr);

/**
 * Put the nodes of a tree into breadth-first order, re-assigning all node
 * IDs. Optionally, apply the same sorting operation to an array.
 *
 * \param t the tree
 * \param base if non-NULL, the base of the array to sort in parallel with
 * 	the nodes; the array must have length equal to the number of nodes
 * 	in the tree
 * \param width if base is non-NULL, the size of each array element,
 * 	in bytes; otherwise, ignored
 *
 * \returns 0 on success.
 */
int corpus_tree_sort(struct corpus_tree *t, void *base, size_t width);

#endif /* CORPUS_TREE_H */
