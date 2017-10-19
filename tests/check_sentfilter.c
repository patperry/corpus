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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../lib/utf8lite/src/utf8lite.h"
#include "../src/table.h"
#include "../src/tree.h"
#include "../src/sentscan.h"
#include "../src/sentfilter.h"
#include "testutil.h"

#define STRICT CORPUS_SENTSCAN_STRICT
#define SPCRLF CORPUS_SENTSCAN_SPCRLF

#define SENT_EOT ((const struct utf8lite_text *)&sent_eot)

static struct corpus_sentfilter sentfilter;
static struct utf8lite_text sent_eot;
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


static void init(int flags)
{
	ck_assert(!has_sentfilter);
	ck_assert(!corpus_sentfilter_init(&sentfilter, flags));
	has_sentfilter = 1;
}


static void clear(void)
{
	ck_assert(has_sentfilter);
	corpus_sentfilter_clear(&sentfilter);
	ck_assert_int_eq(sentfilter.backsupp.nnode, 0);
	ck_assert_int_eq(sentfilter.fwdsupp.nnode, 0);
}


static void suppress(const struct utf8lite_text *pattern)
{
	ck_assert(has_sentfilter);
	ck_assert(!corpus_sentfilter_suppress(&sentfilter, pattern));
}


static void start(const struct utf8lite_text *text)
{
	ck_assert(!corpus_sentfilter_start(&sentfilter, text));
}


static const struct utf8lite_text *next(void)
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
	init(STRICT);
	start(T("Mr. Jones."));
	assert_text_eq(next(), T("Mr. "));
	assert_text_eq(next(), T("Jones."));
	assert_text_eq(next(), SENT_EOT);
}
END_TEST


START_TEST(test_newline)
{
	init(STRICT);
	start(T("Mr.\nJones."));
	assert_text_eq(next(), T("Mr.\n"));
	assert_text_eq(next(), T("Jones."));
	assert_text_eq(next(), SENT_EOT);
}
END_TEST


START_TEST(test_suppress)
{
	init(STRICT);
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
	init(STRICT);
	suppress(T("Mx."));

	start(T("AMx. Split."));
	assert_text_eq(next(), T("AMx. "));
	assert_text_eq(next(), T("Split."));
	assert_text_eq(next(), SENT_EOT);

	start(T("M x. Split."));
	assert_text_eq(next(), T("M x. "));
	assert_text_eq(next(), T("Split."));
	assert_text_eq(next(), SENT_EOT);
}
END_TEST


START_TEST(test_suppress_break)
{
	init(STRICT);
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

	start(T("end)Mx. Jones."));
	assert_text_eq(next(), T("end)Mx. Jones."));
	assert_text_eq(next(), SENT_EOT);
}
END_TEST


START_TEST(test_suppress_multi)
{
	init(STRICT);
	suppress(T("Ph.D."));
	suppress(T("z.B."));

	start(T("Ph.D. English"));
	assert_text_eq(next(), T("Ph.D. English"));
	assert_text_eq(next(), SENT_EOT);

	start(T("z.B. Deutsch"));
	assert_text_eq(next(), T("z.B. Deutsch"));
	assert_text_eq(next(), SENT_EOT);
}
END_TEST


START_TEST(test_suppress_space)
{
	init(STRICT);
	suppress(T("U. U."));

	//start(T("U. U. A"));
	//assert_text_eq(next(), T("U. U. A"));
	//assert_text_eq(next(), SENT_EOT);

	suppress(T("D. h."));
	start(T("D. h. A"));
	assert_text_eq(next(), T("D. h. A"));
	assert_text_eq(next(), SENT_EOT);

	start(T("U. U. D. h. A"));
	assert_text_eq(next(), T("U. U. D. h. A"));
	assert_text_eq(next(), SENT_EOT);
}
END_TEST


START_TEST(test_suppress_cldr)
{
	const char *name, **names = corpus_sentsuppress_names();
	const uint8_t *supp, **list;
	const struct utf8lite_text *sent;
	struct utf8lite_text text;
	size_t size;
	uint8_t *ptr;
	uint8_t buffer[128];
	int nfail = 0;

	init(STRICT);

	while ((name = *names++)) {
		list = corpus_sentsuppress_list(name, NULL);
		while ((supp = *list++)) {
			clear();

			// add the suppression rule
			ptr = buffer;
			size = strlen((const char *)supp);
			memcpy(ptr, supp, size);
			ck_assert(!utf8lite_text_assign(&text, buffer, size, 0,
						        NULL));
			suppress(&text);

			// test the rule
			ptr[size++] = ' ';
			ptr[size++] = 'A';
			ptr[size] = '\0';
			ck_assert(!utf8lite_text_assign(&text, buffer, size, 0,
						        NULL));
			
			start(&text);
			sent = next();
			if (!utf8lite_text_equals(sent, &text)) {
				printf("failed (%s): %s\n", name, supp);
				nfail++;
			} else {
				assert_text_eq(next(), SENT_EOT);
			}
		}
	}

	ck_assert(nfail == 0);
}
END_TEST


START_TEST(test_suppress_cldr_crlf)
{
	const char *name, **names = corpus_sentsuppress_names();
	const uint8_t *supp, **list;
	struct utf8lite_text text;
	size_t size;
	uint8_t *ptr;
	uint8_t buffer[128];

	init(SPCRLF);

	while ((name = *names++)) {
		list = corpus_sentsuppress_list(name, NULL);
		while ((supp = *list++)) {
			clear();

			// add the suppression rule
			ptr = buffer;
			size = strlen((const char *)supp);
			memcpy(ptr, supp, size);
			ck_assert(!utf8lite_text_assign(&text, buffer, size, 0,
						        NULL));
			suppress(&text);

			// test the rule
			ptr[size++] = '\r';
			ptr[size++] = '\n';
			ptr[size++] = 'A';
			ptr[size] = '\0';
			ck_assert(!utf8lite_text_assign(&text, buffer, size, 0,
						        NULL));
			start(&text);
			assert_text_eq(next(), &text);
			assert_text_eq(next(), SENT_EOT);
		}
	}
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
        tcase_add_test(tc, test_suppress_multi);
        tcase_add_test(tc, test_suppress_space);
	suite_add_tcase(s, tc);

	tc = tcase_create("cldr");
	tcase_add_checked_fixture(tc, setup_sentfilter, teardown_sentfilter);
        tcase_add_test(tc, test_suppress_cldr);
        tcase_add_test(tc, test_suppress_cldr_crlf);
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
