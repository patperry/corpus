/*
 * Copyright 2016 Patrick O. Perry.
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

#define ck_assert_tok_eq(X, Y) do { \
	const struct text * _ck_x = (X); \
	const struct text * _ck_y = (Y); \
	ck_assert_msg(tok_equals(_ck_y, _ck_x), \
		"Assertion '%s == %s' failed: %s==\"%s\" (0x%zx)," \
		" %s==\"%s\" (0x%zx)", \
		#X, #Y, #X, _ck_x->ptr, _ck_x->attr, \
		#Y, _ck_y->ptr, _ck_y->attr); \
} while (0)

struct text *get_typ(const struct text *tok, int flags)
{
	struct text *typ;
	struct typbuf buf;
	size_t size;

	ck_assert(!typbuf_init(&buf, flags));
	ck_assert(!typbuf_set(&buf, tok));
	size = TEXT_SIZE(&buf.text);

	typ = alloc(sizeof(*typ));

	typ->ptr = alloc(size + 1);
	memcpy(typ->ptr, buf.text.ptr, size);
	typ->ptr[size] = '\0';
	typ->attr = buf.text.attr;

	typbuf_destroy(&buf);

	return typ;
}


struct text *casefold(const struct text *tok)
{
	return get_typ(tok, ~TYP_NOFOLD_CASE);
}


START_TEST(test_equals)
{
	ck_assert(tok_equals(S("hello"), S("hello")));
	ck_assert(tok_equals(S("hello"), T("hello")));

	ck_assert(tok_equals(S(""), S("")));
	ck_assert(tok_equals(S(""), T("")));
}
END_TEST


START_TEST(test_not_equals)
{
	ck_assert(!tok_equals(S("foo"), S("bar")));
}
END_TEST


START_TEST(test_equals_esc)
{
	ck_assert(!tok_equals(S("\n"), T("\\n")));
	ck_assert(!tok_equals(S("\\n"), T("\\n")));
}
END_TEST


START_TEST(test_typ_basic)
{
	ck_assert_tok_eq(get_typ(S("hello"), 0), S("hello"));
	ck_assert_tok_eq(get_typ(S("world"), 0), T("world"));
	ck_assert_tok_eq(get_typ(T("foo"), 0), S("foo"));
}
END_TEST


START_TEST(test_typ_esc)
{
	// backslash
	ck_assert_tok_eq(get_typ(S("\\"), 0), S("\\"));
	ck_assert_tok_eq(get_typ(T("\\\\"), 0), S("\\"));
	ck_assert_tok_eq(get_typ(T("\\u005C"), 0), S("\\"));
	ck_assert_tok_eq(get_typ(S("\\\\"), 0), S("\\\\"));
	ck_assert_tok_eq(get_typ(S("\\u005C"), 0), S("\\u005c")); // casefold

	// quote (")
	ck_assert_tok_eq(get_typ(S("\""), 0), S("\'")); // quote fold
	ck_assert_tok_eq(get_typ(T("\\\""), 0), S("\'"));
	ck_assert_tok_eq(get_typ(T("\\u0022"), 0), S("\'"));
	ck_assert_tok_eq(get_typ(S("\\\'"), 0), S("\\\'"));
	ck_assert_tok_eq(get_typ(S("\\u0022"), 0), S("\\u0022"));
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

	ck_assert_tok_eq(get_typ(S("\a"), 0), S(""));
	ck_assert_tok_eq(get_typ(S("\b"), 0), S(""));
	ck_assert_tok_eq(get_typ(S("\t"), 0), S(""));
	ck_assert_tok_eq(get_typ(S("\n"), 0), S(""));
	ck_assert_tok_eq(get_typ(S("\v"), 0), S(""));
	ck_assert_tok_eq(get_typ(S("\f"), 0), S(""));
	ck_assert_tok_eq(get_typ(S("\r"), 0), S(""));

	// C0
	for (i = 1; i < 0x20; i++) {
		str[0] = (char)i; str[1] = '\0';
		ck_assert_tok_eq(get_typ(S(str), 0), S(""));

		sprintf(str, "\\u%04X", i);
		ck_assert_tok_eq(get_typ(T(str), 0), S(""));
	}

	// delete
	ck_assert_tok_eq(get_typ(S("\x7F"), 0), S(""));
	ck_assert_tok_eq(get_typ(T("\\u007F"), 0), S(""));
}
END_TEST


START_TEST(test_rm_control_utf8)
{
	uint8_t str[256];
	uint8_t i;

	// C1: JSON
	for (i = 0x80; i < 0xA0; i++) {
		str[0] = 0xC2; str[1] = i; str[2] = '\0';
		ck_assert_tok_eq(get_typ(S((char *)str), 0), S(""));

		sprintf((char *)str, "\\u%04X", i);
		ck_assert_tok_eq(get_typ(T((char *)str), 0), S(""));
	}
}
END_TEST


START_TEST(test_keep_control_ascii)
{
	const struct text *js, *t;
	char str[256];
	uint8_t i;

	ck_assert_tok_eq(get_typ(S("\a"), TYP_KEEP_CC), S("\a"));
	ck_assert_tok_eq(get_typ(S("\b"), TYP_KEEP_CC), S("\b"));

	// C0
	for (i = 1; i < 0x20; i++) {
		if (0x09 <= i && i <= 0x0D) {
			continue;
		}
		str[0] = (char)i; str[1] = '\0';
		t = S(str);
		ck_assert_tok_eq(get_typ(t, TYP_KEEP_CC), t);

		sprintf(str, "\\u%04X", i);
		js = T(str);
		ck_assert_tok_eq(get_typ(js, TYP_KEEP_CC), t);
	}

	// delete
	ck_assert_tok_eq(get_typ(S("\x7F"), TYP_KEEP_CC), S("\x7F"));
	ck_assert_tok_eq(get_typ(T("\\u007F"), TYP_KEEP_CC), S("\x7F"));
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
		ck_assert_tok_eq(get_typ(t, TYP_KEEP_CC), t);

		sprintf((char *)str, "\\u%04X", i);
		js = T((char *)str);
		ck_assert_tok_eq(get_typ(js, TYP_KEEP_CC), t);
	}
}
END_TEST


START_TEST(test_keep_ws_ascii)
{
	ck_assert_tok_eq(get_typ(S("\t"), TYP_KEEP_WS), S("\t"));
	ck_assert_tok_eq(get_typ(S("\n"), TYP_KEEP_WS), S("\n"));
	ck_assert_tok_eq(get_typ(S("\v"), TYP_KEEP_WS), S("\v"));
	ck_assert_tok_eq(get_typ(S("\f"), TYP_KEEP_WS), S("\f"));
	ck_assert_tok_eq(get_typ(S("\r"), TYP_KEEP_WS), S("\r"));
	ck_assert_tok_eq(get_typ(S(" "), TYP_KEEP_WS), S(" "));
}
END_TEST


START_TEST(test_rm_ws_ascii)
{
	ck_assert_tok_eq(get_typ(S("\t"), 0), S(""));
	ck_assert_tok_eq(get_typ(S("\n"), 0), S(""));
	ck_assert_tok_eq(get_typ(S("\v"), 0), S(""));
	ck_assert_tok_eq(get_typ(S("\f"), 0), S(""));
	ck_assert_tok_eq(get_typ(S("\r"), 0), S(""));
	ck_assert_tok_eq(get_typ(S(" "), 0), S(""));
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
		ck_assert_tok_eq(get_typ(t, TYP_KEEP_WS), typ);

		sprintf((char *)str, "\\u%04x", ws[i]);
		js = T((char *)str);
		ck_assert_tok_eq(get_typ(js, TYP_KEEP_WS), typ);
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
		ck_assert_tok_eq(get_typ(t, 0), S(""));

		sprintf((char *)str, "\\u%04x", ws[i]);
		js = T((char *)str);
		ck_assert_tok_eq(get_typ(js, 0), S(""));
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
	ck_assert_tok_eq(get_typ(S("-"), 0), S("-"));
	ck_assert_tok_eq(get_typ(T("\\u058A"), 0), S("-"));
	ck_assert_tok_eq(get_typ(T("\\u2212"), 0), S("-"));
	ck_assert_tok_eq(get_typ(T("\\u2E3A"), 0), S("-"));
	ck_assert_tok_eq(get_typ(T("\\u2E3B"), 0), S("-"));
	ck_assert_tok_eq(get_typ(T("\\uFF0D"), 0), S("-"));
}
END_TEST


START_TEST(test_nofold_dash)
{
	ck_assert_tok_eq(get_typ(S("-"), TYP_NOFOLD_DASH), S("-"));
	ck_assert_tok_eq(get_typ(T("\\u058A"), TYP_NOFOLD_DASH),
			 S("\xD6\x8A"));
	ck_assert_tok_eq(get_typ(T("\\u2212"), TYP_NOFOLD_DASH),
			 S("\xE2\x88\x92"));
	ck_assert_tok_eq(get_typ(T("\\u2E3A"), TYP_NOFOLD_DASH),
			 S("\xE2\xB8\xBA"));
	ck_assert_tok_eq(get_typ(T("\\u2E3B"), TYP_NOFOLD_DASH),
			 S("\xE2\xB8\xBB"));
	ck_assert_tok_eq(get_typ(T("\\uFF0D"),
			         TYP_NOCOMPAT | TYP_NOFOLD_DASH),
			 S("\xEF\xBC\x8D"));
}
END_TEST


START_TEST(test_fold_quote)
{
	ck_assert_tok_eq(get_typ(S("'"), 0), S("'"));
	ck_assert_tok_eq(get_typ(S("\""), 0), S("'"));
	ck_assert_tok_eq(get_typ(T("\\u2018"), 0), S("'"));
	ck_assert_tok_eq(get_typ(T("\\u2019"), 0), S("'"));
	ck_assert_tok_eq(get_typ(T("\\u201A"), 0), S("'"));
	ck_assert_tok_eq(get_typ(T("\\u201F"), 0), S("'"));
}
END_TEST


START_TEST(test_nofold_quote)
{
	ck_assert_tok_eq(get_typ(S("'"), TYP_NOFOLD_QUOT), S("'"));
	ck_assert_tok_eq(get_typ(S("\""), TYP_NOFOLD_QUOT), S("\""));
	ck_assert_tok_eq(get_typ(T("\\u2018"), TYP_NOFOLD_QUOT),
			 S("\xE2\x80\x98"));
	ck_assert_tok_eq(get_typ(T("\\u2019"), TYP_NOFOLD_QUOT),
			 S("\xE2\x80\x99"));
	ck_assert_tok_eq(get_typ(T("\\u201A"), TYP_NOFOLD_QUOT),
			 S("\xE2\x80\x9A"));
	ck_assert_tok_eq(get_typ(T("\\u201F"), TYP_NOFOLD_QUOT),
			 S("\xE2\x80\x9F"));
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
