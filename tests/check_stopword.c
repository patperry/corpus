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

#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include "../lib/utf8lite/src/utf8lite.h"
#include "../src/stopword.h"
#include "testutil.h"


START_TEST(test_stopwords_en)
{
	int len;
	const uint8_t **words = corpus_stopword_list("english", &len);

	ck_assert_int_eq(len, 175);
	assert_text_eq(S((char *)words[0]), S("a"));
	assert_text_eq(S((char *)words[len - 1]), S("yourselves"));
	ck_assert(words[len] == NULL);
}
END_TEST


START_TEST(test_stopwords_unknown)
{
	int len;
	const uint8_t **words = corpus_stopword_list("gibberish", &len);

	ck_assert(words == NULL);
	ck_assert_int_eq(len, 0);
}
END_TEST


Suite *stopword_suite(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("stopword");
	tc = tcase_create("basic");
	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_test(tc, test_stopwords_en);
	tcase_add_test(tc, test_stopwords_unknown);
	suite_add_tcase(s, tc);

	return s;
}


int main(void)
{
        int number_failed;
        Suite *s;
        SRunner *sr;

        s = stopword_suite();
        sr = srunner_create(s);

        srunner_run_all(sr, CK_NORMAL);
        number_failed = srunner_ntests_failed(sr);
        srunner_free(sr);

        return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
