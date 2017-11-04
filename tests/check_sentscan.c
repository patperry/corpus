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
#include "../src/sentscan.h"
#include "testutil.h"

#define SENT_BREAK_TEST "data/ucd/auxiliary/SentenceBreakTest.txt"
struct corpus_sentscan scan;


void setup_scan(void)
{
	setup();
}


void teardown_scan(void)
{
	teardown();
}


void start(const struct utf8lite_text *text)
{
	corpus_sentscan_make(&scan, text, CORPUS_SENTSCAN_STRICT);
}


const struct utf8lite_text *next(void)
{
	struct utf8lite_text *sent;
	if (!corpus_sentscan_advance(&scan)) {
		return NULL;
	}
	sent = alloc(sizeof(*sent));
	*sent = scan.current;
	return sent;
}


START_TEST(test_figure3)
{
	// Test Figure 3 from http://www.unicode.org/reports/tr29/
	start(T("c.d"));
	assert_text_eq(next(), T("c.d"));
	ck_assert(next() == NULL);

	start(T("3.4"));
	assert_text_eq(next(), T("3.4"));
	ck_assert(next() == NULL);

	start(T("U.S."));
	assert_text_eq(next(), T("U.S."));
	ck_assert(next() == NULL);

	start(T("the resp. leaders are"));
	assert_text_eq(next(), T("the resp. leaders are"));
	ck_assert(next() == NULL);

	start(T("etc.)\\u2019\\u2018(the"));
	assert_text_eq(next(), T("etc.)\\u2019\\u2018(the"));
	ck_assert(next() == NULL);
}
END_TEST


START_TEST(test_figure4)
{
	start(T("She said \\u201cSee spot run.\\u201d John shook his head."));
	assert_text_eq(next(), T("She said \\u201cSee spot run.\\u201d "));
	assert_text_eq(next(), T("John shook his head."));
	ck_assert(next() == NULL);
}
END_TEST


START_TEST(test_empty)
{
	start(T(""));
	assert_text_eq(next(), T(""));
	ck_assert(next() == NULL);
	ck_assert(next() == NULL);
}
END_TEST


// Unicode Sentence Break Test
// http://www.unicode.org/Public/UCD/latest/ucd/auxiliary/SentBreakTest.txt
struct unitest {
	char comment[4096];
	unsigned line;
	int is_ascii;

	struct utf8lite_text text;
	uint8_t buf[4096];

	int32_t code[256];
	int can_break_before[256];
	uint8_t *code_end[256];
	unsigned ncode;

	uint8_t *break_begin[256];
	uint8_t *break_end[256];
	unsigned nbreak;

};

struct unitest unitests[4096];
unsigned nunitest;

void write_unitest(FILE *stream, const struct unitest *test)
{
	unsigned i, n = test->ncode;

	for (i = 0; i < n; i++) {
		fprintf(stream, "%s %04X ",
			(test->can_break_before[i]) ? "\xC3\xB7" : "\xC3\x97",
			test->code[i]);
	}
	fprintf(stream, "\xC3\xB7 %s\n", test->comment);
}

