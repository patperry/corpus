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
#include <syslog.h>
#include <check.h>
#include "../src/text.h"
#include "../src/token.h"
#include "../src/unicode.h"
#include "testutil.h"


struct text *get_type(const struct text *tok, int flags)
{
	struct text *typ;
	struct typemap map;
	size_t size;

	ck_assert(!typemap_init(&map, flags));
	ck_assert(!typemap_set(&map, tok));
	size = TEXT_SIZE(&map.type);

	typ = alloc(sizeof(*typ));

	typ->ptr = alloc(size + 1);
	memcpy(typ->ptr, map.type.ptr, size);
	typ->ptr[size] = '\0';
	typ->attr = map.type.attr;

	typemap_destroy(&map);

	return typ;
}


struct text *casefold(const struct text *tok)
{
	return get_type(tok, TYPE_CASEFOLD);
}


START_TEST(test_equals)
{
	ck_assert(token_equals(S("hello"), S("hello")));
	ck_assert(token_equals(S("hello"), T("hello")));

	ck_assert(token_equals(S(""), S("")));
	ck_assert(token_equals(S(""), T("")));
}
END_TEST


START_TEST(test_not_equals)
{
	ck_assert(!token_equals(S("foo"), S("bar")));
}
END_TEST


START_TEST(test_equals_esc)
{
	ck_assert(!token_equals(S("\n"), T("\\n")));
	ck_assert(!token_equals(S("\\n"), T("\\n")));
}
END_TEST


START_TEST(test_typ_basic)
{
	ck_assert_tok_eq(get_type(S("hello"), 0), S("hello"));
	ck_assert_tok_eq(get_type(S("world"), 0), T("world"));
	ck_assert_tok_eq(get_type(T("foo"), 0), S("foo"));
}
END_TEST


START_TEST(test_typ_esc)
{
	// backslash
	ck_assert_tok_eq(get_type(S("\\"), 0), S("\\"));
	ck_assert_tok_eq(get_type(T("\\\\"), 0), S("\\"));
	ck_assert_tok_eq(get_type(T("\\u005C"), 0), S("\\"));
	ck_assert_tok_eq(get_type(S("\\\\"), 0), S("\\\\"));
	ck_assert_tok_eq(get_type(S("\\u005C"), TYPE_CASEFOLD), S("\\u005c"));

	// quote (")
	ck_assert_tok_eq(get_type(S("\""), TYPE_QUOTFOLD), S("\'"));
	ck_assert_tok_eq(get_type(T("\\\""), TYPE_QUOTFOLD), S("\'"));
	ck_assert_tok_eq(get_type(T("\\u0022"), TYPE_QUOTFOLD), S("\'"));
	ck_assert_tok_eq(get_type(S("\\\'"), TYPE_QUOTFOLD), S("\\\'"));
	ck_assert_tok_eq(get_type(S("\\u0022"), TYPE_QUOTFOLD), S("\\u0022"));
}
END_TEST


/*
 * Control Characters (Cc)
 * -----------------------
 *
 *	U+0000..U+001F	(C0)
 *	U+007F		(delete)
 *	U+0080..U+009F	(C1)
 *
 * Source: UnicodeStandard-8.0, Sec. 23.1, p. 808.
 */

START_TEST(test_rm_control_ascii)
{
	char str[256];
	uint8_t i;

	ck_assert_tok_eq(get_type(S("\a"), TYPE_RMCC), S(""));
	ck_assert_tok_eq(get_type(S("\b"), TYPE_RMCC), S(""));
	ck_assert_tok_eq(get_type(S("\t"), TYPE_RMCC), S("\t"));
	ck_assert_tok_eq(get_type(S("\n"), TYPE_RMCC), S("\n"));
	ck_assert_tok_eq(get_type(S("\v"), TYPE_RMCC), S("\v"));
	ck_assert_tok_eq(get_type(S("\f"), TYPE_RMCC), S("\f"));
	ck_assert_tok_eq(get_type(S("\r"), TYPE_RMCC), S("\r"));

	// C0
	for (i = 1; i < 0x20; i++) {
		if (0x09 <= i && i <= 0x0D) {
			continue;
		}

		str[0] = (char)i; str[1] = '\0';
		ck_assert_tok_eq(get_type(S(str), TYPE_RMCC), S(""));

		sprintf(str, "\\u%04X", i);
		ck_assert_tok_eq(get_type(T(str), TYPE_RMCC), S(""));
	}

	// delete
	ck_assert_tok_eq(get_type(S("\x7F"), TYPE_RMCC), S(""));
	ck_assert_tok_eq(get_type(T("\\u007F"), TYPE_RMCC), S(""));
}
END_TEST


