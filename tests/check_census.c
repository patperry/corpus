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

#include <check.h>
#include <stdlib.h>
#include "../src/table.h"
#include "../src/census.h"
#include "testutil.h"

struct corpus_census census;

void setup_census(void)
{
	setup();
	corpus_census_init(&census);
}


void teardown_census(void)
{
	corpus_census_destroy(&census);
	teardown();
}


double get(int i)
{
	int pos;
	double val0, val;

	val0 = 0;
	for (pos = 0; pos < census.nitem; pos++) {
		if (census.items[pos] == i) {
			val0 += census.weights[pos];
		}
	}

	if (corpus_census_has(&census, i, &val)) {
		ck_assert(val == val0);
	} else {
		ck_assert(0.0 == val0);
	}

	return val;
}


void add(int i, double x)
{
	double oldval, newval;

	oldval = get(i);
	ck_assert(!corpus_census_add(&census, i, x));
	newval = get(i);

	ck_assert(newval == oldval + x);
}


void clear()
{
	corpus_census_clear(&census);
	ck_assert_int_eq(census.nitem, 0);
}


void sort()
{
	int nval = census.nitem;
	double *vals = alloc((size_t)nval * sizeof(double));
	int *inds = alloc((size_t)nval * sizeof(int));
	double val;
	int i, j;

	for (i = 0; i < nval; i++) {
		inds[i] = census.items[i];
		if (corpus_census_has(&census, inds[i], &val)) {
			vals[i] = val;
		} else {
			vals[i] = 0;
		}
	}

	ck_assert(!corpus_census_sort(&census));

	// check that the weights are in descending order
	for (i = 1; i < census.nitem; i++) {
		// if a tie, sort by item
		if (census.weights[i-1] == census.weights[i]) {
			ck_assert(census.items[i-1] < census.items[i]);
		} else {
			ck_assert(census.weights[i-1] > census.weights[i]);
		}
	}

	// check that all indices existed prior to the sorting
	ck_assert(census.nitem <= nval);
	for (i = 0; i < census.nitem; i++) {
		for (j = 0; j < nval; j++) {
			if (inds[j] == census.items[i]) {
				break;
			}
		}
		ck_assert(j < nval);
	}

	// check the new values match the old values
	for (i = 0; i < nval; i++) {
		if (corpus_census_has(&census, inds[i], &val)) {
			ck_assert(vals[i] == val);
		} else {
			ck_assert(vals[i] == 0);
		}
	}
}


START_TEST(test_init)
{
	ck_assert_int_eq(census.nitem, 0);
	ck_assert(get(0) == 0);
	ck_assert(get(4) == 0);
}
END_TEST


START_TEST(test_add)
{
	add(4, 3.14);
	ck_assert(get(0) == 0);
	ck_assert(get(3) == 0);
	ck_assert(get(4) == 3.14);
	ck_assert(get(5) == 0);
}
END_TEST


START_TEST(test_add_duplicates)
{
	add(7, 2);
	ck_assert(get(7) == 2);
	add(7, 1);
	ck_assert(get(7) == 3);
	add(7, 8);
	ck_assert(get(7) == 11);
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
		add(i, (double)(n - i));
	}
	sort();
}
END_TEST


START_TEST(test_sort_reversed)
{
	int i, n = 100;
	for (i = 0; i < n; i++) {
		add(i, (double)i);
	}
	sort();
}
END_TEST


START_TEST(test_sort_duplicates)
{
	add(7, 2);
	ck_assert(get(7) == 2);
	add(7, 1);
	ck_assert(get(7) == 3);
	add(7, 8);
	ck_assert(get(7) == 11);
	sort();
	ck_assert(get(7) == 11);
}
END_TEST


START_TEST(test_sort_random)
{
	int seed, nseed = 50;
	int i, ind;
	double val;

	for (seed = 0; seed < nseed; seed++) {
		srand((unsigned)seed);
		clear();

		for (i = 0; i < 2 * nseed; i++) {
			// choose an index in [0,nseed)
			ind = (int)rand() % nseed;
			// choose a value in [-10, 10]
			val = (double)(rand() % 21) - 10;
			add(ind, val);
		}
		sort();
	}
}
END_TEST


Suite *census_suite(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("census");

	tc = tcase_create("add");
	tcase_add_checked_fixture(tc, setup_census, teardown_census);
	tcase_add_test(tc, test_init);
	tcase_add_test(tc, test_add);
	tcase_add_test(tc, test_add_duplicates);
	suite_add_tcase(s, tc);

	tc = tcase_create("sort");
	tcase_add_checked_fixture(tc, setup_census, teardown_census);
	tcase_add_test(tc, test_sort_empty);
	tcase_add_test(tc, test_sort_ordered);
	tcase_add_test(tc, test_sort_reversed);
	tcase_add_test(tc, test_sort_duplicates);
	tcase_add_test(tc, test_sort_random);
	suite_add_tcase(s, tc);

	return s;
}


int main(void)
{
	int nfail;
	Suite *s;
	SRunner *sr;

	s = census_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	nfail = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (nfail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
