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
#include "../src/unicode.h"

#define NORMALIZATION_TEST "data/ucd/NormalizationTest.txt"

// Unicode Normalization Test
// http://www.unicode.org/Public/UCD/latest/ucd/NormalizationTest.txt
struct normalization_test {
	char comment[1024];
	unsigned line;
	int part;

	uint32_t source[128];
	unsigned source_len;

	uint32_t nfc[128];
	unsigned nfc_len;

	uint32_t nfd[128];
	unsigned nfd_len;

	uint32_t nfkc[128];
	unsigned nfkc_len;

	uint32_t nfkd[128];
	unsigned nfkd_len;
};

struct normalization_test normalization_tests[32768];
unsigned num_normalization_test;

void write_normalization_test(FILE *stream,
			      const struct normalization_test *test)
{
	unsigned i;

	for (i = 0; i < test->source_len; i++) {
		if (i > 0) {
			fprintf(stream, " ");
		}
		fprintf(stream, "%04X", test->source[i]);
	}
	fprintf(stream, ";");

	for (i = 0; i < test->nfc_len; i++) {
		if (i > 0) {
			fprintf(stream, " ");
		}
		fprintf(stream, "%04X", test->nfc[i]);
	}
	fprintf(stream, ";");

	for (i = 0; i < test->nfd_len; i++) {
		if (i > 0) {
			fprintf(stream, " ");
		}
		fprintf(stream, "%04X", test->nfd[i]);
	}
	fprintf(stream, ";");

	for (i = 0; i < test->nfkc_len; i++) {
		if (i > 0) {
			fprintf(stream, " ");
		}
		fprintf(stream, "%04X", test->nfkc[i]);
	}
	fprintf(stream, ";");

	for (i = 0; i < test->nfkd_len; i++) {
		if (i > 0) {
			fprintf(stream, " ");
		}
		fprintf(stream, "%04X", test->nfkd[i]);
	}
	fprintf(stream, ";");

	fprintf(stream, "%s\n", test->comment);
}


void setup_normalization(void)
{
	struct normalization_test *test;
	FILE *file;
	int part;
	unsigned field, line, ntest;
	unsigned code;
	uint32_t *dst;
	unsigned *lenp;
	char *comment;
	char ch;

	file = fopen(NORMALIZATION_TEST, "r");
	if (!file) {
		file = fopen("../"NORMALIZATION_TEST, "r");
	}
	ck_assert_msg(file, "file '"NORMALIZATION_TEST"' not found");

	ntest = 0;
	test = &normalization_tests[ntest];
	test->comment[0] = '\0';
	test->source_len = 0;
	test->nfc_len = 0;
	test->nfd_len = 0;
	test->nfkc_len = 0;
	test->nfkd_len = 0;
	dst = test->source;
	lenp = &test->source_len;

	part = -1;
	line = 1;
	field = 0;

	while ((ch = fgetc(file)) != EOF) {
		switch (ch) {
		case '#':
			comment = &test->comment[0];
			do {
				*comment++ = ch;
				ch = fgetc(file);
			} while (ch != EOF && ch != '\n');
			*comment = '\0';

			if (ch == EOF) {
				goto eof;
			}
			/* fallthrough */
		case '\n':
			test->line = line;
			test->part = part;
			if (test->source_len > 0) {
				ntest++;
				test = &normalization_tests[ntest];
				test->comment[0] = '\0';
				test->source_len = 0;
				test->nfc_len = 0;
				test->nfd_len = 0;
				test->nfkc_len = 0;
				test->nfkd_len = 0;
				dst = test->source;
				lenp = &test->source_len;
			}
			line++;
			field = 0;
			break;
		case ';':
			field++;
			switch (field) {
			case 1:
				dst = test->nfc;
				lenp = &test->nfc_len;
				break;
			case 2:
				dst = test->nfd;
				lenp = &test->nfd_len;
				break;
			case 3:
				dst = test->nfkc;
				lenp = &test->nfkc_len;
				break;
			case 4:
				dst = test->nfkd;
				lenp = &test->nfkd_len;
				break;
			case 5:
				break;
			default:
				fprintf(stderr, "too many fields\n");
				goto inval;
			}
			break;
		case ' ':
			break;
		case '@':
			fscanf(file, "Part%d", &part);
			break;
		default:
			ungetc(ch, file);
			fscanf(file, "%X", &code);
			*dst++ = (uint32_t)code;
			*lenp = *lenp + 1;
			break;
		}
	}
eof:
	num_normalization_test = ntest;
	return;
inval:
	fprintf(stderr, "invalid character on line %d\n", line);

	fclose(file);
}