START_TEST(test_rm_control_utf8)
{
	uint8_t str[256];
	uint8_t i;

	// C1: JSON
	for (i = 0x80; i < 0xA0; i++) {
		if (i == 0x85) {
			continue;
		}

		str[0] = 0xC2; str[1] = i; str[2] = '\0';
		ck_assert_tok_eq(get_type(S((char *)str), TYPE_RMCC), S(""));

		sprintf((char *)str, "\\u%04X", i);
		ck_assert_tok_eq(get_type(T((char *)str), TYPE_RMCC), S(""));
	}
}
END_TEST


START_TEST(test_keep_control_ascii)
{
	const struct text *js, *t;
	char str[256];
	uint8_t i;

	ck_assert_tok_eq(get_type(S("\a"), 0), S("\a"));
	ck_assert_tok_eq(get_type(S("\b"), 0), S("\b"));

	// C0
	for (i = 1; i < 0x20; i++) {
		if (0x09 <= i && i <= 0x0D) {
			continue;
		}
		str[0] = (char)i; str[1] = '\0';
		t = S(str);
		ck_assert_tok_eq(get_type(t, 0), t);

		sprintf(str, "\\u%04X", i);
		js = T(str);
		ck_assert_tok_eq(get_type(js, 0), t);
	}

	// delete
	ck_assert_tok_eq(get_type(S("\x7F"), 0), S("\x7F"));
	ck_assert_tok_eq(get_type(T("\\u007F"), 0), S("\x7F"));
}
END_TEST


START_TEST(test_keep_control_utf8)
{
	const struct text *t, *js;
	uint8_t str[256];
	uint8_t i;

	// C1
	for (i = 0x80; i < 0xA0; i++) {
		if (i == 0x85) {
			continue;
		}

		str[0] = 0xC2; str[1] = i; str[2] = '\0';
		t = S((char *)str);
		ck_assert_tok_eq(get_type(t, 0), t);

		sprintf((char *)str, "\\u%04X", i);
		js = T((char *)str);
		ck_assert_tok_eq(get_type(js, 0), t);
	}
}
END_TEST


START_TEST(test_keep_ws_ascii)
{
	ck_assert_tok_eq(get_type(S("\t"), 0), S("\t"));
	ck_assert_tok_eq(get_type(S("\n"), 0), S("\n"));
	ck_assert_tok_eq(get_type(S("\v"), 0), S("\v"));
	ck_assert_tok_eq(get_type(S("\f"), 0), S("\f"));
	ck_assert_tok_eq(get_type(S("\r"), 0), S("\r"));
	ck_assert_tok_eq(get_type(S(" "), 0), S(" "));
}
END_TEST


START_TEST(test_rm_ws_ascii)
{
	ck_assert_tok_eq(get_type(S("\t"), TYPE_RMWS), S(""));
	ck_assert_tok_eq(get_type(S("\n"), TYPE_RMWS), S(""));
	ck_assert_tok_eq(get_type(S("\v"), TYPE_RMWS), S(""));
	ck_assert_tok_eq(get_type(S("\f"), TYPE_RMWS), S(""));
	ck_assert_tok_eq(get_type(S("\r"), TYPE_RMWS), S(""));
	ck_assert_tok_eq(get_type(S(" "), TYPE_RMWS), S(""));
}
END_TEST


