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
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "../src/text.h"
#include "../src/tree.h"
#include "../src/sentscan.h"
#include "../src/sentfilter.h"
#include "testutil.h"

#define SENT_EOT ((const struct corpus_text *)&sent_eot)

static struct corpus_sentfilter sentfilter;
static struct corpus_text sent_eot;
static int has_sentfilter;


static void setup_sentfilter(void)
{
	setup();
	has_sentfilter = 0;
	sent_eot.ptr = (uint8_t *)"<eot>";
	sent_eot.attr = strlen("<eot>");
}


static void teardown_sentfilter(void)
{
	if (has_sentfilter) {
		corpus_sentfilter_destroy(&sentfilter);
		has_sentfilter = 0;
	}
	teardown();
}


static void init(void)
{
	ck_assert(!has_sentfilter);
	ck_assert(!corpus_sentfilter_init(&sentfilter));
	has_sentfilter = 1;
}


static void suppress(const struct corpus_text *pattern)
{
	ck_assert(has_sentfilter);
	ck_assert(!corpus_sentfilter_suppress(&sentfilter, pattern));
}


static void start(const struct corpus_text *text)
{
	ck_assert(!corpus_sentfilter_start(&sentfilter, text));
}


static const struct corpus_text *next(void)
{
	int has = corpus_sentfilter_advance(&sentfilter);

	ck_assert(!sentfilter.error);
	if (!has) {
		return &sent_eot;
	} else {
		return &sentfilter.current;
	}
}


START_TEST(test_space)
{
	init();
	start(T("Mr. Jones."));
	assert_text_eq(next(), T("Mr. "));
	assert_text_eq(next(), T("Jones."));
	assert_text_eq(next(), SENT_EOT);
}
END_TEST


START_TEST(test_newline)
{
	init();
	start(T("Mr.\nJones."));
	assert_text_eq(next(), T("Mr.\n"));
	assert_text_eq(next(), T("Jones."));
	assert_text_eq(next(), SENT_EOT);
}
END_TEST


START_TEST(test_suppress)
{
	init();
	suppress(T("Mr."));
	suppress(T("Mrs."));
	suppress(T("Mx."));

	start(T("Mr. and Mrs. Jones."));
	assert_text_eq(next(), T("Mr. and Mrs. Jones."));
	assert_text_eq(next(), SENT_EOT);

	start(T("Mx. Jones."));
	assert_text_eq(next(), T("Mx. Jones."));
	assert_text_eq(next(), SENT_EOT);
}
END_TEST


START_TEST(test_nonsuppress)
{
	init();
	suppress(T("Mx."));

	start(T("AMx. Split."));
	assert_text_eq(next(), T("AMx. "));
	assert_text_eq(next(), T("Split."));
	assert_text_eq(next(), SENT_EOT);
}
END_TEST


START_TEST(test_suppress_break)
{
	init();
	suppress(T("Mx."));

	start(T("end.\nMx. Jones."));
	assert_text_eq(next(), T("end.\n"));
	assert_text_eq(next(), T("Mx. Jones."));
	assert_text_eq(next(), SENT_EOT);

	start(T("end.\r\nMx. Jones."));
	assert_text_eq(next(), T("end.\r\n"));
	assert_text_eq(next(), T("Mx. Jones."));
	assert_text_eq(next(), SENT_EOT);

	start(T("end.\rMx. Jones."));
	assert_text_eq(next(), T("end.\r"));
	assert_text_eq(next(), T("Mx. Jones."));
	assert_text_eq(next(), SENT_EOT);

	start(T("end?Mx. Jones."));
	assert_text_eq(next(), T("end?"));
	assert_text_eq(next(), T("Mx. Jones."));
	assert_text_eq(next(), SENT_EOT);

	start(T("end.Mx. Jones."));
	assert_text_eq(next(), T("end.Mx. Jones."));
	assert_text_eq(next(), SENT_EOT);
}
END_TEST


Suite *sentfilter_suite(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("sentfilter");

	tc = tcase_create("basic");
	tcase_add_checked_fixture(tc, setup_sentfilter, teardown_sentfilter);
        tcase_add_test(tc, test_space);
        tcase_add_test(tc, test_newline);
        tcase_add_test(tc, test_suppress);
        tcase_add_test(tc, test_nonsuppress);
        tcase_add_test(tc, test_suppress_break);
	suite_add_tcase(s, tc);

	return s;
}


int main(void)
{
	int nfail;
	Suite *s;
	SRunner *sr;

	s = sentfilter_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	nfail = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (nfail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
