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

#include <stddef.h>
#include "error.h"
#include "memory.h"
#include "tree.h"


int corpus_tree_init(struct corpus_tree *t)
{
	t->nodes = NULL;
	t->nnode = 0;
	t->nnode_max = 0;
	t->depth = 0;
	t->is_unsorted = 0;
	return 0;
}


void corpus_tree_destroy(struct corpus_tree *t)
{
	corpus_free(t->nodes);
}


void corpus_tree_clear(struct corpus_tree *t)
{
	(void)t;
}


int corpus_tree_root(struct corpus_tree *t)
{
	(void)t;
	return 0;
}


int corpus_tree_add(struct corpus_tree *t, int parent_id, int key, int *idptr)
{
	(void)t;
	(void)parent_id;
	(void)key;
	(void)idptr;
	return 0;
}


int corpus_tree_has(const struct corpus_tree *t, int parent_id, int key,
		    int *idptr)
{
	(void)t;
	(void)parent_id;
	(void)key;
	(void)idptr;
	return 0;
}


int corpus_tree_sort(struct corpus_tree *t, void *base, size_t width)
{
	(void)t;
	(void)base;
	(void)width;
	return 0;
}