void teardown_normalization(void)
{
}


int is_utf8(const char *str, size_t len)
{
	const uint8_t *ptr = (const uint8_t *)str;
	const uint8_t *end = ptr + len;
        int err;

        while (ptr < end) {
                if ((err = scan_utf8(&ptr, end))) {
			return 0;
                }
        }

        return 1;
}

START_TEST(test_accept_valid_1byte_utf8)
{
	ck_assert(is_utf8("\x00", 1));
	ck_assert(is_utf8("\x01", 1));
	ck_assert(is_utf8("\x7E", 1));
	ck_assert(is_utf8("\x7F", 1));

}
END_TEST

START_TEST(test_reject_invalid_1byte_utf8)
{
	ck_assert(!is_utf8("\x80", 1));
	ck_assert(!is_utf8("\xBF", 1));
	ck_assert(!is_utf8("\xC0", 1));
	ck_assert(!is_utf8("\xE0", 1));
	ck_assert(!is_utf8("\xF0", 1));
	ck_assert(!is_utf8("\xF8", 1));
	ck_assert(!is_utf8("\xFC", 1));
	ck_assert(!is_utf8("\xFE", 1));
	ck_assert(!is_utf8("\xFF", 1));
}
END_TEST

START_TEST(test_accept_valid_2byte_utf8)
{
	ck_assert(is_utf8("\xC2\x80", 2));
	ck_assert(is_utf8("\xC2\x8F", 2));
	ck_assert(is_utf8("\xDF\x80", 2));
	ck_assert(is_utf8("\xDF\x8F", 2));
}
END_TEST

START_TEST(test_reject_invalid_2byte_utf8)
{
	// invalid first byte, valid second byte
	ck_assert(!is_utf8("\x80\x80", 2));
	ck_assert(!is_utf8("\xC1\x8F", 2));
	ck_assert(!is_utf8("\xF5\x80", 2));
	ck_assert(!is_utf8("\xFF\x80", 2));
	ck_assert(!is_utf8("\xFF\xFF", 2));

	// valid first byte, invalid second byte
	ck_assert(!is_utf8("\xC2\x00", 2));
	ck_assert(!is_utf8("\xC2\x7F", 2));
	ck_assert(!is_utf8("\xDF\x00", 2));
	ck_assert(!is_utf8("\xDF\x7F", 2));

	// too short
	ck_assert(!is_utf8("\xE0\x80", 2));
	ck_assert(!is_utf8("\xE0\xA0", 2));
	ck_assert(!is_utf8("\xE1\x80", 2));
	ck_assert(!is_utf8("\xEC\x80", 2));
	ck_assert(!is_utf8("\xEE\x80", 2));
	ck_assert(!is_utf8("\xED\x80", 2));
	ck_assert(!is_utf8("\xEF\x80", 2));
	ck_assert(!is_utf8("\xF0\x80", 2));
	ck_assert(!is_utf8("\xF0\x90", 2));
	ck_assert(!is_utf8("\xF1\x80", 2));
	ck_assert(!is_utf8("\xF4\x80", 2));
}
END_TEST