START_TEST(test_keep_ws_utf8)
{
	const struct text *t, *js, *typ;
	uint8_t str[256];
	uint8_t *buf;
	unsigned ws[] = { 0x0009, 0x000A, 0x000B, 0x000C, 0x000D,
			  0x0020, 0x0085, 0x00A0, 0x1680, 0x2000,
			  0x2001, 0x2002, 0x2003, 0x2004, 0x2005,
			  0x2006, 0x2007, 0x2008, 0x2009, 0x200A,
			  0x2028, 0x2029, 0x202F, 0x205F, 0x3000 };
	int i, n = sizeof(ws) / sizeof(ws[0]);

	for (i = 0; i < n; i++) {
		//fprintf(stderr, "i = %d; ws = U+%04X\n", i, ws[i]);

		switch (ws[i]) {
		case 0x0009:
			typ = S("\t");
			break;
		case 0x000A:
			typ = S("\n");
			break;
		case 0x000B:
			typ = S("\v");
			break;
		case 0x000C:
			typ = S("\f");
			break;
		case 0x000D:
			typ = S("\r");
			break;
		case 0x0085: // NEXT LINE (NEL)
			typ = S("\xC2\x85");
			break;
		case 0x1680: // OGHAM SPACE MARK
			typ = S("\xE1\x9A\x80");
			break;
		case 0x2028: // LINE SEPARATOR
			typ = S("\xE2\x80\xA8");
			break;
		case 0x2029: // PARAGRAPH SEPARATOR
			typ = S("\xE2\x80\xA9");
			break;
		default:
			typ = S(" ");
			break;
		}

		buf = str;
		encode_utf8(ws[i], &buf);
		*buf = '\0';
		t = S((char *)str);
		ck_assert_tok_eq(get_type(t, TYPE_COMPAT), typ);

		sprintf((char *)str, "\\u%04x", ws[i]);
		js = T((char *)str);
		ck_assert_tok_eq(get_type(js, TYPE_COMPAT), typ);
	}
}
END_TEST


START_TEST(test_rm_ws_utf8)
{
	const struct text *t, *js;
	uint8_t str[256];
	uint8_t *buf;
	uint32_t ws[] = { 0x0009, 0x000A, 0x000B, 0x000C, 0x000D,
			  0x0020, 0x0085, 0x00A0, 0x1680, 0x2000,
			  0x2001, 0x2002, 0x2003, 0x2004, 0x2005,
			  0x2006, 0x2007, 0x2008, 0x2009, 0x200A,
			  0x2028, 0x2029, 0x202F, 0x205F, 0x3000 };
	int i, n = sizeof(ws) / sizeof(ws[0]);

	for (i = 0; i < n; i++) {
		buf = str;
		encode_utf8(ws[i], &buf);
		*buf = '\0';
		t = S((char *)str);
		ck_assert_tok_eq(get_type(t, TYPE_RMWS), S(""));

		sprintf((char *)str, "\\u%04x", ws[i]);
		js = T((char *)str);
		ck_assert_tok_eq(get_type(js, TYPE_RMWS), S(""));
	}
}
END_TEST


START_TEST(test_casefold_ascii)
{
	const struct text *tok;
	uint8_t buf[2] = { 0, 0 };
	uint8_t i;

	ck_assert_tok_eq(casefold(S("UPPER CASE")), S("upper case"));
	ck_assert_tok_eq(casefold(S("lower case")), S("lower case"));
	ck_assert_tok_eq(casefold(S("mIxEd CaSe")), S("mixed case"));

	for (i = 0x01; i < 'A'; i++) {
		buf[0] = i;
		tok = S((char *)buf);
		ck_assert_tok_eq(casefold(tok), tok);
	}
	for (i = 'Z' + 1; i < 0x7F; i++) {
		buf[0] = i;
		tok = S((char *)buf);
		ck_assert_tok_eq(casefold(tok), tok);
	}

	// upper
	ck_assert_tok_eq(casefold(S("ABCDEFGHIJKLMNOPQRSTUVWXYZ")),
			 S("abcdefghijklmnopqrstuvwxyz"));

	// lower
	ck_assert_tok_eq(casefold(S("abcdefghijklmnopqrstuvwxyz")),
			 S("abcdefghijklmnopqrstuvwxyz"));

	// digit
	ck_assert_tok_eq(casefold(S("0123456789")), S("0123456789"));

	// punct
	ck_assert_tok_eq(casefold(S("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~")),
			 S("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"));

	// space
	ck_assert_tok_eq(casefold(S("\t\n\v\f\r ")), S("\t\n\v\f\r "));
}
END_TEST


