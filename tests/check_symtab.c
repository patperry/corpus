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
#include <syslog.h>
#include <check.h>
#include "../src/table.h"
#include "../src/text.h"
#include "../src/token.h"
#include "../src/symtab.h"
#include "testutil.h"

struct symtab tab;


void setup_empty_symtab(void)
{
	setup();
	symtab_init(&tab, TYPE_COMPAT | TYPE_CASEFOLD | TYPE_RMDI);
}


void teardown_symtab(void)
{
	symtab_destroy(&tab);
	teardown();
}


int has_typ(const struct text *typ)
{
	int ntok = tab.ntok;
	int ntyp = tab.ntyp;
	int i, typ_id;
	int ans;

	// after searching for a type
	ans = symtab_has_typ(&tab, typ, &typ_id);

	// it should leave the token count unchanged
	ck_assert_int_eq(tab.ntok, ntok);

	// it should leave the type count unchanged
	ck_assert_int_eq(tab.ntyp, ntyp);

	if (ans) {
		// when the return value is true
		//   it should return a valid type id
		ck_assert_int_lt(typ_id, tab.ntyp);

		// it should return a type id matching the query
		ck_assert_tok_eq(&tab.typs[typ_id].text, typ);
	} else {
		// when the return value is 'false'
		//   the type should not be in the table
		for (i = 0; i < tab.ntyp; i++) {
			ck_assert_tok_ne(&tab.typs[i].text, typ);
		}
	}

	return ans;
}


int has_tok(const struct text *tok)
{
	int ntok = tab.ntok;
	int ntyp = tab.ntyp;
	int i, tok_id;
	int ans;

	// after searching for a token
	ans = symtab_has_tok(&tab, tok, &tok_id);

	// it should leave the token count unchanged
	ck_assert_int_eq(tab.ntok, ntok);

	// it should leave the type count unchanged
	ck_assert_int_eq(tab.ntyp, ntyp);

	if (ans) {
		// when the return value is true
		//   it should return a valid token id
		ck_assert_int_lt(tok_id, tab.ntok);

		// it should return a token id matching the query
		ck_assert_tok_eq(&tab.toks[tok_id].text, tok);
	} else {
		// when the return value is 'false'
		//   the token should not be in the table
		for (i = 0; i < tab.ntok; i++) {
			ck_assert_tok_ne(&tab.toks[i].text, tok);
		}
	}

	return ans;
}



int add_typ(const struct text *typ)
{
	int typ_id;
	int ntok = tab.ntok;
	int ntyp = tab.ntyp;
	bool had_typ = symtab_has_typ(&tab, typ, NULL);

	// after adding a type
	symtab_add_typ(&tab, typ, &typ_id);

	// it should leave the token count unchanged
	ck_assert_int_eq(tab.ntok, ntok);

	if (had_typ) {
		// when the type already existed,
		//   it should leave the type count unchanged
		ck_assert_int_eq(tab.ntyp, ntyp);
	} else {
		// otherwise,
		//   it should increment the type count
		ck_assert_int_eq(tab.ntyp, ntyp + 1);
	}

	// it should return a valid id
	ck_assert_int_lt(typ_id, tab.ntyp);

	// it should return an id matching the insert
	ck_assert_tok_eq(&tab.typs[typ_id].text, typ);

	if (!had_typ) {
		// when the type is new
		//   it should have an empty token set
		ck_assert_int_eq(tab.typs[typ_id].ntok, 0);
	}

	return typ_id;
}


int add_tok(const struct text *tok)
{
	int i, tok_id, typ_id;
	int ntok = tab.ntok;
	int ntyp = tab.ntyp;
	int typ_ntok;
	bool had_tok = symtab_has_tok(&tab, tok, &tok_id);

	if (had_tok) {
		typ_id = tab.toks[tok_id].typ_id;
		typ_ntok = tab.typs[typ_id].ntok;
	} else {
		typ_ntok = 0;
	}

	// after adding a token
	symtab_add_tok(&tab, tok, &tok_id);

	if (had_tok) {
		// when the token already existed,
		//   it should leave the token count unchanged
		ck_assert_int_eq(tab.ntok, ntok);

		//   it should leave the type count unchanged
		ck_assert_int_eq(tab.ntyp, ntyp);

		//   it should leave the type's token count unchanged
		ck_assert_int_eq(tab.typs[typ_id].ntok, typ_ntok);
	} else {
		// otherwise,
		//   it should increment the token count
		ck_assert_int_eq(tab.ntok, ntok + 1);
	}

	// the returned token
	// (a) should have a valid id
	ck_assert_int_lt(tok_id, tab.ntok);

	// (b) should match the insert
	ck_assert_tok_eq(&tab.toks[tok_id].text, tok);

	// (c) should should have a valid type id
	ck_assert_int_lt(tab.toks[tok_id].typ_id, tab.ntyp);

	// (d) should be a member of the indicated type
	typ_id = tab.toks[tok_id].typ_id;
	for (i = 0; i < tab.typs[typ_id].ntok; i++) {
		if (tab.typs[typ_id].tok_ids[i] == tok_id) {
			break;
		}
	}
	ck_assert_int_lt(i, tab.typs[typ_id].ntok);

	// (e) it should only appear once in the types token set
	for (i = i + 1; i < tab.typs[typ_id].ntok; i++) {
		if (tab.typs[typ_id].tok_ids[i] == tok_id) {
			break;
		}
	}
	ck_assert_int_eq(i, tab.typs[typ_id].ntok);

	return tok_id;
}


