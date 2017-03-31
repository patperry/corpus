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
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "../src/errcode.h"
#include "../src/table.h"
#include "../src/text.h"
#include "../src/token.h"
#include "../src/symtab.h"
#include "../src/data.h"
#include "../src/schema.h"
#include "testutil.h"

#define STR_VALUE(x) #x
#define STRING(x) STR_VALUE(x)

struct schema schema;

const int Null = DATATYPE_NULL;
const int Bool = DATATYPE_BOOL;
const int Number = DATATYPE_NUMBER;
const int Text = DATATYPE_TEXT;
const int Any = DATATYPE_ANY;

void setup_data(void)
{
	setup();
	schema_init(&schema);
}


void teardown_data(void)
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
	struct data d;
	size_t n = strlen(str);
	ck_assert(!data_assign(&d, &schema, (const uint8_t *)str, n));
	return d.type_id;
}


int is_error(const char *str)
{
	struct data d;
	size_t n = strlen(str);
	int err = data_assign(&d, &schema, (const uint8_t *)str, n);
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


int decode_bool(const char *str)
{
	struct data d;
	size_t n = strlen(str);

	ck_assert(!data_assign(&d, &schema, (const uint8_t *)str, n));
	ck_assert(d.type_id == DATATYPE_BOOL);

	return d.value.bool_;
}


int is_number(const char *str)
{
	return (get_type(str) == DATATYPE_NUMBER);
}


double decode_number(const char *str)
{
	struct data d;
	size_t n = strlen(str);

	ck_assert(!data_assign(&d, &schema, (const uint8_t *)str, n));
	ck_assert(d.type_id == DATATYPE_NUMBER);

	return d.value.number;
}


int is_text(const char *str)
{
	return (get_type(str) == DATATYPE_TEXT);
}


int assert_types_equal(int id1, int id2)
{
	if (id1 != id2) {
		printf("unequal types: ");
		write_datatype(stdout, &schema, id1);
		printf(" != ");
		write_datatype(stdout, &schema, id2);
		printf("\n");
		return 0;
	}
	return 1;
}

#define ck_assert_typ_eq(X, Y) do { \
	ck_assert(assert_types_equal(X, Y)); \
} while (0)


int Array(int length, int element_id)
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


START_TEST(test_decode_bool)
{
	ck_assert_int_eq(decode_bool("true"), 1);
	ck_assert_int_eq(decode_bool("  true"), 1);
	ck_assert_int_eq(decode_bool(" true "), 1);
	ck_assert_int_eq(decode_bool("true  "), 1);

	ck_assert_int_eq(decode_bool("false"), 0);
	ck_assert_int_eq(decode_bool("  false"), 0);
	ck_assert_int_eq(decode_bool(" false "), 0);
	ck_assert_int_eq(decode_bool("false  "), 0);
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

	// not valid JSON, but accept anyway
	ck_assert(is_number("+1"));
	ck_assert(is_number("01"));
	ck_assert(is_number("1."));
	ck_assert(is_number(".1"));
	ck_assert(is_number("-.1"));
}
END_TEST


START_TEST(test_valid_nonfinite)
{
	ck_assert(is_number("Infinity"));
	ck_assert(is_number("-Infinity"));
	ck_assert(is_number("+Infinity"));
	ck_assert(is_number("NaN"));
	ck_assert(is_number("-NaN"));
	ck_assert(is_number("+NaN"));
}
END_TEST


START_TEST(test_invalid_number)
{
	ck_assert(is_error("1a"));
	ck_assert(is_error("-"));
	ck_assert(is_error("1e"));
	ck_assert(is_error("1E+"));

	ck_assert(is_error("[1a]"));
	ck_assert(is_error("[-]"));
	ck_assert(is_error("[1e]"));
	ck_assert(is_error("[1E+]"));
}
END_TEST


