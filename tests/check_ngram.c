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
#include <string.h>
#include <check.h>
#include "../src/table.h"
#include "../src/ngram.h"
#include "testutil.h"

struct corpus_ngram ngram;
struct corpus_ngram_iter iter;
int has_ngram;
int has_iter;

char buffer[128];

void setup_ngram(void)
{
	setup();
	has_ngram = 0;
	has_iter = 0;
}


void teardown_ngram(void)
{
	if (has_ngram) {
		corpus_ngram_destroy(&ngram);
		has_ngram = 0;
		has_iter = 0;
	}
	teardown();
}


void init(int width)
{
	ck_assert(!has_ngram);
	ck_assert(!corpus_ngram_init(&ngram, width));
	has_ngram = 1;
}


void add_weight(char c, double weight)
{
	ck_assert(has_ngram);
	ck_assert(!corpus_ngram_add(&ngram, (int)c, weight));
}


void add(char c)
{
	add_weight(c, 1);
}


void break_(void)
{
	ck_assert(has_ngram);
	ck_assert(!corpus_ngram_break(&ngram));
}


double weight(const char *term)
{
	int buffer[16];
	int width = (int)strlen(term);
	double w;
	int k;

	ck_assert(has_ngram);

	for (k = 0; k < width; k++) {
		buffer[k] = (int)term[k];
	}
	if (corpus_ngram_has(&ngram, buffer, width, &w)) {
		return w;
	} else {
		return 0;
	}
}


void start(int width)
{
	ck_assert(has_ngram);
	corpus_ngram_iter_make(&iter, &ngram, width);
	has_iter = 1;
}


const char *next(void)
{
	int k;

	ck_assert(has_iter);

	if (corpus_ngram_iter_advance(&iter)) {
		for (k = 0; k < iter.width; k++) {
			buffer[k] = (char)iter.type_ids[k];
		}
		buffer[k] = '\0';
		return buffer;
	} else {
		return NULL;
	}
}


int count(int width)
{
	ck_assert(has_ngram);
	return corpus_ngram_count(&ngram, width);
}


START_TEST(test_unigram_init)
{
	init(1);
	ck_assert_int_eq(count(1), 0);
	ck_assert_int_eq(count(2), 0);
	ck_assert(weight("a") == 0);
	ck_assert(weight("ab") == 0);
}
END_TEST


START_TEST(test_unigram_add1)
{
	init(1);
	add('z');
	ck_assert(weight("z") == 1);
	ck_assert_int_eq(count(1), 1);
}
END_TEST


START_TEST(test_unigram_add2)
{
	init(1);

	add('a');
	add('b');
	ck_assert(weight("a") == 1);
	ck_assert(weight("b") == 1);
	ck_assert_int_eq(count(1), 2);

	add_weight('b', 3.0);
	ck_assert(weight("b") == 4);
	ck_assert(weight("a") == 1);
	ck_assert_int_eq(count(1), 2);
}
END_TEST


START_TEST(test_unigram_iter)
{
	init(1);
	add('a');
	add('b');
	add('c');
	add('b');
	add('c');

	start(0);
	ck_assert(next() == NULL);

	start(1);
	ck_assert_str_eq(next(), "a");
	ck_assert(iter.weight == 1);
	ck_assert_str_eq(next(), "b");
	ck_assert(iter.weight == 2);
	ck_assert_str_eq(next(), "c");
	ck_assert(iter.weight == 2);
	ck_assert(next() == NULL);

	start(2);
	ck_assert(next() == NULL);
}
END_TEST



START_TEST(test_bigram_add2)
{
	init(2);
	add('x');
	add('y');

	ck_assert(weight("x") == 1);
	ck_assert(weight("y") == 1);
	ck_assert(weight("xy") == 1);
	ck_assert_int_eq(count(1), 2);
	ck_assert_int_eq(count(2), 1);
}
END_TEST


START_TEST(test_bigram_add3)
{
	init(2);
	add('x');
	add('y');
	add('y');

	ck_assert(weight("x") == 1);
	ck_assert(weight("y") == 2);
	ck_assert(weight("xy") == 1);
	ck_assert(weight("yy") == 1);
	ck_assert_int_eq(count(1), 2);
	ck_assert_int_eq(count(2), 2);
}
END_TEST


START_TEST(test_bigram_add5)
{
	init(2);
	add('x');
	add('y');
	add('y');
	add('y');
	add('x');

	ck_assert(weight("x") == 2);
	ck_assert(weight("y") == 3);
	ck_assert(weight("xy") == 1);
	ck_assert(weight("yy") == 2);
	ck_assert(weight("yx") == 1);
	ck_assert_int_eq(count(1), 2);
	ck_assert_int_eq(count(2), 3);
}
END_TEST


START_TEST(test_bigram_break)
{
	init(2);

	add('x');
	add('y');
	break_();
	add('z');

	ck_assert_int_eq(count(1), 3);
	ck_assert(weight("x") == 1);
	ck_assert(weight("y") == 1);
	ck_assert(weight("z") == 1);

	ck_assert_int_eq(count(2), 1);
	ck_assert(weight("xy") == 1);
	ck_assert(weight("yz") == 0);
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
        tcase_add_test(tc, test_unigram_add2);
        tcase_add_test(tc, test_unigram_iter);
        suite_add_tcase(s, tc);

	tc = tcase_create("bigram");
        tcase_add_checked_fixture(tc, setup_ngram, teardown_ngram);
        tcase_add_test(tc, test_bigram_add2);
        tcase_add_test(tc, test_bigram_add3);
        tcase_add_test(tc, test_bigram_add5);
        tcase_add_test(tc, test_bigram_break);
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
