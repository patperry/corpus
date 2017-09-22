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
#include "../src/table.h"
#include "../src/text.h"
#include "../src/textset.h"
#include "../src/stem.h"
#include "../src/typemap.h"
#include "../src/unicode.h"
#include "testutil.h"


#define TYPE_MAPCASE	CORPUS_TYPE_MAPCASE
#define TYPE_MAPCOMPAT	CORPUS_TYPE_MAPCOMPAT
#define TYPE_MAPQUOTE	CORPUS_TYPE_MAPQUOTE
#define TYPE_RMDI	CORPUS_TYPE_RMDI


struct corpus_text *get_type_stem(const struct corpus_text *tok, int flags,
				  const char *stemmer)
{
	struct corpus_text *typ;
	struct corpus_typemap map;
	struct corpus_stem_snowball snowball;
	size_t size;

	if (stemmer) {
		ck_assert(!corpus_stem_snowball_init(&snowball, stemmer));
		ck_assert(!corpus_typemap_init(&map, flags,
					       corpus_stem_snowball,
					       &snowball));
	} else {
		ck_assert(!corpus_typemap_init(&map, flags, NULL, NULL));
	}

	ck_assert(!corpus_typemap_set(&map, tok));
	if (!map.has_type) {
		typ = NULL;
	} else {
		size = CORPUS_TEXT_SIZE(&map.type);
		typ = alloc(sizeof(*typ));

		typ->ptr = alloc(size + 1);
		memcpy(typ->ptr, map.type.ptr, size);
		typ->ptr[size] = '\0';
		typ->attr = map.type.attr;
	}

	corpus_typemap_destroy(&map);
	if (stemmer) {
		corpus_stem_snowball_destroy(&snowball);
	}

	return typ;
}


struct corpus_text *get_type(const struct corpus_text *tok, int flags)
{
	return get_type_stem(tok, flags, NULL);
}


struct corpus_text *casefold(const struct corpus_text *tok)
{
	return get_type(tok, TYPE_MAPCASE);
}


struct corpus_text *stem_en(const struct corpus_text *tok)
{
	return get_type_stem(tok, 0, "english");
}


START_TEST(test_equals)
{
	assert_text_eq(S("hello"), S("hello"));
	assert_text_eq(S("hello"), T("hello"));

	assert_text_eq(S(""), S(""));
	assert_text_eq(S(""), T(""));
}
END_TEST


START_TEST(test_not_equals)
{
	assert_text_ne(S("foo"), S("bar"));
}
END_TEST


START_TEST(test_equals_esc)
{
	assert_text_ne(S("\n"), T("\\n"));
	assert_text_ne(S("\\n"), T("\\n"));
}
END_TEST


START_TEST(test_typ_basic)
{
	assert_text_eq(get_type(S("hello"), 0), S("hello"));
	assert_text_eq(get_type(S("world"), 0), T("world"));
	assert_text_eq(get_type(T("foo"), 0), S("foo"));
}
END_TEST


START_TEST(test_typ_esc)
{
	// backslash
	assert_text_eq(get_type(S("\\"), 0), S("\\"));
	assert_text_eq(get_type(T("\\\\"), 0), S("\\"));
	assert_text_eq(get_type(T("\\u005C"), 0), S("\\"));
	assert_text_eq(get_type(S("\\\\"), 0), S("\\\\"));
	assert_text_eq(get_type(S("\\u005C"), TYPE_MAPCASE), S("\\u005c"));

	// quote (")
	assert_text_eq(get_type(S("'"), TYPE_MAPQUOTE), S("'"));
	assert_text_eq(get_type(T("'"), TYPE_MAPQUOTE), S("'"));
	assert_text_eq(get_type(S("\""), TYPE_MAPQUOTE), S("\""));
	assert_text_eq(get_type(T("\\\""), TYPE_MAPQUOTE), S("\""));
	assert_text_eq(get_type(T("\\u2019"), TYPE_MAPQUOTE), S("\'"));
	assert_text_eq(get_type(T("\\u201c"), TYPE_MAPQUOTE), S("\""));
	assert_text_eq(get_type(S("\\\'"), TYPE_MAPQUOTE), S("\\\'"));
	assert_text_eq(get_type(S("\\u2019"), TYPE_MAPQUOTE), S("\\u2019"));
}
END_TEST