START_TEST(test_valid_nested_number)
{
	ck_assert(get_type("[31337]") == Array(1, Number));
	ck_assert(get_type("[123]") == Array(1, Number));
	ck_assert(get_type("[-1]") == Array(1, Number));
	ck_assert(get_type("[0]") == Array(1, Number));
	ck_assert(get_type("[99]") == Array(1, Number));
	ck_assert(get_type("[0.1]") == Array(1, Number));
	ck_assert(get_type("[1e17]") == Array(1, Number));
	ck_assert(get_type("[2.02e-07]") == Array(1, Number));
	ck_assert(get_type("[5.0e+006]") == Array(1, Number));

	// not valid JSON, but accept anyway
	ck_assert(get_type("[+1]") == Array(1, Number));
	ck_assert(get_type("[01]") == Array(1, Number));
	ck_assert(get_type("[1.]") == Array(1, Number));
	ck_assert(get_type("[.1]") == Array(1, Number));
	ck_assert(get_type("[-.1]") == Array(1, Number));
}
END_TEST


START_TEST(test_decode_number)
{
	ck_assert(decode_number("1") == 1);
	ck_assert(decode_number("-1.0") == -1.0);
	ck_assert(decode_number("314E-2") == 314E-2);

	ck_assert(decode_number(STRING(DBL_MAX)) == DBL_MAX);
	ck_assert(decode_number(STRING(DBL_MIN)) == DBL_MIN);
	ck_assert(decode_number(STRING(DBL_EPSILON)) == DBL_EPSILON);
}
END_TEST


START_TEST(test_decode_zero)
{
	ck_assert(decode_number("0") == 0);
	ck_assert(decode_number("-0") == 0);
	ck_assert(copysign(1, decode_number("-0") == -1));

	// https://bugs.r-project.org/bugzilla/show_bug.cgi?id=15976
	ck_assert(decode_number("0E4932") == 0);
	ck_assert(decode_number("0E4933") == 0);
	ck_assert(decode_number("0E-4932") == 0);
	ck_assert(decode_number("0E-4933") == 0);

	ck_assert(decode_number("-0e9999") == 0);
	ck_assert(copysign(1, decode_number("-0e9999")) == -1);
}
END_TEST


START_TEST(test_decode_huge_exponent)
{
	// https://bugs.r-project.org/bugzilla/show_bug.cgi?id=16358
	ck_assert(decode_number("1e99") == 1e99);
	ck_assert(decode_number("1e99999999999") == INFINITY);
	ck_assert(decode_number("1e999999999999") == INFINITY);
	ck_assert(decode_number("1e9999999999999") == INFINITY);
	ck_assert(decode_number("1e99999999999999") == INFINITY);
	ck_assert(decode_number("-1e99") == -1e99);
	ck_assert(decode_number("-1e99999999999") == -INFINITY);
	ck_assert(decode_number("-1e999999999999") == -INFINITY);
	ck_assert(decode_number("-1e9999999999999") == -INFINITY);
	ck_assert(decode_number("-1e99999999999999") == -INFINITY);
}
END_TEST


START_TEST(test_decode_huge_mantissa)
{
	char buf[10240];
	int i, ndig = 9999;

	ck_assert(decode_number("144115188075855877")
		  == 144115188075855877);
	ck_assert(decode_number("2.328306436538696289075e+22")
		  == 2.328306436538696289075e+22);

	buf[0] = '1';
	for (i = 0; i < ndig; i++) {
		buf[i + 1] = '0';
	}
	buf[ndig + 1] = 'e';
	buf[ndig + 2] = '-';
	buf[ndig + 3] = '9';
	buf[ndig + 4] = '9';
	buf[ndig + 5] = '9';
	buf[ndig + 6] = '9';
	buf[ndig + 7] = '\0';
	ck_assert(decode_number(buf) == 1);

	buf[0] = '.';
	buf[ndig] = '1';
	buf[ndig + 2] = '+';
	ck_assert(decode_number(buf) == 1);
}
END_TEST