START_TEST(test_fold_dash)
{
	ck_assert_tok_eq(get_type(S("-"), TYPE_DASHFOLD), S("-"));
	ck_assert_tok_eq(get_type(T("\\u058A"), TYPE_DASHFOLD), S("-"));
	ck_assert_tok_eq(get_type(T("\\u2212"), TYPE_DASHFOLD), S("-"));
	ck_assert_tok_eq(get_type(T("\\u2E3A"), TYPE_DASHFOLD), S("-"));
	ck_assert_tok_eq(get_type(T("\\u2E3B"), TYPE_DASHFOLD), S("-"));
	ck_assert_tok_eq(get_type(T("\\uFF0D"), TYPE_DASHFOLD), S("-"));
}
END_TEST


START_TEST(test_nofold_dash)
{
	ck_assert_tok_eq(get_type(S("-"), 0), S("-"));
	ck_assert_tok_eq(get_type(T("\\u058A"), 0), S("\xD6\x8A"));
	ck_assert_tok_eq(get_type(T("\\u2212"), 0), S("\xE2\x88\x92"));
	ck_assert_tok_eq(get_type(T("\\u2E3A"), 0), S("\xE2\xB8\xBA"));
	ck_assert_tok_eq(get_type(T("\\u2E3B"), 0), S("\xE2\xB8\xBB"));
	ck_assert_tok_eq(get_type(T("\\uFF0D"), 0), S("\xEF\xBC\x8D"));
}
END_TEST


START_TEST(test_fold_quote)
{
	ck_assert_tok_eq(get_type(S("'"), TYPE_QUOTFOLD), S("'"));
	ck_assert_tok_eq(get_type(S("\""), TYPE_QUOTFOLD), S("'"));
	ck_assert_tok_eq(get_type(T("\\u2018"), TYPE_QUOTFOLD), S("'"));
	ck_assert_tok_eq(get_type(T("\\u2019"), TYPE_QUOTFOLD), S("'"));
	ck_assert_tok_eq(get_type(T("\\u201A"), TYPE_QUOTFOLD), S("'"));
	ck_assert_tok_eq(get_type(T("\\u201F"), TYPE_QUOTFOLD), S("'"));
}
END_TEST


START_TEST(test_nofold_quote)
{
	ck_assert_tok_eq(get_type(S("'"), 0), S("'"));
	ck_assert_tok_eq(get_type(S("\""), 0), S("\""));
	ck_assert_tok_eq(get_type(T("\\u2018"), 0), S("\xE2\x80\x98"));
	ck_assert_tok_eq(get_type(T("\\u2019"), 0), S("\xE2\x80\x99"));
	ck_assert_tok_eq(get_type(T("\\u201A"), 0), S("\xE2\x80\x9A"));
	ck_assert_tok_eq(get_type(T("\\u201F"), 0), S("\xE2\x80\x9F"));
}
END_TEST


Suite *token_suite(void)
{
        Suite *s;
        TCase *tc;

        s = suite_create("token");
        tc = tcase_create("compare");
        tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_test(tc, test_equals);
	tcase_add_test(tc, test_not_equals);
	tcase_add_test(tc, test_equals_esc);
        suite_add_tcase(s, tc);

        tc = tcase_create("normalize");
        tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_test(tc, test_typ_basic);
	tcase_add_test(tc, test_typ_esc);
	tcase_add_test(tc, test_rm_control_ascii);
	tcase_add_test(tc, test_keep_control_ascii);
	tcase_add_test(tc, test_rm_control_utf8);
	tcase_add_test(tc, test_keep_control_utf8);
	tcase_add_test(tc, test_rm_ws_ascii);
	tcase_add_test(tc, test_keep_ws_ascii);
	tcase_add_test(tc, test_rm_ws_utf8);
	tcase_add_test(tc, test_keep_ws_utf8);
	tcase_add_test(tc, test_casefold_ascii);
	tcase_add_test(tc, test_fold_dash);
	tcase_add_test(tc, test_nofold_dash);
	tcase_add_test(tc, test_fold_quote);
	tcase_add_test(tc, test_nofold_quote);
        suite_add_tcase(s, tc);

        return s;
}


int main(void)
{
        int number_failed;
        Suite *s;
        SRunner *sr;

	openlog("check_token", LOG_CONS | LOG_PERROR | LOG_PID, LOG_USER);

        s = token_suite();
        sr = srunner_create(s);

        srunner_run_all(sr, CK_NORMAL);
        number_failed = srunner_ntests_failed(sr);
        srunner_free(sr);

        closelog();

        return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
