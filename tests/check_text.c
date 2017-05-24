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
#include <string.h>
#include "../src/error.h"
#include "../src/text.h"
#include "../src/unicode.h"
#include "testutil.h"


void ignore_message(int code, const char *message)
{
	(void)code;
	(void)message;
}


void setup_text(void)
{
	setup();
}


void teardown_text(void)
{
	teardown();
	corpus_log_func = NULL;
}


int is_valid_text(const char *str)
{
	struct corpus_text text;
	size_t n = strlen(str);
	int err = corpus_text_assign(&text, (const uint8_t *)str, n, 0);
	return !err;
}


int is_valid_raw(const char *str)
{
	struct corpus_text text;
	size_t n = strlen(str);
	int err = corpus_text_assign(&text, (const uint8_t *)str, n,
				     CORPUS_TEXT_NOESCAPE);
	return !err;
}


const char *unescape(const struct corpus_text *text)
{
	struct corpus_text_iter it;
	size_t n = CORPUS_TEXT_SIZE(text);
	uint8_t *buf = alloc(n + 1);
	uint8_t *ptr = buf;

	corpus_text_iter_make(&it, text);
	while (corpus_text_iter_advance(&it)) {
		corpus_encode_utf8(it.current, &ptr);
	}
	*ptr = '\0';
	return (const char *)buf;
}


START_TEST(test_valid_text)
{
	ck_assert(is_valid_text("hello world"));
	ck_assert(is_valid_text("escape: \\n\\r\\t"));
	ck_assert(is_valid_text("unicode escape: \\u0034"));
	ck_assert(is_valid_text("surrogate pair: \\uD834\\uDD1E"));
	ck_assert(is_valid_text("B\\u0153uf Bourguignon"));
}
END_TEST


START_TEST(test_invalid_text)
{
	corpus_log_func = ignore_message;

	ck_assert(!is_valid_text("invalid utf-8 \xBF"));
	ck_assert(!is_valid_text("invalid utf-8 \xC2\x7F"));
	ck_assert(!is_valid_text("invalid escape \\a"));
	ck_assert(!is_valid_text("missing escape \\"));
	ck_assert(!is_valid_text("ends early \\u007"));
	ck_assert(!is_valid_text("non-hex value \\u0G7F"));
	ck_assert(!is_valid_text("\\uD800 high surrogate"));
	ck_assert(!is_valid_text("\\uDBFF high surrogate"));
	ck_assert(!is_valid_text("\\uD800\\uDC0G invalid hex"));
	ck_assert(!is_valid_text("\\uDC00 low surrogate"));
	ck_assert(!is_valid_text("\\uDFFF low surrogate"));
	ck_assert(!is_valid_text("\\uD84 incomplete"));
	ck_assert(!is_valid_text("\\uD804\\u2603 invalid low"));
}
END_TEST


START_TEST(test_valid_raw)
{
	ck_assert(is_valid_raw("invalid escape \\a"));
	ck_assert(is_valid_raw("missing escape \\"));
	ck_assert(is_valid_raw("ends early \\u007"));
	ck_assert(is_valid_raw("non-hex value \\u0G7F"));
	ck_assert(is_valid_raw("\\uD800 high surrogate"));
	ck_assert(is_valid_raw("\\uDBFF high surrogate"));
	ck_assert(is_valid_raw("\\uD800\\uDC0G invalid hex"));
	ck_assert(is_valid_raw("\\uDC00 low surrogate"));
	ck_assert(is_valid_raw("\\uDFFF low surrogate"));
	ck_assert(is_valid_raw("\\uD84 incomplete"));
	ck_assert(is_valid_raw("\\uD804\\u2603 invalid low"));
	ck_assert(is_valid_raw("B\xC5\x93uf Bourguignon"));
}
END_TEST


START_TEST(test_invalid_raw)
{
	corpus_log_func = ignore_message;

	ck_assert(!is_valid_raw("invalid utf-8 \xBF"));
	ck_assert(!is_valid_raw("invalid utf-8 \xC2\x7F"));
}
END_TEST


START_TEST(test_unescape_escape)
{
	ck_assert_str_eq(unescape(T("\\\\")), "\\");
	ck_assert_str_eq(unescape(T("\\/")), "/");
	ck_assert_str_eq(unescape(T("\\\"")), "\"");
	ck_assert_str_eq(unescape(T("\\b")), "\b");
	ck_assert_str_eq(unescape(T("\\f")), "\f");
	ck_assert_str_eq(unescape(T("\\n")), "\n");
	ck_assert_str_eq(unescape(T("\\r")), "\r");
	ck_assert_str_eq(unescape(T("\\t")), "\t");
}
END_TEST