START_TEST(test_accept_valid_3byte_utf8)
{
	ck_assert(is_utf8("\xE0\xA0\x80", 3));
	ck_assert(is_utf8("\xE0\xA0\xBF", 3));
	ck_assert(is_utf8("\xE0\xBF\x80", 3));
	ck_assert(is_utf8("\xE0\xBF\xBF", 3));

	ck_assert(is_utf8("\xE1\x80\x80", 3));
	ck_assert(is_utf8("\xE1\x80\xBF", 3));
	ck_assert(is_utf8("\xE1\xBF\x80", 3));
	ck_assert(is_utf8("\xE1\xBF\xBF", 3));
	ck_assert(is_utf8("\xEC\x80\x80", 3));
	ck_assert(is_utf8("\xEC\x80\xBF", 3));
	ck_assert(is_utf8("\xEC\xBF\x80", 3));
	ck_assert(is_utf8("\xEC\xBF\xBF", 3));

	ck_assert(is_utf8("\xED\x80\x80", 3));
	ck_assert(is_utf8("\xED\x80\xBF", 3));
	ck_assert(is_utf8("\xED\x9F\x80", 3));
	ck_assert(is_utf8("\xED\x9F\xBF", 3));
}
END_TEST

START_TEST(test_reject_invalid_3byte_utf8)
{
	ck_assert(!is_utf8("\xE0\x80\x80", 3));
	ck_assert(!is_utf8("\xE0\x80\xBF", 3));
	ck_assert(!is_utf8("\xE0\x9F\x80", 3));
	ck_assert(!is_utf8("\xE0\x9F\xBF", 3));

	ck_assert(!is_utf8("\xED\xA0\x80", 3));
	ck_assert(!is_utf8("\xED\xA0\xBF", 3));
	ck_assert(!is_utf8("\xED\xBF\x80", 3));
	ck_assert(!is_utf8("\xED\xBF\xBF", 3));
}
END_TEST

START_TEST(test_accept_valid_4byte_utf8)
{
	ck_assert(is_utf8("\xF0\x90\x80\x80", 4));
	ck_assert(is_utf8("\xF0\x90\x80\xBF", 4));
	ck_assert(is_utf8("\xF0\x90\xBF\x80", 4));
	ck_assert(is_utf8("\xF0\x90\xBF\xBF", 4));
	ck_assert(is_utf8("\xF0\xBF\x80\x80", 4));
	ck_assert(is_utf8("\xF0\xBF\x80\xBF", 4));
	ck_assert(is_utf8("\xF0\xBF\xBF\x80", 4));
	ck_assert(is_utf8("\xF0\xBF\xBF\xBF", 4));

	ck_assert(is_utf8("\xF1\x80\x80\x80", 4));
	ck_assert(is_utf8("\xF1\x80\x80\xBF", 4));
	ck_assert(is_utf8("\xF1\x80\xBF\x80", 4));
	ck_assert(is_utf8("\xF1\x80\xBF\xBF", 4));
	ck_assert(is_utf8("\xF1\xBF\x80\x80", 4));
	ck_assert(is_utf8("\xF1\xBF\x80\xBF", 4));
	ck_assert(is_utf8("\xF1\xBF\xBF\x80", 4));
	ck_assert(is_utf8("\xF1\xBF\xBF\xBF", 4));

	ck_assert(is_utf8("\xF3\x80\x80\x80", 4));
	ck_assert(is_utf8("\xF3\x80\x80\xBF", 4));
	ck_assert(is_utf8("\xF3\x80\xBF\x80", 4));
	ck_assert(is_utf8("\xF3\x80\xBF\xBF", 4));
	ck_assert(is_utf8("\xF3\xBF\x80\x80", 4));
	ck_assert(is_utf8("\xF3\xBF\x80\xBF", 4));
	ck_assert(is_utf8("\xF3\xBF\xBF\x80", 4));
	ck_assert(is_utf8("\xF3\xBF\xBF\xBF", 4));

	ck_assert(is_utf8("\xF4\x80\x80\x80", 4));
	ck_assert(is_utf8("\xF4\x80\x80\xBF", 4));
	ck_assert(is_utf8("\xF4\x80\xBF\x80", 4));
	ck_assert(is_utf8("\xF4\x80\xBF\xBF", 4));
	ck_assert(is_utf8("\xF4\x8F\x80\x80", 4));
	ck_assert(is_utf8("\xF4\x8F\x80\xBF", 4));
	ck_assert(is_utf8("\xF4\x8F\xBF\x80", 4));
	ck_assert(is_utf8("\xF4\x8F\xBF\xBF", 4));
}
END_TEST

