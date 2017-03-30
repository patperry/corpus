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
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "../src/errcode.h"
#include "../src/table.h"
#include "../src/text.h"
#include "../src/token.h"
#include "../src/symtab.h"
#include "../src/datatype.h"
#include "testutil.h"

struct schema schema;

const int Null = DATATYPE_NULL;
const int Bool = DATATYPE_BOOL;
const int Number = DATATYPE_NUMBER;
const int Text = DATATYPE_TEXT;
const int Any = DATATYPE_ANY;

void setup_datatype(void)
{
	setup();
	schema_init(&schema);
}


void teardown_datatype(void)
{
	schema_destroy(&schema);
	teardown();
}


int get_name(const char *str)
{
	int id;
	ck_assert(!schema_name(&schema, S(str), &id));
	return id;
}


int get_type(const char *str)
{
	size_t n = strlen(str);
	int id;
	ck_assert(!schema_scan(&schema, (const uint8_t *)str, n, &id));
	return id;
}


int is_error(const char *str)
{
	size_t n = strlen(str);
	int err, id;
	err = schema_scan(&schema, (const uint8_t *)str, n, &id);
	ck_assert(err == ERROR_INVAL || err == 0);
	return (err != 0);
}


int is_null(const char *str)
{
	return (get_type(str) == DATATYPE_NULL);
}


int is_bool(const char *str)
{
	return (get_type(str) == DATATYPE_BOOL);
}


int is_number(const char *str)
{
	return (get_type(str) == DATATYPE_NUMBER);
}


int is_text(const char *str)
{
	return (get_type(str) == DATATYPE_TEXT);
}


int Array(int element_id, int length)
{
	int id;
	ck_assert(!schema_array(&schema, element_id, length, &id));
	return id;
}


int Record(int nfield, ...)
{
	int *name_ids = alloc(nfield * sizeof(*name_ids));
	int *type_ids = alloc(nfield * sizeof(*type_ids));
	const char *name_str;
	int i, id;
	va_list ap;

	va_start(ap, nfield);

	for (i = 0; i < nfield; i++) {
		name_str = va_arg(ap, const char *);
		name_ids[i] = get_name(name_str);
		type_ids[i] = va_arg(ap, int);
	}

	va_end(ap);

	ck_assert(!schema_record(&schema, type_ids, name_ids, nfield, &id));

	return id;
}


int Union (int id1, int id2)
{
	int id;
	ck_assert(!schema_union(&schema, id1, id2, &id));
	return id;
}


START_TEST(test_valid_null)
{
	ck_assert(is_null("null"));
	ck_assert(is_null("  null"));
	ck_assert(is_null("    null "));
	ck_assert(is_null("null   "));
	ck_assert(is_null(""));
	ck_assert(is_null("   "));
}
END_TEST


START_TEST(test_invalid_null)
{
	ck_assert(is_error("n"));
	ck_assert(is_error("nulll"));
	ck_assert(is_error("null null"));
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
	ck_assert(is_error("tru"));
	ck_assert(is_error("true1"));
	ck_assert(is_error("false (really)"));
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
	ck_assert(is_error("1a"));
	ck_assert(is_error("+1"));
	ck_assert(is_error("-"));
	ck_assert(is_error("01"));
	ck_assert(is_error("1."));
	ck_assert(is_error(".1"));
	ck_assert(is_error("-.1"));
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
	ck_assert(is_error("hello"));
	ck_assert(is_error("\"hello\" world"));
	ck_assert(is_error("\"invalid utf-8 \xBF\""));
	ck_assert(is_error("\"invalid utf-8 \xC2\x7F\""));
	ck_assert(is_error("\"invalid escape \\a\""));
	ck_assert(is_error("\"missing escape \\\""));
	ck_assert(is_error("\"ends early \\u007\""));
	ck_assert(is_error("\"non-hex value \\u0G7F\""));
	ck_assert(is_error("\"\\uD800 high surrogate\""));
	ck_assert(is_error("\"\\uDBFF high surrogate\""));
	ck_assert(is_error("\"\\uD800\\uDC0G invalid hex\""));
	ck_assert(is_error("\"\\uDC00 low surrogate\""));
	ck_assert(is_error("\"\\uDFFF low surrogate\""));
	ck_assert(is_error("\"\\uD84 incomplete\""));
	ck_assert(is_error("\"\\uD804\\u2603 invalid low\""));
}
END_TEST


START_TEST(test_valid_array)
{
	ck_assert(get_type("[]") == Array(Null, 0));
	ck_assert(get_type("[\"hello\"]") == Array(Text, 1));
	ck_assert(get_type("[1, 2, 3]") == Array(Number, 3));
	ck_assert(get_type("[[1], [2], [3]]") == Array(Array(Number, 1), 3));
}
END_TEST


START_TEST(test_invalid_array)
{
	ck_assert(is_error("[null, ]"));
}
END_TEST


START_TEST(test_union_null)
{
	ck_assert(Union(Null, Null) == Null);
	ck_assert(Union(Null, Number) == Number);
	ck_assert(Union(Number, Null) == Number);
	ck_assert(Union(Array(Number, 5), Null) == Array(Number, 5));
}
END_TEST


START_TEST(test_union_any)
{
	ck_assert(Union(Null, Any) == Any);
	ck_assert(Union(Text, Number) == Any);
	ck_assert(Union(Any, Number) == Any);
	ck_assert(Union(Array(Number, 5), Text) == Any);
}
END_TEST


START_TEST(test_union_array)
{
	ck_assert(Union(Array(Null, 3), Array(Text, 3)) == Array(Text, 3));
	ck_assert(Union(Array(Text, 0), Array(Text, 3)) == Array(Text, -1));
	ck_assert(Union(Array(Array(Bool, 2), 5), Array(Array(Null, 2), 5))
		  == Array(Array(Bool, 2), 5));
	ck_assert(Union(Array(Array(Bool, 2), 5), Array(Array(Null, 0), 5))
		  == Array(Array(Bool, -1), 5));
	ck_assert(Union(Array(Array(Bool, 2), 5), Array(Array(Null, 0), 4))
		  == Array(Array(Bool, -1), -1));
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

	tc = tcase_create("array");
        tcase_add_checked_fixture(tc, setup_datatype, teardown_datatype);
	tcase_add_test(tc, test_valid_array);
	tcase_add_test(tc, test_invalid_array);
	suite_add_tcase(s, tc);

	tc = tcase_create("union");
        tcase_add_checked_fixture(tc, setup_datatype, teardown_datatype);
	tcase_add_test(tc, test_union_null);
	tcase_add_test(tc, test_union_any);
	tcase_add_test(tc, test_union_array);
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
