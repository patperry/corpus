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
#include <syslog.h>
#include "../src/text.h"
#include "../src/unicode.h"
#include "testutil.h"


void setup_text(void)
{
	setup();
}


void teardown_text(void)
{
	teardown();
}


int is_valid_text(const char *str)
{
	struct text text;
	size_t n = strlen(str);
	int err = text_assign(&text, (const uint8_t *)str, n, 0);
	return !err;
}


int is_valid_raw(const char *str)
{
	struct text text;
	size_t n = strlen(str);
	int err = text_assign(&text, (const uint8_t *)str, n, TEXT_NOESCAPE);
	return !err;
}


const char *unescape(const struct text *text)
{
	struct text_iter it;
	size_t n = TEXT_SIZE(text);
	uint8_t *buf = alloc(n + 1);
	uint8_t *ptr = buf;

	text_iter_make(&it, text);
	while (text_iter_advance(&it)) {
		encode_utf8(it.current, &ptr);
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
}
END_TEST


START_TEST(test_invalid_text)
{
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
}
END_TEST


START_TEST(test_invalid_raw)
{
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

	return s;
}


int main(void)
{
	int nfail;
	Suite *s;
	SRunner *sr;

	openlog("check_text", LOG_CONS | LOG_PERROR | LOG_PID, LOG_USER);
        setlogmask(LOG_UPTO(LOG_INFO));
        setlogmask(LOG_UPTO(LOG_DEBUG));

	s = text_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	nfail = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (nfail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
