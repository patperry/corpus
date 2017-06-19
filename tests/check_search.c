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

#include <stdlib.h>
#include <check.h>
#include "../src/table.h"
#include "../src/tree.h"
#include "../src/termset.h"
#include "../src/text.h"
#include "../src/search.h"
#include "testutil.h"

struct corpus_search search;

static void setup_search(void)
{
	setup();
	ck_assert(!corpus_search_init(&search));
}


static void teardown_search(void)
{
	corpus_search_destroy(&search);
	teardown();
}


START_TEST(test_basic)
{
}
END_TEST


Suite *search_suite(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("search");

	tc = tcase_create("basic");
	tcase_add_checked_fixture(tc, setup_search, teardown_search);
        tcase_add_test(tc, test_basic);
	suite_add_tcase(s, tc);

	return s;
}


int main(void)
{
	int nfail;
	Suite *s;
	SRunner *sr;

	s = search_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	nfail = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (nfail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