START_TEST(test_unescape_raw)
{
	ck_assert_str_eq(unescape(S("\\\\")), "\\\\");
	ck_assert_str_eq(unescape(S("\\/")), "\\/");
	ck_assert_str_eq(unescape(S("\\\"")), "\\\"");
	ck_assert_str_eq(unescape(S("\\b")), "\\b");
	ck_assert_str_eq(unescape(S("\\f")), "\\f");
	ck_assert_str_eq(unescape(S("\\n")), "\\n");
	ck_assert_str_eq(unescape(S("\\r")), "\\r");
	ck_assert_str_eq(unescape(S("\\t")), "\\t");
}
END_TEST


START_TEST(test_unescape_utf16)
{
	ck_assert_str_eq(unescape(T("\\u2603")), "\xE2\x98\x83");
	ck_assert_str_eq(unescape(T("\\u0024")), "\x24");
	ck_assert_str_eq(unescape(T("\\uD801\\uDC37")), "\xF0\x90\x90\xB7");
	ck_assert_str_eq(unescape(T("\\uD852\\uDF62")), "\xF0\xA4\xAD\xA2");

}
END_TEST


static struct corpus_text_iter iter;

static void start (const struct corpus_text *text)
{
	corpus_text_iter_make(&iter, text);
}

static int next(void)
{
	if (corpus_text_iter_advance(&iter)) {
		return (int)iter.current;
	} else {
		return -1;
	}
}

static int prev(void)
{
	if (corpus_text_iter_retreat(&iter)) {
		return (int)iter.current;
	} else {
		return -1;
	}
}


START_TEST(test_iter_ascii)
{
	start(T("abba zabba"));

	// forward
	ck_assert_int_eq(prev(), -1);
	ck_assert_int_eq(next(), 'a');
	ck_assert_int_eq(next(), 'b');
	ck_assert_int_eq(next(), 'b');
	ck_assert_int_eq(next(), 'a');
	ck_assert_int_eq(next(), ' ');
	ck_assert_int_eq(next(), 'z');
	ck_assert_int_eq(next(), 'a');
	ck_assert_int_eq(next(), 'b');
	ck_assert_int_eq(next(), 'b');
	ck_assert_int_eq(next(), 'a');
	ck_assert_int_eq(next(), -1);
	ck_assert_int_eq(next(), -1);

	// backward
	ck_assert_int_eq(prev(), 'a');
	ck_assert_int_eq(prev(), 'b');
	ck_assert_int_eq(prev(), 'b');
	ck_assert_int_eq(prev(), 'a');
	ck_assert_int_eq(prev(), 'z');
	ck_assert_int_eq(prev(), ' ');
	ck_assert_int_eq(prev(), 'a');
	ck_assert_int_eq(prev(), 'b');
	ck_assert_int_eq(prev(), 'b');
	ck_assert_int_eq(prev(), 'a');
	ck_assert_int_eq(prev(), -1);
	ck_assert_int_eq(prev(), -1);
}
END_TEST


START_TEST(test_iter_utf8)
{
	start(T("\xE2\x98\x83 \xF0\x9F\x99\x82 \xC2\xA7\xC2\xA4"));

	ck_assert_int_eq(next(), 0x2603); // \xE2\x98\x83
	ck_assert_int_eq(next(), ' ');
	ck_assert_int_eq(next(), 0x1F642); // \xF0\x9F\x99\x82
	ck_assert_int_eq(next(), ' ');
	ck_assert_int_eq(next(), 0xA7); // \xC2\xA7
	ck_assert_int_eq(next(), 0xA4); // \xC2\xA4
	ck_assert_int_eq(next(), -1);

	ck_assert_int_eq(prev(), 0xA4);
	ck_assert_int_eq(prev(), 0xA7);
	ck_assert_int_eq(prev(), ' ');
	ck_assert_int_eq(prev(), 0x1F642);
	ck_assert_int_eq(prev(), ' ');
	ck_assert_int_eq(prev(), 0x2603);
	ck_assert_int_eq(prev(), -1);
}
END_TEST


START_TEST(test_iter_escape)
{
	start(T("nn\\\\\\n\\nn\\\\n"));

	ck_assert_int_eq(next(), 'n');
	ck_assert_int_eq(next(), 'n');
	ck_assert_int_eq(next(), '\\');
	ck_assert_int_eq(next(), '\n');
	ck_assert_int_eq(next(), '\n');
	ck_assert_int_eq(next(), 'n');
	ck_assert_int_eq(next(), '\\');
	ck_assert_int_eq(next(), 'n');
	ck_assert_int_eq(next(), -1);

	ck_assert_int_eq(prev(), 'n');
	ck_assert_int_eq(prev(), '\\');
	ck_assert_int_eq(prev(), 'n');
	ck_assert_int_eq(prev(), '\n');
	ck_assert_int_eq(prev(), '\n');
	ck_assert_int_eq(prev(), '\\');
	ck_assert_int_eq(prev(), 'n');
	ck_assert_int_eq(prev(), 'n');
	ck_assert_int_eq(prev(), -1);
}
END_TEST