void setup_unicode(void)
{
	struct unitest *test;
	FILE *file;
	unsigned code, line, nbreak, ncode;
	uint8_t *dst;
	char *comment;
	int ch, is_ascii;

	setup_scan();
	file = fopen(SENT_BREAK_TEST, "r");
	if (!file) {
		file = fopen("../"SENT_BREAK_TEST, "r");
	}

	nunitest = 0;
	test = &unitests[0];

	line = 1;
	ncode = 0;
	nbreak = 0;
	is_ascii = 1;
	test->text.ptr = &test->buf[0];
	dst = test->text.ptr;

	ck_assert_msg(file != NULL, "file '"SENT_BREAK_TEST"' not found");
	while ((ch = fgetc(file)) != EOF) {
		switch (ch) {
		case '#':
			comment = &test->comment[0];
			do {
				*comment++ = (char)ch;
				ch = fgetc(file);
			} while (ch != EOF && ch != '\n');
			*comment = '\0';

			if (ch == EOF) {
				goto eof;
			}
			/* fallthrough */
		case '\n':
			*dst = '\0';

			test->line = line;
			test->is_ascii = is_ascii;
			test->text.attr = (size_t)(dst - test->text.ptr);

			if (ncode > 0) {
				test->ncode = ncode;
				test->nbreak = nbreak;
				ncode = 0;
				nbreak = 0;
				is_ascii = 1;
				nunitest++;
				test = &unitests[nunitest];
				comment = &test->comment[0];
				test->text.ptr = &test->buf[0];
				test->comment[0] = '\0';
				dst = test->text.ptr;
			}
			line++;
			break;

		case 0xC3:
			ch = fgetc(file);
			if (ch == EOF) {
				goto eof;
			} else if (ch == 0x97) {
				// MULTIPLICATON SIGN (U+00D7) 0xC3 0x97
				test->can_break_before[ncode] = 0;
			} else if (ch == 0xB7) {
				// DIVISION SIGN (U+00F7) 0xC3 0xB7
				test->can_break_before[ncode] = 1;
			} else {
				goto inval;
			}

			if (test->can_break_before[ncode]) {
				test->break_begin[nbreak] = dst;
				if (nbreak > 0) {
					test->break_end[nbreak - 1] = dst;
				}
				nbreak++;
			}

			if (fscanf(file, "%x", &code)) {
				test->code[ncode] = (int32_t)code;
				if (code > 0x7F) {
					is_ascii = 0;
				}
				utf8lite_encode_utf8((int32_t)code, &dst);
				test->code_end[ncode] = dst;
				ncode++;
			} else {
				test->break_end[nbreak - 1] = dst;
				nbreak--;
			}
			break;
		}

	}
eof:
	return;
inval:
	fprintf(stderr, "invalid character on line %d\n", line);

	fclose(file);
}

void teardown_unicode(void)
{
	teardown_scan();
}

START_TEST(test_unicode)
{
	struct unitest *test;
	unsigned i, j;

	for (i = 0; i < nunitest; i++) {
		test = &unitests[i];

		//fprintf(stderr, "[%u]: ", i);
		//write_unitest(stderr, test);
		corpus_sentscan_make(&scan, &test->text, CORPUS_SENTSCAN_STRICT);

		for (j = 0; j < test->nbreak; j++) {
			//fprintf(stderr, "Break %u\n", j);
			ck_assert(corpus_sentscan_advance(&scan));
			ck_assert(scan.current.ptr == test->break_begin[j]);
			ck_assert(scan.current.ptr
					+ UTF8LITE_TEXT_SIZE(&scan.current)
					== test->break_end[j]);
		}
		ck_assert(!corpus_sentscan_advance(&scan));
	}
}
END_TEST


Suite *sentscan_suite(void)
{
        Suite *s;
        TCase *tc;

        s = suite_create("sentscan");
	tc = tcase_create("core");
        tcase_add_checked_fixture(tc, setup_scan, teardown_scan);
        tcase_add_test(tc, test_figure3);
        tcase_add_test(tc, test_figure4);
        tcase_add_test(tc, test_empty);
        suite_add_tcase(s, tc);

        tc = tcase_create("Unicode SentenceBreakTest.txt");
        tcase_add_checked_fixture(tc, setup_unicode, teardown_unicode);
        tcase_add_test(tc, test_unicode);
        suite_add_tcase(s, tc);

	return s;
}


int main(void)
{
        int number_failed;
        Suite *s;
        SRunner *sr;

        s = sentscan_suite();
        sr = srunner_create(s);

        srunner_run_all(sr, CK_NORMAL);
        number_failed = srunner_ntests_failed(sr);
        srunner_free(sr);

        return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
