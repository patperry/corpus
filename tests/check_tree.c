/*
 * Copyright 2016 Patrick O. Perry.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <check.h>
#include "../src/tree.h"
#include "testutil.h"

struct corpus_tree tree;

char *random_keys(void)
{
	int i, j, nkey = (int)rand() % 6;
	char *keys = alloc(nkey + 1);

	for (i = 0; i < nkey; i++) {
		j = (int)rand() % 4;
		keys[i] = 'a' + j;

	}
	keys[nkey] = '\0';

	return keys;
}


int get_parent(int id, int *keyptr)
{
	int i, n = tree.nnode;
	int j, m;

	for (i = 0; i < n; i++) {
		m = tree.nodes[i].nitem;
		for (j = 0; j < m; j++) {
			if (tree.nodes[i].ids[j] == id) {
				if (keyptr) {
					*keyptr = tree.nodes[i].keys[j];
				}
				return i;
			}
		}
	}

	if (keyptr) {
		*keyptr = -1;
	}
	return -1;
}


int get_depth(int id)
{
	int parent_id;
	int n;

	ck_assert(0 <= id);
	ck_assert(id < tree.nnode);

	n = 0;
	parent_id = id;
	while (parent_id > 0) {
		parent_id = get_parent(parent_id, NULL);
		n++;
	}

	return n;
}


char *get_keys(int id)
{
	int depth = get_depth(id);
	char *keys = alloc(depth + 1);
	int key;
	int parent_id;

	keys[depth] = '\0';

	while (depth-- > 0) {
		parent_id = get_parent(id, &key);
		keys[depth] = (char)key;
		id = parent_id;
	}

	return keys;
}


int compare_keys(const char *s1, const char *s2)
{
	int n1 = (int)strlen(s1);
	int n2 = (int)strlen(s2);
	int cmp;

	if (n1 < n2) {
		cmp = -1;
	} else if (n1 > n2) {
		cmp = +1;
	} else {
		cmp = strcmp(s1, s2);
	}

	return cmp;
}


void setup_tree(void)
{
	setup();
	corpus_tree_init(&tree);
}


void teardown_tree(void)
{
	corpus_tree_destroy(&tree);
	teardown();
}


int has(const char *keys)
{
	int i, nkey = (int)strlen(keys);
	int j, m;
	int id, parent_id;
	int found;

	if (tree.nnode == 0) {
		return 0;
	}

	if (tree.nnode > 0 && nkey == 0) {
		return 1;
	}

	id = 0;
	found = 0;

	for (i = 0; i < nkey; i++) {
		parent_id = id;
		found = corpus_tree_has(&tree, parent_id, keys[i], &id);

		m = tree.nodes[parent_id].nitem;
		for (j = 0; j < m; j++) {
			if (tree.nodes[parent_id].keys[j] == keys[i])
			{
				break;
			}
		}

		if (found) {
			ck_assert(0 <= id);
			ck_assert(id < tree.nnode);

			ck_assert(j < m);
			ck_assert_int_eq(tree.nodes[parent_id].ids[j], id);
		} else {
			ck_assert_int_eq(j, m);
			break;
		}
	}

	return found;
}


void add(const char *keys)
{
	const char *k1, *k2;
	int i, nkey = (int)strlen(keys);
	int j, m, n;
	int id, parent_id;
	int had;

	ck_assert(!corpus_tree_root(&tree));
	ck_assert_int_gt(tree.nnode, 0);
	id = 0;

	for (i = 0; i < nkey; i++) {
		parent_id = id;

		had = corpus_tree_has(&tree, parent_id, keys[i], NULL);
		m = tree.nodes[parent_id].nitem;
		n = tree.nnode;

		ck_assert(!corpus_tree_add(&tree, parent_id, keys[i], &id));
		ck_assert(0 <= id);
		ck_assert(id < tree.nnode);

		if (!had) {
			ck_assert_int_eq(tree.nodes[parent_id].nitem, m + 1);
			ck_assert_int_eq(tree.nnode, n + 1);
			m++;
			n++;
		}

		for (j = 0; j < m; j++) {
			if (tree.nodes[parent_id].keys[j] == keys[i])
			{
				break;
			}
		}

		ck_assert(j < m);
		ck_assert_int_eq(tree.nodes[parent_id].ids[j], id);
		if (!had) {
			ck_assert_int_eq(tree.nodes[id].nitem, 0);
		}
	}

	if (!tree.is_unsorted) {
		// all adjacent keys should be in order
		for (i = 1; i < tree.nnode; i++) {
			k1 = get_keys(i-1);
			k2 = get_keys(i);
			ck_assert(compare_keys(k1, k2) < 0);
		}
	}
}


void tree_clear(void)
{
	corpus_tree_clear(&tree);
	ck_assert_int_eq(tree.nnode, 0);
	ck_assert(!tree.is_unsorted);
}


void sort(void)
{
	int nnode = tree.nnode;
	char **keys = alloc(nnode * sizeof(char *));
	const char *k1, *k2;
	int i, j;

	for (i = 0; i < nnode; i++) {
		keys[i] = get_keys(i);
	}

	ck_assert(!corpus_tree_sort(&tree, NULL, 0));

	// check that the items are in sorted order
	ck_assert(!tree.is_unsorted);
	for (i = 1; i < tree.nnode; i++) {
		k1 = get_keys(i-1);
		k2 = get_keys(i);
		ck_assert(compare_keys(k1, k2) < 0);
	}

	// check that all items existed prior to the sorting
	ck_assert_int_eq(tree.nnode, nnode);
	for (i = 0; i < tree.nnode; i++) {
		k1 = get_keys(i);
		for (j = 0; j < nnode; j++) {
			if (compare_keys(keys[j], k1) == 0) {
				break;
			}
		}
		ck_assert(j < nnode);
	}
}



START_TEST(test_init)
{
	ck_assert_int_eq(tree.nnode, 0);
	ck_assert(!tree.is_unsorted);
	ck_assert(!has(""));
	ck_assert(!has("hello"));
}
END_TEST


START_TEST(test_add)
{
	add("abc");
	ck_assert(has("a"));
	ck_assert(!has("b"));
	ck_assert(!has("c"));
	ck_assert(has(""));
	ck_assert(has("ab"));
	ck_assert(has("abc"));
	ck_assert(!has("bc"));
}
END_TEST


START_TEST(test_add2)
{
	add("ab");
	add("aa");
	ck_assert(has(""));
	ck_assert(has("a"));
	ck_assert(has("ab"));
	ck_assert(has("aa"));
	ck_assert(!has("ba"));
	ck_assert(!has("b"));
}
END_TEST


START_TEST(test_sort_empty)
{
	sort();
}
END_TEST



START_TEST(test_sort_ordered)
{
	add("");
	add("a");
	add("b");
	add("c");
	add("ba");
	add("bc");
	add("ca");
	add("cb");
	add("cac");
	sort();
}
END_TEST



START_TEST(test_sort_reversed)
{
	add("cac");
	add("cb");
	add("ca");
	add("bc");
	add("ba");
	add("c");
	add("b");
	add("a");
	add("");
	sort();
}
END_TEST


START_TEST(test_add_duplicates)
{
	add("hello");
	add("hello");
	add("hello");
	ck_assert(has("hello"));
}
END_TEST


START_TEST(test_add_random)
{
	int seed, nseed = 50;
	int i;
	const char *keys;

	for (seed = 0; seed < nseed; seed++) {
		srand(seed);
		tree_clear();

		for (i = 0; i < 2 * nseed; i++) {
			keys = random_keys();
			add(keys);
		}

		sort();
	}
}
END_TEST


Suite *tree_suite(void)
{
        Suite *s;
        TCase *tc;

        s = suite_create("tree");
	tc = tcase_create("core");
        tcase_add_checked_fixture(tc, setup_tree, teardown_tree);
        tcase_add_test(tc, test_init);
        tcase_add_test(tc, test_add);
        tcase_add_test(tc, test_add2);
        tcase_add_test(tc, test_sort_empty);
        tcase_add_test(tc, test_sort_ordered);
        tcase_add_test(tc, test_sort_reversed);
        tcase_add_test(tc, test_add_duplicates);
        tcase_add_test(tc, test_add_random);
        suite_add_tcase(s, tc);

	return s;
}


int main(void)
{
        int number_failed;
        Suite *s;
        SRunner *sr;

        s = tree_suite();
        sr = srunner_create(s);

        srunner_run_all(sr, CK_NORMAL);
        number_failed = srunner_ntests_failed(sr);
        srunner_free(sr);

        return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
