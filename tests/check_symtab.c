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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include "../lib/utf8lite/src/utf8lite.h"
#include "../src/table.h"
#include "../src/textset.h"
#include "../src/symtab.h"
#include "testutil.h"

struct corpus_symtab tab;


void setup_empty_symtab(void)
{
	setup();
	corpus_symtab_init(&tab, (UTF8LITE_TEXTMAP_CASE
				  | UTF8LITE_TEXTMAP_COMPAT
				  | UTF8LITE_TEXTMAP_RMDI));
}


void teardown_symtab(void)
{
	corpus_symtab_destroy(&tab);
	teardown();
}


int has_type(const struct utf8lite_text *typ)
{
	int ntoken = tab.ntoken;
	int ntype = tab.ntype;
	int i, type_id;
	int ans;

	// after searching for a type
	ans = corpus_symtab_has_type(&tab, typ, &type_id);

	// it should leave the token count unchanged
	ck_assert_int_eq(tab.ntoken, ntoken);

	// it should leave the type count unchanged
	ck_assert_int_eq(tab.ntype, ntype);

	if (ans) {
		// when the return value is true
		//   it should return a valid type id
		ck_assert(type_id < tab.ntype);

		// it should return a type id matching the query
		assert_text_eq(&tab.types[type_id].text, typ);
	} else {
		// when the return value is 'false'
		//   the type should not be in the table
		for (i = 0; i < tab.ntype; i++) {
			assert_text_ne(&tab.types[i].text, typ);
		}
	}

	return ans;
}


int has_token(const struct utf8lite_text *tok)
{
	int ntoken = tab.ntoken;
	int ntype = tab.ntype;
	int i, tok_id;
	int ans;

	// after searching for a token
	ans = corpus_symtab_has_token(&tab, tok, &tok_id);

	// it should leave the token count unchanged
	ck_assert_int_eq(tab.ntoken, ntoken);

	// it should leave the type count unchanged
	ck_assert_int_eq(tab.ntype, ntype);

	if (ans) {
		// when the return value is true
		//   it should return a valid token id
		ck_assert(tok_id < tab.ntoken);

		// it should return a token id matching the query
		assert_text_eq(&tab.tokens[tok_id].text, tok);
	} else {
		// when the return value is 'false'
		//   the token should not be in the table
		for (i = 0; i < tab.ntoken; i++) {
			assert_text_ne(&tab.tokens[i].text, tok);
		}
	}

	return ans;
}



int add_type(const struct utf8lite_text *typ)
{
	int type_id;
	int ntoken = tab.ntoken;
	int ntype = tab.ntype;
	bool had_type = corpus_symtab_has_type(&tab, typ, NULL);

	// after adding a type
	corpus_symtab_add_type(&tab, typ, &type_id);

	// it should leave the token count unchanged
	ck_assert_int_eq(tab.ntoken, ntoken);

	if (had_type) {
		// when the type already existed,
		//   it should leave the type count unchanged
		ck_assert_int_eq(tab.ntype, ntype);
	} else {
		// otherwise,
		//   it should increment the type count
		ck_assert_int_eq(tab.ntype, ntype + 1);
	}

	// it should return a valid id
	ck_assert(type_id < tab.ntype);

	// it should return an id matching the insert
	assert_text_eq(&tab.types[type_id].text, typ);

	if (!had_type) {
		// when the type is new
		//   it should have an empty token set
		ck_assert_int_eq(tab.types[type_id].ntoken, 0);
	}

	return type_id;
}


int add_token(const struct utf8lite_text *tok)
{
	int i, token_id, type_id;
	int ntoken = tab.ntoken;
	int ntype = tab.ntype;
	int type_ntoken;
	bool had_token = corpus_symtab_has_token(&tab, tok, &token_id);

	if (had_token) {
		type_id = tab.tokens[token_id].type_id;
		type_ntoken = tab.types[type_id].ntoken;
	} else {
		type_id = -1;
		type_ntoken = 0;
	}

	// after adding a token
	corpus_symtab_add_token(&tab, tok, &token_id);

	if (had_token) {
		// when the token already existed,
		//   it should leave the token count unchanged
		ck_assert_int_eq(tab.ntoken, ntoken);

		//   it should leave the type count unchanged
		ck_assert_int_eq(tab.ntype, ntype);

		//   it should leave the type's token count unchanged
		ck_assert_int_eq(tab.types[type_id].ntoken, type_ntoken);
	} else {
		// otherwise,
		//   it should increment the token count
		ck_assert_int_eq(tab.ntoken, ntoken + 1);
	}

	// the returned token
	// (a) should have a valid id
	ck_assert(token_id < tab.ntoken);

	// (b) should match the insert
	assert_text_eq(&tab.tokens[token_id].text, tok);

	// (c) should should have a valid type id
	ck_assert(tab.tokens[token_id].type_id < tab.ntype);

	// (d) should be a member of the indicated type
	type_id = tab.tokens[token_id].type_id;
	for (i = 0; i < tab.types[type_id].ntoken; i++) {
		if (tab.types[type_id].token_ids[i] == token_id) {
			break;
		}
	}
	ck_assert(i < tab.types[type_id].ntoken);

	// (e) it should only appear once in the types token set
	for (i = i + 1; i < tab.types[type_id].ntoken; i++) {
		if (tab.types[type_id].token_ids[i] == token_id) {
			break;
		}
	}
	ck_assert_int_eq(i, tab.types[type_id].ntoken);

	return token_id;
}


