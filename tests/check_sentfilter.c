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


static void start(const struct corpus_text *text)
{
	ck_assert(!corpus_sentfilter_start(&sentfilter, text));
}



START_TEST(test_default)
{
	init();
	start(T("Mr. Jones."));
	assert_text_eq(next(), T("Mr. "));
	assert_text_eq(next(), T("Jones."));
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
        tcase_add_test(tc, test_default);
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
