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
#include "../src/census.h"
#include "testutil.h"

struct census census;

void setup_census(void)
{
	setup();
	census_init(&census);
}


void teardown_census(void)
{
	census_destroy(&census);
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

	if (census_has(&census, i, &val)) {
		ck_assert_double_eq(val,  val0);
	} else {
		ck_assert_double_eq(0.0,  val0);
	}

	return val;
}


void add(int i, double x)
{
	double oldval, newval;

	oldval = get(i);
	ck_assert(!census_add(&census, i, x));
	newval = get(i);

	ck_assert_double_eq(newval, oldval + x);
}


void clear()
{
	census_clear(&census);
	ck_assert_int_eq(census.nitem, 0);
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


Suite *census_suite(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("census");
	tc = tcase_create("core");
	tcase_add_checked_fixture(tc, setup_census, teardown_census);
        tcase_add_test(tc, test_init);
        tcase_add_test(tc, test_add);
        tcase_add_test(tc, test_add_duplicates);
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