START_TEST(test_keep_control_ascii)
{
	const struct corpus_text *js, *t;
	char str[256];
	uint8_t i;

	assert_text_eq(get_type(S("\a"), 0), S("\a"));
	assert_text_eq(get_type(S("\b"), 0), S("\b"));

	// C0
	for (i = 1; i < 0x20; i++) {
		if (0x09 <= i && i <= 0x0D) {
			continue;
		}
		str[0] = (char)i; str[1] = '\0';
		t = S(str);
		assert_text_eq(get_type(t, 0), t);

		sprintf(str, "\\u%04X", i);
		js = T(str);
		assert_text_eq(get_type(js, 0), t);
	}

	// delete
	assert_text_eq(get_type(S("\x7F"), 0), S("\x7F"));
	assert_text_eq(get_type(T("\\u007F"), 0), S("\x7F"));
}
END_TEST


START_TEST(test_keep_control_utf8)
{
	const struct corpus_text *t, *js;
	uint8_t str[256];
	uint8_t i;

	// C1
	for (i = 0x80; i < 0xA0; i++) {
		if (i == 0x85) {
			continue;
		}

		str[0] = 0xC2; str[1] = i; str[2] = '\0';
		t = S((char *)str);
		assert_text_eq(get_type(t, 0), t);

		sprintf((char *)str, "\\u%04X", i);
		js = T((char *)str);
		assert_text_eq(get_type(js, 0), t);
	}
}
END_TEST


START_TEST(test_keep_ws_ascii)
{
	assert_text_eq(get_type(S("\t"), 0), S("\t"));
	assert_text_eq(get_type(S("\n"), 0), S("\n"));
	assert_text_eq(get_type(S("\v"), 0), S("\v"));
	assert_text_eq(get_type(S("\f"), 0), S("\f"));
	assert_text_eq(get_type(S("\r"), 0), S("\r"));
	assert_text_eq(get_type(S(" "), 0), S(" "));
}
END_TEST


START_TEST(test_keep_ws_utf8)
{
	const struct corpus_text *t, *js, *typ;
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
		corpus_encode_utf8(ws[i], &buf);
		*buf = '\0';
		t = S((char *)str);
		assert_text_eq(get_type(t, TYPE_MAPCOMPAT), typ);

		sprintf((char *)str, "\\u%04x", ws[i]);
		js = T((char *)str);
		assert_text_eq(get_type(js, TYPE_MAPCOMPAT), typ);
	}
}
END_TEST


// removed the following features from typemap, no need to test
#if 0

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

	assert_text_eq(get_type(S("\a"), TYPE_RMCC), S(""));
	assert_text_eq(get_type(S("\b"), TYPE_RMCC), S(""));
	assert_text_eq(get_type(S("\t"), TYPE_RMCC), S("\t"));
	assert_text_eq(get_type(S("\n"), TYPE_RMCC), S("\n"));
	assert_text_eq(get_type(S("\v"), TYPE_RMCC), S("\v"));
	assert_text_eq(get_type(S("\f"), TYPE_RMCC), S("\f"));
	assert_text_eq(get_type(S("\r"), TYPE_RMCC), S("\r"));

	// C0
	for (i = 1; i < 0x20; i++) {
		if (0x09 <= i && i <= 0x0D) {
			continue;
		}

		str[0] = (char)i; str[1] = '\0';
		assert_text_eq(get_type(S(str), TYPE_RMCC), S(""));

		sprintf(str, "\\u%04X", i);
		assert_text_eq(get_type(T(str), TYPE_RMCC), S(""));
	}

	// delete
	assert_text_eq(get_type(S("\x7F"), TYPE_RMCC), S(""));
	assert_text_eq(get_type(T("\\u007F"), TYPE_RMCC), S(""));
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
		assert_text_eq(get_type(S((char *)str), TYPE_RMCC), S(""));

		sprintf((char *)str, "\\u%04X", i);
		assert_text_eq(get_type(T((char *)str), TYPE_RMCC), S(""));
	}
}
END_TEST


