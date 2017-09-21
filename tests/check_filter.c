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
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../src/table.h"
#include "../src/text.h"
#include "../src/textset.h"
#include "../src/tree.h"
#include "../src/typemap.h"
#include "../src/symtab.h"
#include "../src/wordscan.h"
#include "../src/filter.h"
#include "../src/census.h"
#include "testutil.h"

#define IGNORE_SPACE CORPUS_FILTER_IGNORE_SPACE
#define DROP_LETTER CORPUS_FILTER_DROP_LETTER
#define DROP_MARK CORPUS_FILTER_DROP_MARK
#define DROP_NUMBER CORPUS_FILTER_DROP_NUMBER
#define DROP_PUNCT CORPUS_FILTER_DROP_PUNCT
#define DROP_SYMBOL CORPUS_FILTER_DROP_SYMBOL
#define DROP_OTHER CORPUS_FILTER_DROP_OTHER

#define ID_EOT	  (-1)
#define ID_IGNORE (-2)
#define ID_DROP	  (-3)
#define TYPE_EOT    ((const struct corpus_text *)&type_eot)
#define TYPE_IGNORE ((const struct corpus_text *)&type_ignore)
#define TYPE_DROP   ((const struct corpus_text *)&type_drop)

static struct corpus_text type_eot, type_ignore, type_drop;
static struct corpus_filter filter;
static int has_filter;


static void setup_filter(void)
{
	setup();
	has_filter = 0;
	type_eot.ptr = (uint8_t *)"<eot>";
	type_eot.attr = strlen("<eot>");
	type_ignore.ptr = (uint8_t *)"<ignore>";
	type_ignore.attr = strlen("<ignore>");
	type_drop.ptr = (uint8_t *)"<drop>";
	type_drop.attr = strlen("<drop>");
}


static void teardown_filter(void)
{
	if (has_filter) {
		corpus_filter_destroy(&filter);
		has_filter = 0;
	}
	teardown();
}


static void init(const char *stemmer, int flags)
{
	int type_kind = (CORPUS_TYPE_MAPCASE | CORPUS_TYPE_MAPCOMPAT
			 | CORPUS_TYPE_MAPQUOTE | CORPUS_TYPE_RMDI);

	ck_assert(!has_filter);
	ck_assert(!corpus_filter_init(&filter, type_kind, stemmer, flags));
	has_filter = 1;
}


static void start(const struct corpus_text *text)
{
	ck_assert(!corpus_filter_start(&filter, text,
				       CORPUS_FILTER_SCAN_TOKENS));
}


static void combine(const struct corpus_text *text)
{
	ck_assert(!corpus_filter_combine(&filter, text));
}

static int next_id(void)
{
	int type_id, symbol_id;
	int has = corpus_filter_advance(&filter);

	ck_assert(!filter.error);

	if (has) {
		type_id = filter.type_id;
		if (type_id == CORPUS_FILTER_NONE) {
			return ID_IGNORE;
		} else if (type_id < 0) {
			return ID_DROP;
		}
		ck_assert(type_id < filter.ntype);
		symbol_id = filter.symbol_ids[type_id];
		ck_assert(0 <= symbol_id);
		ck_assert(symbol_id < filter.symtab.ntype);
		return type_id;
	} else {
		return ID_EOT;
	}
}


static const struct corpus_text *next_type(void)
{
	int type_id = next_id();
	int symbol_id;

	switch (type_id) {
	case ID_EOT:
		return TYPE_EOT;
	case ID_IGNORE:
		return TYPE_IGNORE;
	case ID_DROP:
		return TYPE_DROP;
	default:
		symbol_id = filter.symbol_ids[type_id];
		return &filter.symtab.types[symbol_id].text;
	}
}


static const struct corpus_text *token(void)
{
	return &filter.current;
}


START_TEST(test_basic)
{
	init(NULL, IGNORE_SPACE);

	start(T("A rose is a Rose is a ROSE."));

	assert_text_eq(next_type(), T("a"));
	assert_text_eq(token(), T("A"));

	assert_text_eq(next_type(), TYPE_IGNORE);
	assert_text_eq(token(), T(" "));

	assert_text_eq(next_type(), T("rose"));
	assert_text_eq(token(), T("rose"));

	assert_text_eq(next_type(), TYPE_IGNORE);
	assert_text_eq(token(), T(" "));

	assert_text_eq(next_type(), T("is"));
	assert_text_eq(token(), T("is"));

	assert_text_eq(next_type(), TYPE_IGNORE);
	assert_text_eq(token(), T(" "));

	assert_text_eq(next_type(), T("a"));
	assert_text_eq(token(), T("a"));

	assert_text_eq(next_type(), TYPE_IGNORE);
	assert_text_eq(token(), T(" "));

	assert_text_eq(next_type(), T("rose"));
	assert_text_eq(token(), T("Rose"));

	assert_text_eq(next_type(), TYPE_IGNORE);
	assert_text_eq(token(), T(" "));

	assert_text_eq(next_type(), T("is"));
	assert_text_eq(token(), T("is"));

	assert_text_eq(next_type(), TYPE_IGNORE);
	assert_text_eq(token(), T(" "));

	assert_text_eq(next_type(), T("a"));
	assert_text_eq(token(), T("a"));

	assert_text_eq(next_type(), TYPE_IGNORE);
	assert_text_eq(token(), T(" "));

	assert_text_eq(next_type(), T("rose"));
	assert_text_eq(token(), T("ROSE"));

	assert_text_eq(next_type(), T("."));
	assert_text_eq(token(), T("."));

	assert_text_eq(next_type(), TYPE_EOT);
}
END_TEST