START_TEST(test_reject_invalid_4byte_utf8)
{
	ck_assert(!is_utf8("\xF0\x80\x80\x80", 4));
	ck_assert(!is_utf8("\xF0\x8F\x80\x80", 4));
	ck_assert(!is_utf8("\xF4\x90\x80\x80", 4));
	ck_assert(!is_utf8("\xF4\xBF\x80\x80", 4));

	ck_assert(!is_utf8("\xF5\x80\x80\x80", 4));
	ck_assert(!is_utf8("\xFF\x80\x80\x80", 4));
}
END_TEST


START_TEST(test_normalize_nfd)
{
	unsigned i, n = num_normalization_test;
	unsigned j, len;
	struct normalization_test *test;
	uint32_t buf[128];
	uint32_t *dst;
	uint32_t code;

	for (i = 0; i < n; i++) {
		test = &normalization_tests[i];
		dst = buf;
		for (j = 0; j < test->source_len; j++) {
			code = test->source[j];
			utf32_decompose(0, code, &dst);
		}

		len = dst - buf;
		ck_assert_int_eq(len, test->nfd_len);

		utf32_reorder(buf, dst);

		for (j = 0; j < test->nfd_len; j++) {
			if (buf[j] != test->nfd[j]) {
				write_normalization_test(stderr, test);
			}
			ck_assert_uint_eq(buf[j], test->nfd[j]);
		}
	}
}
END_TEST


START_TEST(test_normalize_nfkd)
{
	unsigned i, n = num_normalization_test;
	unsigned j, len;
	struct normalization_test *test;
	uint32_t buf[128];
	uint32_t *dst;
	uint32_t code;

	for (i = 0; i < n; i++) {
		test = &normalization_tests[i];
		dst = buf;
		for (j = 0; j < test->source_len; j++) {
			code = test->source[j];
			utf32_decompose(UDECOMP_ALL, code, &dst);
		}

		len = dst - buf;
		ck_assert_int_eq(len, test->nfkd_len);

		utf32_reorder(buf, dst);

		for (j = 0; j < test->nfkd_len; j++) {
			if (buf[j] != test->nfkd[j]) {
				write_normalization_test(stderr, test);
			}
			ck_assert_uint_eq(buf[j], test->nfkd[j]);
		}
	}
}
END_TEST


Suite *unicode_suite(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("unicode");

	tc = tcase_create("utf8 validation");
	tcase_add_test(tc, test_accept_valid_1byte_utf8);
	tcase_add_test(tc, test_reject_invalid_1byte_utf8);
	tcase_add_test(tc, test_accept_valid_2byte_utf8);
	tcase_add_test(tc, test_reject_invalid_2byte_utf8);
	tcase_add_test(tc, test_accept_valid_3byte_utf8);
	tcase_add_test(tc, test_reject_invalid_3byte_utf8);
	tcase_add_test(tc, test_accept_valid_4byte_utf8);
	tcase_add_test(tc, test_reject_invalid_4byte_utf8);
	suite_add_tcase(s, tc);

	tc = tcase_create("utf32 normalization");
	tcase_add_checked_fixture(tc, setup_normalization,
				  teardown_normalization);
	tcase_add_test(tc, test_normalize_nfd);
	tcase_add_test(tc, test_normalize_nfkd);
	suite_add_tcase(s, tc);

	return s;
}

int main(void)
{
	int number_failed;
	Suite *s;
	SRunner *sr;

	s = unicode_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
