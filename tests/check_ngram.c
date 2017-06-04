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
#include <string.h>
#include <check.h>
#include "../src/table.h"
#include "../src/tree.h"
#include "../src/ngram.h"
#include "testutil.h"

struct corpus_ngram ngram;
//struct corpus_ngram_iter iter;
int has_ngram;
int has_iter;
int iter_width;

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


void clear(void)
{
	ck_assert(has_ngram);
	corpus_ngram_clear(&ngram);
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


/*
void start(int width)
{
	ck_assert(has_ngram);
	//corpus_ngram_iter_make(&iter, &ngram, width);
	has_iter = 1;
	iter_width = width;
}


const char *next(void)
{
	int k;

	ck_assert(has_iter);

	if (corpus_ngram_iter_advance(&iter)) {
		for (k = 0; k < iter_width; k++) {
			buffer[k] = (char)iter.type_ids[k];
		}
		buffer[k] = '\0';
		return buffer;
	} else {
		return NULL;
	}
}

*/


int count(void)
{
	ck_assert(has_ngram);
	return ngram.terms.nnode;
}


START_TEST(test_unigram_init)
{
	init(1);
	ck_assert_int_eq(count(), 0);
	ck_assert(weight("a") == 0);
	ck_assert(weight("ab") == 0);
}
END_TEST


START_TEST(test_unigram_add1)
{
	init(1);
	add('z');
	ck_assert(weight("z") == 1);
	ck_assert_int_eq(count(), 1);
}
END_TEST


START_TEST(test_unigram_add2)
{
	init(1);

	add('a');
	add('b');
	ck_assert(weight("a") == 1);
	ck_assert(weight("b") == 1);
	ck_assert_int_eq(count(), 2);

	add_weight('b', 3.0);
	ck_assert(weight("b") == 4);
	ck_assert(weight("a") == 1);
	ck_assert_int_eq(count(), 2);
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

	/*
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
	*/
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
	ck_assert_int_eq(count(), 3);
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
	ck_assert_int_eq(count(), 4);
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
	ck_assert_int_eq(count(), 5);
}
END_TEST


START_TEST(test_bigram_break)
{
	init(2);

	add('x');
	add('y');
	break_();
	add('z');

	ck_assert(weight("x") == 1);
	ck_assert(weight("y") == 1);
	ck_assert(weight("z") == 1);
	ck_assert(weight("xy") == 1);
	ck_assert(weight("yz") == 0);
	ck_assert_int_eq(count(), 4);
}
END_TEST


START_TEST(test_bigram_iter)
{
	init(2);
	add('a');
	add('a');
	add('a');
	add('b');
	add('a');
	add('b');

	/*
	start(0);
	ck_assert(next() == NULL);

	start(1);
	ck_assert_str_eq(next(), "a");
	ck_assert(iter.weight == 4);
	ck_assert_str_eq(next(), "b");
	ck_assert(iter.weight == 2);
	ck_assert(next() == NULL);

	start(2);
	ck_assert_str_eq(next(), "aa");
	ck_assert(iter.weight == 2);
	ck_assert_str_eq(next(), "ab");
	ck_assert(iter.weight == 2);
	ck_assert_str_eq(next(), "ba");
	ck_assert(iter.weight == 1);
	ck_assert(next() == NULL);

	start(3);
	ck_assert(next() == NULL);
	*/
}
END_TEST


START_TEST(test_bigram_clear)
{
	init(2);
	add('a');
	add('a');
	add('a');
	add('a');
	clear();
	add('a');
	add('a');

	ck_assert(weight("a") == 2);
	ck_assert(weight("aa") == 1);
	ck_assert_int_eq(count(), 2);
}
END_TEST


START_TEST(test_trigram_random)
{
	double count3[10][10][10];
	double count2[10][10];
	double count1[10];
	double w;
	int buf[3];
	int a, nadd = 2000;
	int key, b, nbuf, n;
	int i, j, k;

	srand(0);
	init(3);
	n = 10;

	for (i = 0; i < n; i++) {
		count1[i] = 0;
	}

	for (j = 0; j < n; j++) {
		for (i = 0; i < n; i++) {
			count2[i][j] = 0;
		}
	}

	for (k = 0; k < n; k++) {
		for (j = 0; j < n; j++) {
			for (i = 0; i < n; i++) {
				count3[i][j][k] = 0;
			}
		}
	}

	nbuf = 0;
	for (a = 0; a < nadd; a++) {
		key = rand() % n;
		w = (double)(rand() % 3);

		ck_assert(!corpus_ngram_add(&ngram, key, w));

		if (nbuf == 3) {
			for (b = 1; b < nbuf; b++) {
				buf[b - 1] = buf[b];
			}
			nbuf--;
		}
		buf[nbuf] = key;
		nbuf++;

		count1[buf[nbuf-1]] += w;
		if (nbuf > 1) {
			count2[buf[nbuf-2]][buf[nbuf-1]] += w;
		}
		if (nbuf > 2) {
			count3[buf[nbuf-3]][buf[nbuf-2]][buf[nbuf-1]] += w;
		}
	}

	for (i = 0; i < n; i++) {
		buf[0] = i;
		if (corpus_ngram_has(&ngram, buf, 1, &w)) {
			ck_assert(count1[i] == w);
		} else {
			ck_assert(count1[i] == 0);
		}
	}

	for (j = 0; j < n; j++) {
		for (i = 0; i < n; i++) {
			buf[0] = i;
			buf[1] = j;
			if (corpus_ngram_has(&ngram, buf, 2, &w)) {
				ck_assert(count2[i][j] == w);
			} else {
				ck_assert(count2[i][j] == 0);
			}
		}
	}

	for (k = 0; k < n; k++) {
		for (j = 0; j < n; j++) {
			for (i = 0; i < n; i++) {
				buf[0] = i;
				buf[1] = j;
				buf[2] = k;
				if (corpus_ngram_has(&ngram, buf, 3, &w)) {
					ck_assert(count3[i][j][k] == w);
				} else {
					ck_assert(count3[i][j][k] == 0);
				}
			}
		}
	}
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
        tcase_add_test(tc, test_bigram_iter);
        tcase_add_test(tc, test_bigram_clear);
        suite_add_tcase(s, tc);

	tc = tcase_create("trigram");
        tcase_add_checked_fixture(tc, setup_ngram, teardown_ngram);
        tcase_add_test(tc, test_trigram_random);
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