START_TEST(test_rm_ws_ascii)
{
	assert_text_eq(get_type(S("\t"), TYPE_RMWS), S(""));
	assert_text_eq(get_type(S("\n"), TYPE_RMWS), S(""));
	assert_text_eq(get_type(S("\v"), TYPE_RMWS), S(""));
	assert_text_eq(get_type(S("\f"), TYPE_RMWS), S(""));
	assert_text_eq(get_type(S("\r"), TYPE_RMWS), S(""));
	assert_text_eq(get_type(S(" "), TYPE_RMWS), S(""));
}
END_TEST


START_TEST(test_rm_ws_utf8)
{
	const struct corpus_text *t, *js;
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
		corpus_encode_utf8(ws[i], &buf);
		*buf = '\0';
		t = S((char *)str);
		assert_text_eq(get_type(t, TYPE_RMWS), S(""));

		sprintf((char *)str, "\\u%04x", ws[i]);
		js = T((char *)str);
		assert_text_eq(get_type(js, TYPE_RMWS), S(""));
	}
}
END_TEST

#endif

START_TEST(test_casefold_ascii)
{
	const struct corpus_text *tok;
	uint8_t buf[2] = { 0, 0 };
	uint8_t i;

	assert_text_eq(casefold(S("UPPER CASE")), S("upper case"));
	assert_text_eq(casefold(S("lower case")), S("lower case"));
	assert_text_eq(casefold(S("mIxEd CaSe")), S("mixed case"));

	for (i = 0x01; i < 'A'; i++) {
		buf[0] = i;
		tok = S((char *)buf);
		assert_text_eq(casefold(tok), tok);
	}
	for (i = 'Z' + 1; i < 0x7F; i++) {
		buf[0] = i;
		tok = S((char *)buf);
		assert_text_eq(casefold(tok), tok);
	}

	// upper
	assert_text_eq(casefold(S("ABCDEFGHIJKLMNOPQRSTUVWXYZ")),
			 S("abcdefghijklmnopqrstuvwxyz"));

	// lower
	assert_text_eq(casefold(S("abcdefghijklmnopqrstuvwxyz")),
			 S("abcdefghijklmnopqrstuvwxyz"));

	// digit
	assert_text_eq(casefold(S("0123456789")), S("0123456789"));

	// punct
	assert_text_eq(casefold(S("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~")),
			 S("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"));

	// space
	assert_text_eq(casefold(S("\t\n\v\f\r ")), S("\t\n\v\f\r "));
}
END_TEST


START_TEST(test_casefold_utf8)
{
	assert_text_eq(casefold(T("\u1e9e")), T("ss")); // capital eszett
	assert_text_eq(casefold(T("\u00df")), T("ss")); // lowercase eszett
}
END_TEST


// removed this feature
#if 0

START_TEST(test_fold_dash)
{
	assert_text_eq(get_type(S("-"), TYPE_DASHFOLD), S("-"));
	assert_text_eq(get_type(T("\\u058A"), TYPE_DASHFOLD), S("-"));
	assert_text_eq(get_type(T("\\u2212"), TYPE_DASHFOLD), S("-"));
	assert_text_eq(get_type(T("\\u2E3A"), TYPE_DASHFOLD), S("-"));
	assert_text_eq(get_type(T("\\u2E3B"), TYPE_DASHFOLD), S("-"));
	assert_text_eq(get_type(T("\\uFF0D"), TYPE_DASHFOLD), S("-"));
}
END_TEST


START_TEST(test_nofold_dash)
{
	assert_text_eq(get_type(S("-"), 0), S("-"));
	assert_text_eq(get_type(T("\\u058A"), 0), S("\xD6\x8A"));
	assert_text_eq(get_type(T("\\u2212"), 0), S("\xE2\x88\x92"));
	assert_text_eq(get_type(T("\\u2E3A"), 0), S("\xE2\xB8\xBA"));
	assert_text_eq(get_type(T("\\u2E3B"), 0), S("\xE2\xB8\xBB"));
	assert_text_eq(get_type(T("\\uFF0D"), 0), S("\xEF\xBC\x8D"));
}
END_TEST