START_TEST(test_combine)
{
	init(NULL, IGNORE_SPACE | DROP_PUNCT);
	combine(T("new york"));
	combine(T("new york city"));

	start(T("New York City, New York."));

	assert_text_eq(next_type(), T("new york city"));
	assert_text_eq(token(), T("New York City"));

	assert_text_eq(next_type(), TYPE_DROP);
	assert_text_eq(token(), T(","));

	assert_text_eq(next_type(), TYPE_IGNORE);
	assert_text_eq(token(), T(" "));

	assert_text_eq(next_type(), T("new york"));
	assert_text_eq(token(), T("New York"));

	assert_text_eq(next_type(), TYPE_DROP);
	assert_text_eq(token(), T("."));

	assert_text_eq(next_type(), TYPE_EOT);
}
END_TEST


START_TEST(test_basic_census)
{
	struct corpus_census census;
	int type_id;

	ck_assert(!corpus_census_init(&census));

	init(NULL, IGNORE_SPACE);
	start(T("A rose is a rose is a rose."));

	while ((type_id = next_id()) != ID_EOT) {
		if (type_id < 0) {
			continue;
		}

		ck_assert(!corpus_census_add(&census, type_id, 1));
	}

	ck_assert(!corpus_census_sort(&census));

	ck_assert_int_eq(census.nitem, 4);
	ck_assert_int_eq(census.items[0], 0); // a
	ck_assert(census.weights[0] == 3);
	ck_assert_int_eq(census.items[1], 1); // rose
	ck_assert(census.weights[1] == 3);
	ck_assert_int_eq(census.items[2], 2); // is
	ck_assert(census.weights[2] == 2);
	ck_assert_int_eq(census.items[3], 3); // .
	ck_assert(census.weights[3] == 1);

	corpus_census_destroy(&census);
}
END_TEST


START_TEST(test_drop_ideo)
{
	init(NULL, DROP_LETTER);
	start(T("\\u53d1\\u5c55"));
	assert_text_eq(next_type(), TYPE_DROP);
	assert_text_eq(next_type(), TYPE_DROP);
	assert_text_eq(next_type(), TYPE_EOT);
}
END_TEST

START_TEST(test_url)
{
	init(NULL, IGNORE_SPACE);
	start(T("http PTRCKPRRY http://www.PTRCKPRRY.com/"));
	assert_text_eq(next_type(), T("http"));
	assert_text_eq(next_type(), TYPE_IGNORE);
	assert_text_eq(next_type(), T("ptrckprry"));
	assert_text_eq(next_type(), TYPE_IGNORE);
	assert_text_eq(next_type(), T("http://www.ptrckprry.com/"));
	assert_text_eq(next_type(), TYPE_EOT);
}
END_TEST


START_TEST(test_hashtag)
{
	init(NULL, IGNORE_SPACE);
	start(T("#useR2017"));
	assert_text_eq(next_type(), T("#user2017"));
	assert_text_eq(next_type(), TYPE_EOT);
}
END_TEST


START_TEST(test_mention)
{
	init(NULL, IGNORE_SPACE);
	start(T("@PtrckPrry"));
	assert_text_eq(next_type(), T("@ptrckprry"));
	assert_text_eq(next_type(), TYPE_EOT);
}
END_TEST



Suite *filter_suite(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("filter");

	tc = tcase_create("basic");
	tcase_add_checked_fixture(tc, setup_filter, teardown_filter);
        tcase_add_test(tc, test_basic);
        tcase_add_test(tc, test_combine);
        tcase_add_test(tc, test_basic_census);
        tcase_add_test(tc, test_drop_ideo);
        tcase_add_test(tc, test_url);
        tcase_add_test(tc, test_hashtag);
        tcase_add_test(tc, test_mention);
	suite_add_tcase(s, tc);

	return s;
}


int main(void)
{
	int nfail;
	Suite *s;
	SRunner *sr;

	s = filter_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	nfail = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (nfail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

