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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "../src/table.h"
#include "../src/text.h"
#include "../src/token.h"
#include "../src/symtab.h"
#include "../src/datatype.h"
#include "testutil.h"

struct datatyper typer;


void setup_datatype(void)
{
	setup();
	datatyper_init(&typer);
}


void teardown_datatype(void)
{
	datatyper_destroy(&typer);
	teardown();
}


int scan(const char *str)
{
	size_t n = strlen(str);
	int id;

	datatyper_scan(&typer, (const uint8_t *)str, n, &id);

	return id;
}


int is_null(const char *str)
{
	return (scan(str) == DATATYPE_NULL);
}


int is_bool(const char *str)
{
	return (scan(str) == DATATYPE_BOOL);
}


int is_number(const char *str)
{
	return (scan(str) == DATATYPE_NUMBER);
}


int is_text(const char *str)
{
	return (scan(str) == DATATYPE_TEXT);
}


START_TEST(test_valid_null)
{
	ck_assert(is_null("null"));
	ck_assert(is_null(""));
	ck_assert(is_null("   "));
	ck_assert(is_null("  null"));
	ck_assert(is_null("    null "));
	ck_assert(is_null("null   "));
}
END_TEST


START_TEST(test_invalid_null)
{
	ck_assert(!is_null("n"));
	ck_assert(!is_null("nulll"));
	ck_assert(!is_null("null null"));
}
END_TEST


START_TEST(test_valid_bool)
{
	ck_assert(is_bool("false"));
	ck_assert(is_bool("true"));

	ck_assert(is_bool("  true"));
	ck_assert(is_bool("    true "));
	ck_assert(is_bool("false   "));
}
END_TEST


START_TEST(test_invalid_bool)
{
	ck_assert(!is_bool(""));
	ck_assert(!is_bool("tru"));
	ck_assert(!is_bool("true1"));
	ck_assert(!is_bool("false (really)"));
}
END_TEST


START_TEST(test_valid_number)
{
	ck_assert(is_number("31337"));
	ck_assert(is_number("123"));
	ck_assert(is_number("-1"));
	ck_assert(is_number("0"));
	ck_assert(is_number("99"));
	ck_assert(is_number("0.1"));
	ck_assert(is_number("1e17"));
	ck_assert(is_number("2.02e-07"));
	ck_assert(is_number("5.0e+006"));
}
END_TEST


START_TEST(test_valid_nonfinite)
{
	ck_assert(is_number("Infinity"));
	ck_assert(is_number("-Infinity"));
	ck_assert(is_number("NaN"));
	ck_assert(is_number("-NaN"));
}
END_TEST


START_TEST(test_invalid_number)
{
	ck_assert(!is_number("1a"));
	ck_assert(!is_number("+1"));
	ck_assert(!is_number("-"));
	ck_assert(!is_number("01"));
	ck_assert(!is_number("1."));
	ck_assert(!is_number(".1"));
	ck_assert(!is_number("-.1"));
}
END_TEST


START_TEST(test_valid_text)
{
	ck_assert(is_text("\"hello\""));
	ck_assert(is_text("\"hello world\""));
}
END_TEST


START_TEST(test_invalid_text)
{
	ck_assert(!is_text("hello"));
	ck_assert(!is_text("\"hello\" world"));
}
END_TEST


Suite *datatype_suite(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("datatype");

	tc = tcase_create("null");
        tcase_add_checked_fixture(tc, setup_datatype, teardown_datatype);
	tcase_add_test(tc, test_valid_null);
	tcase_add_test(tc, test_invalid_null);
	suite_add_tcase(s, tc);

	tc = tcase_create("bool");
        tcase_add_checked_fixture(tc, setup_datatype, teardown_datatype);
	tcase_add_test(tc, test_valid_bool);
	tcase_add_test(tc, test_invalid_bool);
	suite_add_tcase(s, tc);

	tc = tcase_create("number");
        tcase_add_checked_fixture(tc, setup_datatype, teardown_datatype);
	tcase_add_test(tc, test_valid_number);
	tcase_add_test(tc, test_valid_nonfinite);
	tcase_add_test(tc, test_invalid_number);
	suite_add_tcase(s, tc);

	tc = tcase_create("text");
        tcase_add_checked_fixture(tc, setup_datatype, teardown_datatype);
	tcase_add_test(tc, test_valid_text);
	tcase_add_test(tc, test_invalid_text);
	suite_add_tcase(s, tc);

	return s;
}


int main(void)
{
	int nfail;
	Suite *s;
	SRunner *sr;

	openlog("check_datatype", LOG_CONS | LOG_PERROR | LOG_PID, LOG_USER);
        setlogmask(LOG_UPTO(LOG_INFO));
        setlogmask(LOG_UPTO(LOG_DEBUG));

	s = datatype_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	nfail = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (nfail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
