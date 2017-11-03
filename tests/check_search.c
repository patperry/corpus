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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <check.h>
#include "../lib/utf8lite/src/utf8lite.h"
#include "../src/table.h"
#include "../src/tree.h"
#include "../src/termset.h"
#include "../src/textset.h"
#include "../src/stem.h"
#include "../src/symtab.h"
#include "../src/wordscan.h"
#include "../src/filter.h"
#include "../src/search.h"
#include "testutil.h"


struct corpus_filter filter;
struct corpus_search search;
struct utf8lite_text search_text;
int offset;
int size;


static void setup_search(void)
{
	setup();
	ck_assert(!corpus_filter_init(&filter, CORPUS_FILTER_KEEP_ALL,
				      UTF8LITE_TEXTMAP_CASE,
				      CORPUS_FILTER_CONNECTOR, NULL, NULL));
	ck_assert(!corpus_search_init(&search));
}


static void teardown_search(void)
{
	corpus_search_destroy(&search);
	corpus_filter_destroy(&filter);
	teardown();
}


static int add(const struct utf8lite_text *term)
{
	int type_ids[256];
	int length, term_id;

	ck_assert(!corpus_filter_start(&filter, term));
	length = 0;
	while (corpus_filter_advance(&filter)) {
		if (filter.type_id < 0) {
			continue;
		}

		ck_assert(length < 256);
		type_ids[length] = filter.type_id;
		length++;
	}

	ck_assert(!corpus_search_add(&search, type_ids, length, &term_id));
	return term_id;
}


static void start(const struct utf8lite_text *text)
{
	ck_assert(!corpus_search_start(&search, text, &filter));
	search_text = *text;
}


static int next(void)
{
	int id;

	if (corpus_search_advance(&search)) {
		id = search.term_id;
		offset = (int)(search.current.ptr - search_text.ptr);
		size = (int)UTF8LITE_TEXT_SIZE(&search.current);
	} else {
		ck_assert(!search.error);
		id = -1;
		offset = 0;
		size = 0;
	}

	return id;
}


START_TEST(test_unigram)
{
	int a;

	a = add(T("a"));
	start(T("a b c a a"));

	ck_assert_int_eq(next(), a);
	ck_assert_int_eq(offset, 0);
	ck_assert_int_eq(size, 1);

	ck_assert_int_eq(next(), a);
	ck_assert_int_eq(offset, strlen("a b c "));
	ck_assert_int_eq(size, 1);

	ck_assert_int_eq(next(), a);
	ck_assert_int_eq(offset, strlen("a b c a "));
	ck_assert_int_eq(size, 1);

	ck_assert_int_eq(next(), -1);
}
END_TEST


START_TEST(test_bigram)
{
	int ab = add(T("a b"));

	start(T("b a b c a b a"));

	ck_assert_int_eq(next(), ab);
	ck_assert_int_eq(offset, strlen("b "));
	ck_assert_int_eq(size, 3);

	ck_assert_int_eq(next(), ab);
	ck_assert_int_eq(offset, strlen("b a b c "));
	ck_assert_int_eq(size, 3);

	ck_assert_int_eq(next(), -1);
}
END_TEST


START_TEST(test_subterm)
{
	int ab = add(T("a b"));
	int a = add(T("a"));
	int b = add(T("b"));

	start(T("c b a b a b b b c"));

	ck_assert_int_eq(next(), b);
	ck_assert_int_eq(offset, strlen("c "));
	ck_assert_int_eq(size, 1);

	ck_assert_int_eq(next(), a);
	ck_assert_int_eq(offset, strlen("c b "));
	ck_assert_int_eq(size, 1);

	ck_assert_int_eq(next(), ab);
	ck_assert_int_eq(offset, strlen("c b "));
	ck_assert_int_eq(size, 3);

	ck_assert_int_eq(next(), b);
	ck_assert_int_eq(offset, strlen("c b a "));
	ck_assert_int_eq(size, 1);

	ck_assert_int_eq(next(), a);
	ck_assert_int_eq(offset, strlen("c b a b "));
	ck_assert_int_eq(size, 1);

	ck_assert_int_eq(next(), ab);
	ck_assert_int_eq(offset, strlen("c b a b "));
	ck_assert_int_eq(size, 3);

	ck_assert_int_eq(next(), b);
	ck_assert_int_eq(offset, strlen("c b a b a "));
	ck_assert_int_eq(size, 1);

	ck_assert_int_eq(next(), b);
	ck_assert_int_eq(offset, strlen("c b a b a b "));
	ck_assert_int_eq(size, 1);

	ck_assert_int_eq(next(), b);
	ck_assert_int_eq(offset, strlen("c b a b a b b "));
	ck_assert_int_eq(size, 1);

	ck_assert_int_eq(next(), -1);
}
END_TEST


START_TEST(test_overlap)
{
	int ab = add(T("a b"));
	int bc = add(T("b c"));

	start(T("a b c"));

	ck_assert_int_eq(next(), ab);
	ck_assert_int_eq(offset, 0);
	ck_assert_int_eq(size, 3);

	ck_assert_int_eq(next(), bc);
	ck_assert_int_eq(offset, strlen("a "));
	ck_assert_int_eq(size, 3);
}
END_TEST


Suite *search_suite(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("search");

	tc = tcase_create("basic");
	tcase_add_checked_fixture(tc, setup_search, teardown_search);
        tcase_add_test(tc, test_unigram);
        tcase_add_test(tc, test_bigram);
        tcase_add_test(tc, test_subterm);
        tcase_add_test(tc, test_overlap);
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