START_TEST(test_iter_uescape)
{
	start(T("\\u2603 \\uD83D\\uDE42 \\u00a7\\u00a4"));

	ck_assert_int_eq(next(), 0x2603);
	ck_assert_int_eq(next(), ' ');
	ck_assert_int_eq(next(), 0x1F642);
	ck_assert_int_eq(next(), ' ');
	ck_assert_int_eq(next(), 0xA7);
	ck_assert_int_eq(next(), 0xA4);
	ck_assert_int_eq(next(), -1);

	ck_assert_int_eq(prev(), 0xA4);
	ck_assert_int_eq(prev(), 0xA7);
	ck_assert_int_eq(prev(), ' ');
	ck_assert_int_eq(prev(), 0x1F642);
	ck_assert_int_eq(prev(), ' ');
	ck_assert_int_eq(prev(), 0x2603);
	ck_assert_int_eq(prev(), -1);

}
END_TEST

struct type {
	const char *string;
	int value;
	size_t attr;
};

#define ESC CORPUS_TEXT_ESC_BIT
#define UTF8 CORPUS_TEXT_UTF8_BIT

START_TEST(test_iter_random)
{
	const struct type types[] = {
		// escaped
		{ "\\\"", '\"', ESC },
		{ "\\\\", '\\', ESC },
		{ "\\/", '/', ESC },
		{ "\\b", '\b', ESC },
		{ "\\f", '\f', ESC },
		{ "\\n", '\n', ESC },
		{ "\\r", '\r', ESC },
		{ "\\t", '\t', ESC },

		// not escaped
		{ "\"", '\"', 0 },
		{ "/", '/', 0 },
		{ "b", 'b', 0 },
		{ "f", 'f', 0 },
		{ "n", 'n', 0 },
		{ "r", 'r', 0 },
		{ "t", 't', 0 },
		{ "u", 'u', 0 },

		// 2-byte UTF-8
		{ "\xC2\xA7", 0xA7, UTF8 },
		{ "\\u00a7", 0xA7, ESC|UTF8 },

		// 3-byte UTF-8
		{ "\xE2\x98\x83", 0x2603, UTF8 },
		{ "\\u2603", 0x2603, ESC|UTF8 },

		// 4-byte UTF-8
		{ "\xE2\x98\x83", 0x2603, UTF8 },
		{ "\\uD83D\\uDE42", 0x1F642, ESC|UTF8 }
	};
	unsigned ntype = sizeof(types) / sizeof(types[0]);

	struct corpus_text text;
	struct corpus_text_iter iter;
	uint8_t buffer[1024 * 12];
	int toks[1024];
	int ntok_max = 1024 - 1;
	int ntok;
	size_t len, size;
	int i, id;

	srand(0);

	ntok = (100 + rand()) % ntok_max;
	size = 0;
	for (i = 0; i < ntok; i++) {
		id = rand() % ntype;
		toks[i] = id;

		len = strlen(types[id].string);
		memcpy(buffer + size, types[id].string, len);
		size += len;
	}
	toks[ntok] = -1;
	buffer[size] = '\0';

	ck_assert(!corpus_text_assign(&text, buffer, size, 0));
	corpus_text_iter_make(&iter, &text);

	// forward iteration
	for (i = 0; i < ntok; i++) {
		ck_assert(corpus_text_iter_advance(&iter));

		id = toks[i];
		ck_assert_int_eq(iter.current, types[id].value);
		ck_assert_int_eq(iter.attr, types[id].attr);
	}
	ck_assert(!corpus_text_iter_advance(&iter));

	// reverse iteration
	while (i-- > 0) {
		ck_assert(corpus_text_iter_retreat(&iter));

		id = toks[i];
		ck_assert_int_eq(iter.current, types[id].value);
		ck_assert_int_eq(iter.attr, types[id].attr);
	}
	ck_assert(!corpus_text_iter_retreat(&iter));
}
END_TEST


Suite *text_suite(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("text");

	tc = tcase_create("validation");
        tcase_add_checked_fixture(tc, setup_text, teardown_text);
	tcase_add_test(tc, test_valid_text);
	tcase_add_test(tc, test_invalid_text);
	tcase_add_test(tc, test_valid_raw);
	tcase_add_test(tc, test_invalid_raw);
	suite_add_tcase(s, tc);

	tc = tcase_create("unescaping");
        tcase_add_checked_fixture(tc, setup_text, teardown_text);
	tcase_add_test(tc, test_unescape_escape);
	tcase_add_test(tc, test_unescape_raw);
	tcase_add_test(tc, test_unescape_utf16);
	suite_add_tcase(s, tc);

	tc = tcase_create("iteration");
        tcase_add_checked_fixture(tc, setup_text, teardown_text);
	tcase_add_test(tc, test_iter_ascii);
	tcase_add_test(tc, test_iter_utf8);
	tcase_add_test(tc, test_iter_escape);
	tcase_add_test(tc, test_iter_uescape);
	tcase_add_test(tc, test_iter_random);
	suite_add_tcase(s, tc);

	return s;
}


int main(void)
{
	int nfail;
	Suite *s;
	SRunner *sr;

	s = text_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	nfail = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (nfail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