START_TEST(test_empty_init)
{
	ck_assert_int_eq(tab.ntype, 0);
	ck_assert_int_eq(tab.ntoken, 0);
}
END_TEST


START_TEST(test_empty_has_typ)
{
	ck_assert(!has_type(T("")));
	ck_assert(!has_type(T("foo")));
	ck_assert(!has_type(T("bar")));
}
END_TEST


START_TEST(test_empty_has_tok)
{
	ck_assert(!has_token(T("")));
	ck_assert(!has_token(T("foo")));
	ck_assert(!has_token(T("bar")));
}
END_TEST


START_TEST(test_add_typ)
{
	add_type(T("type"));
	ck_assert(has_type(T("type")));
}
END_TEST


START_TEST(test_add_tok)
{
	add_token(T("token"));
	ck_assert(has_token(T("token")));
}
END_TEST


START_TEST(test_add_empty_typ)
{
	add_type(T(""));
	ck_assert(has_type(T("")));
}
END_TEST


START_TEST(test_add_empty_tok)
{
	add_token(T(""));
	ck_assert(has_token(T("")));
}
END_TEST


START_TEST(test_twice_add_typ)
{
	int id1, id2;
	id1 = add_type(T("repeated type"));
	id2 = add_type(T("repeated type"));
	ck_assert_int_eq(id1, id2);
}
END_TEST


START_TEST(test_twice_add_tok)
{
	int id1, id2;
	id1 = add_token(T("repeated token"));
	id2 = add_token(T("repeated token"));
	ck_assert_int_eq(id1, id2);
}
END_TEST


START_TEST(test_many_add_typ)
{
	char buf[256];
	int i, j, n = 100;
	struct utf8lite_text *typ;

	for (i = 0; i < n; i++) {
		sprintf(buf, "type %d", i);
		typ = T(buf);
		add_type(typ);

		for (j = 0; j <= i; j++) {
			sprintf(buf, "type %d", i);
			typ = T(buf);
			ck_assert(has_type(typ));
		}
	}
}
END_TEST


START_TEST(test_many_add_tok)
{
	char buf[256];
	int i, j, n = 100;
	struct utf8lite_text *tok;

	for (i = 0; i < n; i++) {
		sprintf(buf, "token %d", i);
		tok = T(buf);
		add_token(tok);

		for (j = 0; j <= i; j++) {
			sprintf(buf, "token %d", i);
			tok = T(buf);
			ck_assert(has_token(tok));
		}
	}
}
END_TEST


START_TEST(test_casefold)
{
	int tok_id1 = add_token(T("CrAzY_CaSiNg"));
	int tok_id2 = add_token(T("CRAZY_casing"));
	int tok_id3 = add_token(T("CRAZY_CASING"));
	int typ_id1 = tab.tokens[tok_id1].type_id;
	int typ_id2 = tab.tokens[tok_id2].type_id;
	int typ_id3 = tab.tokens[tok_id3].type_id;

	ck_assert(has_token(T("CrAzY_CaSiNg")));
	ck_assert(has_token(T("CRAZY_casing")));
	ck_assert(has_token(T("CRAZY_CASING")));
	ck_assert(has_type(T("crazy_casing")));

	assert_text_eq(&tab.types[typ_id1].text, T("crazy_casing"));
	ck_assert_int_eq(typ_id1, typ_id2);
	ck_assert_int_eq(typ_id1, typ_id3);
}
END_TEST


Suite *symtab_suite(void)
{
        Suite *s;
        TCase *tc;

        s = suite_create("symtab");
        tc = tcase_create("core");
        tcase_add_checked_fixture(tc, setup_empty_symtab, teardown_symtab);
        tcase_add_test(tc, test_empty_init);
        tcase_add_test(tc, test_empty_has_typ);
        tcase_add_test(tc, test_empty_has_tok);
        tcase_add_test(tc, test_add_typ);
        tcase_add_test(tc, test_add_tok);
        tcase_add_test(tc, test_add_empty_typ);
        tcase_add_test(tc, test_add_empty_tok);
        tcase_add_test(tc, test_twice_add_tok);
        tcase_add_test(tc, test_twice_add_typ);
        tcase_add_test(tc, test_many_add_typ);
        tcase_add_test(tc, test_many_add_tok);
        tcase_add_test(tc, test_casefold);
        suite_add_tcase(s, tc);

        return s;
}


int main(void)
{
        int number_failed;
        Suite *s;
        SRunner *sr;

        s = symtab_suite();
        sr = srunner_create(s);

        srunner_run_all(sr, CK_NORMAL);
        number_failed = srunner_ntests_failed(sr);
        srunner_free(sr);

        return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