START_TEST(test_decode_leading_zeroes)
{
	ck_assert(decode_number("0000000000000000000001") == 1);
	ck_assert(decode_number("0000000000000000000001.0") == 1);
	ck_assert(decode_number(".0000000000000000000001") == 1e-22);
	ck_assert(decode_number("00000000.000000000000001") == 1e-15);
}
END_TEST


START_TEST(test_decode_subnormal)
{
	ck_assert(decode_number("123456789012345678e-300")
		  == 123456789012345678e-300);

	ck_assert(decode_number("123456789012345679e-300")
		  == 123456789012345679e-300);

	ck_assert(decode_number("4.940656458412465441766e-324")
		  == DBL_EPSILON * DBL_MIN);

	ck_assert(decode_number("2.2250738585072012e-308")
		  == 2.2250738585072012e-308);
}
END_TEST


START_TEST(test_decode_nonfinite)
{
	ck_assert(decode_number("Infinity") == INFINITY);
	ck_assert(decode_number("-Infinity") == -INFINITY);
	ck_assert(isnan(decode_number("NaN")));
	ck_assert(isnan(decode_number("-NaN")));
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
	ck_assert(get_type("[]") == Array(0, Null));
	ck_assert(get_type("[\"hello\"]") == Array(1, Text));
	ck_assert(get_type("[1, 2, 3]") == Array(3, Number));
	ck_assert(get_type("[[1], [2], [3]]") == Array(3, Array(1, Number)));
}
END_TEST


START_TEST(test_invalid_array)
{
	ck_assert(is_error("[null, ]"));
}
END_TEST


START_TEST(test_valid_record)
{
	ck_assert(get_type("{}") == Record(0));
	ck_assert_typ_eq(get_type("{\"a\":\"b\"}"), Record(1, "a", Text));
	ck_assert(get_type("{\"x\":1, \"y\":true}")
		  == Record(2, "x", Number, "y", Bool));

	// field order doesn't matter
	ck_assert(get_type("{\"y\":false, \"x\":-1e14}")
		  == Record(2, "x", Number, "y", Bool));
}
END_TEST


START_TEST(test_equal_record)
{
	ck_assert(Record(1, "a", Text) == Record(1, "a", Text));
	ck_assert(Record(2, "x", Number, "y", Bool)
		  == Record(2, "x", Number, "y", Bool));
	ck_assert(Record(2, "x", Number, "y", Bool)
		  == Record(2, "y", Bool, "x", Number));
}
END_TEST


START_TEST(test_nested_record)
{
	assert_types_equal(get_type("{ \"outer_a\": true, \
			      \"outer_b\": { \"re\": 0, \"im\": null } }"),
			   Record(2, "outer_a", Bool, "outer_b",
				     Record(2, "re", Number, "im", Null)));

}
END_TEST


START_TEST(test_invalid_record)
{
	ck_assert(is_error("{ \"hello\": }"));
	ck_assert(is_error("{ \"a\":1, }"));
	ck_assert(is_error("{ \"x\":,\"y\":null }"));
	ck_assert(is_error("{ \"1\":\"duplicate\", \"1\":\"duplicate\" }"));
}
END_TEST


START_TEST(test_union_null)
{
	ck_assert(Union(Null, Null) == Null);
	ck_assert(Union(Null, Number) == Number);
	ck_assert(Union(Number, Null) == Number);
	ck_assert(Union(Array(5, Number), Null) == Array(5, Number));
}
END_TEST


START_TEST(test_union_any)
{
	ck_assert(Union(Null, Any) == Any);
	ck_assert(Union(Text, Number) == Any);
	ck_assert(Union(Any, Number) == Any);
	ck_assert(Union(Array(5, Number), Text) == Any);
}
END_TEST