START_TEST(test_empty_init)
{
	ck_assert_int_eq(tab.ntyp, 0);
	ck_assert_int_eq(tab.ntok, 0);
}
END_TEST


START_TEST(test_empty_has_typ)
{
	ck_assert(!has_typ(T("")));
	ck_assert(!has_typ(T("foo")));
	ck_assert(!has_typ(T("bar")));
}
END_TEST


START_TEST(test_empty_has_tok)
{
	ck_assert(!has_tok(T("")));
	ck_assert(!has_tok(T("foo")));
	ck_assert(!has_tok(T("bar")));
}
END_TEST


START_TEST(test_add_typ)
{
	add_typ(T("type"));
	ck_assert(has_typ(T("type")));
}
END_TEST


START_TEST(test_add_tok)
{
	add_tok(T("token"));
	ck_assert(has_tok(T("token")));
}
END_TEST


START_TEST(test_add_empty_typ)
{
	add_typ(T(""));
	ck_assert(has_typ(T("")));
}
END_TEST


START_TEST(test_add_empty_tok)
{
	add_tok(T(""));
	ck_assert(has_tok(T("")));
}
END_TEST


START_TEST(test_twice_add_typ)
{
	int id1, id2;
	id1 = add_typ(T("repeated type"));
	id2 = add_typ(T("repeated type"));
	ck_assert_int_eq(id1, id2);
}
END_TEST


START_TEST(test_twice_add_tok)
{
	int id1, id2;
	id1 = add_tok(T("repeated token"));
	id2 = add_tok(T("repeated token"));
	ck_assert_int_eq(id1, id2);
}
END_TEST


START_TEST(test_many_add_typ)
{
	char buf[256];
	int i, j, n = 100;
	struct text *typ;

	for (i = 0; i < n; i++) {
		sprintf(buf, "type %d", i);
		typ = T(buf);
		add_typ(typ);

		for (j = 0; j <= i; j++) {
			sprintf(buf, "type %d", i);
			typ = T(buf);
			ck_assert(has_typ(typ));
		}
	}
}
END_TEST


START_TEST(test_many_add_tok)
{
	char buf[256];
	int i, j, n = 100;
	struct text *tok;

	for (i = 0; i < n; i++) {
		sprintf(buf, "token %d", i);
		tok = T(buf);
		add_tok(tok);

		for (j = 0; j <= i; j++) {
			sprintf(buf, "token %d", i);
			tok = T(buf);
			ck_assert(has_tok(tok));
		}
	}
}
END_TEST


START_TEST(test_casefold)
{
	int tok_id1 = add_tok(T("CrAzY_CaSiNg"));
	int tok_id2 = add_tok(T("CRAZY_casing"));
	int tok_id3 = add_tok(T("CRAZY_CASING"));
	int typ_id1 = tab.toks[tok_id1].typ_id;
	int typ_id2 = tab.toks[tok_id2].typ_id;
	int typ_id3 = tab.toks[tok_id3].typ_id;

	ck_assert(has_tok(T("CrAzY_CaSiNg")));
	ck_assert(has_tok(T("CRAZY_casing")));
	ck_assert(has_tok(T("CRAZY_CASING")));
	ck_assert(has_typ(T("crazy_casing")));

	ck_assert_tok_eq(&tab.typs[typ_id1].text, T("crazy_casing"));
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

	openlog("corpus", LOG_CONS | LOG_PERROR | LOG_PID, LOG_USER);
	setlogmask(LOG_UPTO(LOG_DEBUG));

        s = symtab_suite();
        sr = srunner_create(s);

        srunner_run_all(sr, CK_NORMAL);
        number_failed = srunner_ntests_failed(sr);
        srunner_free(sr);

        closelog();

        return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
