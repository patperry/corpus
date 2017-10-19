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
#include <check.h>
#include "../src/table.h"
#include "../src/intset.h"
#include "testutil.h"

struct corpus_intset set;

void setup_intset(void)
{
	setup();
	corpus_intset_init(&set);
}


void teardown_intset(void)
{
	corpus_intset_destroy(&set);
	teardown();
}


int has(int key)
{
	int id, i, n;
	int found;

	found = corpus_intset_has(&set, key, &id);

	if (found) {
		ck_assert(0 <= id);
		ck_assert(id < set.nitem);
		ck_assert_int_eq(set.items[id], key);
	} else {
		n = set.nitem;
		for (i = 0; i < n; i++) {
			ck_assert(set.items[i] != key);
		}
	}

	return found;
}


void add(int key)
{
	int id;
	int found0, n0;

	found0 = has(key);
	n0 = set.nitem;

	ck_assert(!corpus_intset_add(&set, key, &id));

	if (found0) {
		ck_assert_int_eq(set.nitem, n0);
	} else {
		ck_assert_int_eq(set.nitem, n0 + 1);
	}

	ck_assert(0 <= id);
	ck_assert(id < set.nitem);
	ck_assert_int_eq(set.items[id], key);
}


void clear()
{
	corpus_intset_clear(&set);
	ck_assert_int_eq(set.nitem, 0);
}


void sort()
{
	int nitem = set.nitem;
	int *items = alloc((size_t)nitem * sizeof(int));
	int i, j;

	for (i = 0; i < nitem; i++) {
		items[i] = set.items[i];
	}

	ck_assert(!corpus_intset_sort(&set, NULL, 0));

	// check that the items are in sorted order
	for (i = 1; i < set.nitem; i++) {
		ck_assert(set.items[i-1] < set.items[i]);
	}

	// check that all items existed prior to the sorting
	ck_assert_int_eq(set.nitem, nitem);
	for (i = 0; i < set.nitem; i++) {
		for (j = 0; j < nitem; j++) {
			if (items[j] == set.items[i]) {
				break;
			}
		}
		ck_assert(j < nitem);
	}
}


START_TEST(test_init)
{
	ck_assert_int_eq(set.nitem, 0);
	ck_assert(!has(0));
	ck_assert(!has(4));
}
END_TEST


START_TEST(test_add)
{
	add(4);
	ck_assert(!has(0));
	ck_assert(!has(3));
	ck_assert(has(4));
	ck_assert(!has(5));
}
END_TEST


START_TEST(test_add2)
{
	add(1);
	add(2);
	ck_assert(!has(0));
	ck_assert(has(1));
	ck_assert(has(2));
	ck_assert(!has(3));
}
END_TEST


START_TEST(test_sort_empty)
{
	sort();
}
END_TEST


START_TEST(test_sort_ordered)
{
	int i, n = 100;
	for (i = 0; i < n; i++) {
		add(i);
	}
	sort();
}
END_TEST


START_TEST(test_sort_reversed)
{
	int i, n = 100;
	for (i = 0; i < n; i++) {
		add(n - i);
	}
	sort();
}
END_TEST


START_TEST(test_add_duplicates)
{
	add(7);
	add(7);
	add(7);
	ck_assert(has(7));
}
END_TEST


START_TEST(test_add_random)
{
	int seed, nseed = 50;
	int i, key;

	for (seed = 0; seed < nseed; seed++) {
		srand((unsigned)seed);
		clear();

		for (i = 0; i < 2 * nseed; i++) {
			// choose a key in [0,nseed)
			key = rand() % nseed;
			add(key);
		}

		sort();
	}
}
END_TEST


Suite *intset_suite(void)
{
        Suite *s;
        TCase *tc;

        s = suite_create("intset");
	tc = tcase_create("core");
        tcase_add_checked_fixture(tc, setup_intset, teardown_intset);
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

        s = intset_suite();
        sr = srunner_create(s);

        srunner_run_all(sr, CK_NORMAL);
        number_failed = srunner_ntests_failed(sr);
        srunner_free(sr);

        return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
