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
#include <stdint.h>
#include <stdlib.h>
#include "../src/table.h"
#include "../src/text.h"
#include "../src/textset.h"
#include "../src/tree.h"
#include "../src/typemap.h"
#include "../src/symtab.h"
#include "../src/wordscan.h"
#include "../src/filter.h"
#include "testutil.h"

#define IGNORE_EMPTY CORPUS_FILTER_IGNORE_EMPTY

struct corpus_text dropped;
const struct corpus_text *DROPPED;
struct corpus_filter filter;
int has_filter;

static void setup_filter(void)
{
	setup();
	has_filter = 0;
	dropped.ptr = (uint8_t *)"<drop>";
	dropped.attr = 0;
	DROPPED = &dropped;
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
	int type_kind = (CORPUS_TYPE_COMPAT | CORPUS_TYPE_CASEFOLD
			 | CORPUS_TYPE_DASHFOLD | CORPUS_TYPE_QUOTFOLD
			 | CORPUS_TYPE_RMCC | CORPUS_TYPE_RMDI
			 | CORPUS_TYPE_RMWS);

	ck_assert(!has_filter);
	ck_assert(!corpus_filter_init(&filter, type_kind, stemmer, flags));
	has_filter = 1;
}


static void start(const struct corpus_text *text)
{
	ck_assert(!corpus_filter_start(&filter, text));
}


static const struct corpus_text *next(void)
{
	int term_id, type_id;
	int has = corpus_filter_advance(&filter, &term_id);

	if (has) {
		if (term_id < 0) {
			return DROPPED;
		}
		ck_assert_int_lt(term_id, filter.nterm);
		type_id = filter.type_ids[term_id];
		ck_assert_int_le(0, type_id);
		ck_assert_int_lt(type_id, filter.symtab.ntype);
		return &filter.symtab.types[type_id].text;
	} else {
		ck_assert(!filter.error);
		return NULL;
	}
}


START_TEST(test_basic)
{
	init(NULL, IGNORE_EMPTY);

	start(T("A rose is a rose is a rose."));
	assert_text_eq(next(), T("a"));
	assert_text_eq(next(), T("rose"));
	assert_text_eq(next(), T("is"));
	assert_text_eq(next(), T("a"));
	assert_text_eq(next(), T("rose"));
	assert_text_eq(next(), T("is"));
	assert_text_eq(next(), T("a"));
	assert_text_eq(next(), T("rose"));
	assert_text_eq(next(), T("."));
	assert_text_eq(next(), NULL);
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