START_TEST(test_union_array)
{
	ck_assert(Union(Array(3, Null), Array(3, Text)) == Array(3, Text));
	ck_assert(Union(Array(0, Text), Array(3, Text)) == Array(-1, Text));
	ck_assert(Union(Array(5, Array(2, Bool)), Array(5, Array(2, Null)))
		  == Array(5, Array(2, Bool)));
	ck_assert(Union(Array(5, Array(2, Bool)), Array(5, Array(0, Null)))
		  == Array(5, Array(-1, Bool)));
	ck_assert(Union(Array(5, Array(2, Bool)), Array(4, Array(0, Null)))
		  == Array(-1, Array(-1, Bool)));
}
END_TEST


START_TEST(test_union_record)
{
	ck_assert(Union(Record(0), Record(1, "a", Number))
		  == Record(1, "a", Number));
	ck_assert(Union(Record(1, "a", Text), Record(1, "a", Number))
		  == Record(1, "a", Any));
	ck_assert(Union(Record(2, "a", Text, "b", Null),
			Record(2, "b", Number, "c", Bool))
		  == Record(3, "a", Text, "b", Number, "c", Bool));
}
END_TEST 


Suite *data_suite(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("data");

	tc = tcase_create("null");
        tcase_add_checked_fixture(tc, setup_data, teardown_data);
	tcase_add_test(tc, test_valid_null);
	tcase_add_test(tc, test_invalid_null);
	suite_add_tcase(s, tc);

	tc = tcase_create("bool");
        tcase_add_checked_fixture(tc, setup_data, teardown_data);
	tcase_add_test(tc, test_valid_bool);
	tcase_add_test(tc, test_invalid_bool);
	tcase_add_test(tc, test_decode_bool);
	suite_add_tcase(s, tc);

	tc = tcase_create("number");
        tcase_add_checked_fixture(tc, setup_data, teardown_data);
	tcase_add_test(tc, test_valid_number);
	tcase_add_test(tc, test_valid_nonfinite);
	tcase_add_test(tc, test_invalid_number);
	tcase_add_test(tc, test_valid_nested_number);
	tcase_add_test(tc, test_decode_number);
	tcase_add_test(tc, test_decode_zero);
	tcase_add_test(tc, test_decode_huge_exponent);
	tcase_add_test(tc, test_decode_huge_mantissa);
	tcase_add_test(tc, test_decode_leading_zeroes);
	tcase_add_test(tc, test_decode_subnormal);
	tcase_add_test(tc, test_decode_nonfinite);
	suite_add_tcase(s, tc);

	tc = tcase_create("text");
        tcase_add_checked_fixture(tc, setup_data, teardown_data);
	tcase_add_test(tc, test_valid_text);
	tcase_add_test(tc, test_invalid_text);
	suite_add_tcase(s, tc);

	tc = tcase_create("array");
        tcase_add_checked_fixture(tc, setup_data, teardown_data);
	tcase_add_test(tc, test_valid_array);
	tcase_add_test(tc, test_invalid_array);
	suite_add_tcase(s, tc);

	tc = tcase_create("record");
        tcase_add_checked_fixture(tc, setup_data, teardown_data);
	tcase_add_test(tc, test_valid_record);
	tcase_add_test(tc, test_equal_record);
	tcase_add_test(tc, test_nested_record);
	tcase_add_test(tc, test_invalid_record);
	suite_add_tcase(s, tc);

	tc = tcase_create("union");
        tcase_add_checked_fixture(tc, setup_data, teardown_data);
	tcase_add_test(tc, test_union_null);
	tcase_add_test(tc, test_union_any);
	tcase_add_test(tc, test_union_array);
	tcase_add_test(tc, test_union_record);
	suite_add_tcase(s, tc);

	return s;
}


int main(void)
{
	int nfail;
	Suite *s;
	SRunner *sr;

	openlog("check_data", LOG_CONS | LOG_PERROR | LOG_PID, LOG_USER);
        setlogmask(LOG_UPTO(LOG_INFO));
        setlogmask(LOG_UPTO(LOG_DEBUG));

	s = data_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	nfail = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (nfail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
