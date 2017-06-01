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
#include "../src/census.h"
#include "../src/ngram.h"
#include "testutil.h"

struct corpus_ngram ngram;
int has_ngram;


void setup_ngram(void)
{
	setup();
	has_ngram = 0;
}


void teardown_ngram(void)
{
	if (has_ngram) {
		corpus_ngram_destroy(&ngram);
		has_ngram = 0;
	}
	teardown();
}


void init(int width)
{
	ck_assert(!has_ngram);
	ck_assert(!corpus_ngram_init(&ngram, width));
	has_ngram = 1;
}


START_TEST(test_unigram_init)
{
	init(1);
	ck_assert_int_eq(ngram.width, 1);
	ck_assert_int_eq(ngram.terms[0].nterm, 0);
	ck_assert_int_eq(ngram.terms[0].census.nitem, 0);
}
END_TEST



START_TEST(test_unigram_add1)
{
	init(1);
}
END_TEST


Suite *ngram_suite(void)
{
        Suite *s;
        TCase *tc;

        s = suite_create("ngram");
	tc = tcase_create("unigram");
        tcase_add_checked_fixture(tc, setup_ngram, teardown_ngram);
        tcase_add_test(tc, test_unigram_init);
        tcase_add_test(tc, test_unigram_add1);
        suite_add_tcase(s, tc);

	return s;
}


int main(void)
{
        int number_failed;
        Suite *s;
        SRunner *sr;

        s = ngram_suite();
        sr = srunner_create(s);

        srunner_run_all(sr, CK_NORMAL);
        number_failed = srunner_ntests_failed(sr);
        srunner_free(sr);

        return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