#endif


START_TEST(test_map_quote)
{
	assert_text_eq(get_type(S("'"), TYPE_MAPQUOTE), S("'"));
	assert_text_eq(get_type(S("\""), TYPE_MAPQUOTE), S("\""));
	assert_text_eq(get_type(T("\\u2018"), TYPE_MAPQUOTE), S("'"));
	assert_text_eq(get_type(T("\\u2019"), TYPE_MAPQUOTE), S("'"));
	assert_text_eq(get_type(T("\\u201C"), TYPE_MAPQUOTE), S("\""));
	assert_text_eq(get_type(T("\\u201D"), TYPE_MAPQUOTE), S("\""));
}
END_TEST


START_TEST(test_nomap_quote)
{
	assert_text_eq(get_type(S("'"), 0), S("'"));
	assert_text_eq(get_type(S("\""), 0), S("\""));
	assert_text_eq(get_type(T("\\u2018"), 0), S("\xE2\x80\x98"));
	assert_text_eq(get_type(T("\\u2019"), 0), S("\xE2\x80\x99"));
	assert_text_eq(get_type(T("\\u201A"), 0), S("\xE2\x80\x9A"));
	assert_text_eq(get_type(T("\\u201F"), 0), S("\xE2\x80\x9F"));
}
END_TEST


START_TEST(test_stem_en)
{
	assert_text_eq(stem_en(S("consign")), S("consign"));
	assert_text_eq(stem_en(S("consigned")), S("consign"));
	assert_text_eq(stem_en(S("consigning")), S("consign"));
	assert_text_eq(stem_en(S("consignment")), S("consign"));

	assert_text_eq(stem_en(S("consolation")), S("consol"));
	assert_text_eq(stem_en(S("consolations")), S("consol"));
	assert_text_eq(stem_en(S("consolatory")), S("consolatori"));
	assert_text_eq(stem_en(S("console")), S("consol"));

	assert_text_eq(stem_en(S("")), S(""));
}
END_TEST


START_TEST(test_stopwords_en)
{
	int len;
	const uint8_t **words = corpus_stopword_list("english", &len);

	ck_assert_int_eq(len, 175);
	assert_text_eq(S((char *)words[0]), S("a"));
	assert_text_eq(S((char *)words[len - 1]), S("yourselves"));
	ck_assert(words[len] == NULL);
}
END_TEST


START_TEST(test_stopwords_unknown)
{
	int len;
	const uint8_t **words = corpus_stopword_list("gibberish", &len);

	ck_assert(words == NULL);
	ck_assert_int_eq(len, 0);
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
	// tcase_add_test(tc, test_rm_control_ascii);
	tcase_add_test(tc, test_keep_control_ascii);
	// tcase_add_test(tc, test_rm_control_utf8);
	tcase_add_test(tc, test_keep_control_utf8);
	// tcase_add_test(tc, test_rm_ws_ascii);
	tcase_add_test(tc, test_keep_ws_ascii);
	// tcase_add_test(tc, test_rm_ws_utf8);
	tcase_add_test(tc, test_keep_ws_utf8);
	tcase_add_test(tc, test_casefold_ascii);
	tcase_add_test(tc, test_casefold_utf8);
	// tcase_add_test(tc, test_fold_dash);
	// tcase_add_test(tc, test_nofold_dash);
	tcase_add_test(tc, test_map_quote);
	tcase_add_test(tc, test_nomap_quote);
	suite_add_tcase(s, tc);

	tc = tcase_create("stem");
	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_test(tc, test_stem_en);
	suite_add_tcase(s, tc);

	tc = tcase_create("stem");
	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_test(tc, test_stopwords_en);
	tcase_add_test(tc, test_stopwords_unknown);
	suite_add_tcase(s, tc);

	return s;
}


int main(void)
{
        int number_failed;
        Suite *s;
        SRunner *sr;

        s = token_suite();
        sr = srunner_create(s);

        srunner_run_all(sr, CK_NORMAL);
        number_failed = srunner_ntests_failed(sr);
        srunner_free(sr);

        return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
