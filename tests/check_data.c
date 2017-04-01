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
const int Boolean = DATATYPE_BOOLEAN;
const int Integer = DATATYPE_INTEGER;
const int Real = DATATYPE_REAL;
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
	size_t n = strlen(str);
	int id;
	ck_assert(!schema_scan(&schema, (const uint8_t *)str, n, &id));
	return id;
}


int is_error(const char *str)
{
	size_t n = strlen(str);
	int id;
	int err = schema_scan(&schema, (const uint8_t *)str, n, &id);
	ck_assert(err == ERROR_INVAL || err == 0);
	return (err != 0);
}


int is_null(const char *str)
{
	return (get_type(str) == DATATYPE_NULL);
}


int is_boolean(const char *str)
{
	return (get_type(str) == DATATYPE_BOOLEAN);
}


int decode_boolean(const char *str)
{
	int val;
	size_t n = strlen(str);

	ck_assert(is_boolean(str));
	ck_assert(!bool_assign(&val, (const uint8_t *)str, n, -1));

	return val;
}


int is_integer(const char *str)
{
	return (get_type(str) == DATATYPE_INTEGER);
}


double decode_int(const char *str)
{
	int val;
	size_t n = strlen(str);

	ck_assert(is_integer(str));
	ck_assert(!int_assign(&val, (const uint8_t *)str, n, INT_MIN));

	return val;
}


int is_real(const char *str)
{
	return (get_type(str) == DATATYPE_REAL);
}


int is_numeric(const char *str)
{
	int id = get_type(str);
	return (id == DATATYPE_INTEGER || id == DATATYPE_REAL);
}


double decode_double(const char *str)
{
	double val;
	size_t n = strlen(str);

	ck_assert(is_numeric(str));
	ck_assert(!double_assign(&val, (const uint8_t *)str, n, NAN));

	return val;
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


START_TEST(test_valid_boolean)
{
	ck_assert(is_boolean("false"));
	ck_assert(is_boolean("true"));

	ck_assert(is_boolean("  true"));
	ck_assert(is_boolean("    true "));
	ck_assert(is_boolean("false   "));
}
END_TEST


START_TEST(test_invalid_boolean)
{
	ck_assert(is_error("tru"));
	ck_assert(is_error("true1"));
	ck_assert(is_error("false (really)"));
}
END_TEST


START_TEST(test_decode_boolean)
{
	ck_assert_int_eq(decode_boolean("true"), 1);
	ck_assert_int_eq(decode_boolean("  true"), 1);
	ck_assert_int_eq(decode_boolean(" true "), 1);
	ck_assert_int_eq(decode_boolean("true  "), 1);

	ck_assert_int_eq(decode_boolean("false"), 0);
	ck_assert_int_eq(decode_boolean("  false"), 0);
	ck_assert_int_eq(decode_boolean(" false "), 0);
	ck_assert_int_eq(decode_boolean("false  "), 0);
}
END_TEST


START_TEST(test_valid_number)
{
	ck_assert(is_integer("31337"));
	ck_assert(is_integer("123"));
	ck_assert(is_integer("-1"));
	ck_assert(is_integer("0"));
	ck_assert(is_integer("99"));

	ck_assert(is_real("0.1"));
	ck_assert(is_real("1e17"));
	ck_assert(is_real("2.02e-07"));
	ck_assert(is_real("5.0e+006"));

	// not valid JSON, but accept anyway
	ck_assert(is_integer("+1"));
	ck_assert(is_integer("01"));
	ck_assert(is_real("1."));
	ck_assert(is_real(".1"));
	ck_assert(is_real("-.1"));
}
END_TEST


START_TEST(test_valid_nonfinite)
{
	ck_assert(is_real("Infinity"));
	ck_assert(is_real("-Infinity"));
	ck_assert(is_real("+Infinity"));
	ck_assert(is_real("NaN"));
	ck_assert(is_real("-NaN"));
	ck_assert(is_real("+NaN"));
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
	ck_assert(get_type("[31337]") == Array(1, Integer));
	ck_assert(get_type("[123]") == Array(1, Integer));
	ck_assert(get_type("[-1]") == Array(1, Integer));
	ck_assert(get_type("[0]") == Array(1, Integer));
	ck_assert(get_type("[99]") == Array(1, Integer));
	ck_assert(get_type("[0.1]") == Array(1, Real));
	ck_assert(get_type("[1e17]") == Array(1, Real));
	ck_assert(get_type("[2.02e-07]") == Array(1, Real));
	ck_assert(get_type("[5.0e+006]") == Array(1, Real));

	// not valid JSON, but accept anyway
	ck_assert(get_type("[+1]") == Array(1, Integer));
	ck_assert(get_type("[01]") == Array(1, Integer));
	ck_assert(get_type("[1.]") == Array(1, Real));
	ck_assert(get_type("[.1]") == Array(1, Real));
	ck_assert(get_type("[-.1]") == Array(1, Real));
}
END_TEST


START_TEST(test_decode_number)
{
	ck_assert(decode_double("1") == 1);
	ck_assert(decode_double("-1.0") == -1.0);
	ck_assert(decode_double("314E-2") == 314E-2);

	ck_assert(decode_double(STRING(DBL_MAX)) == DBL_MAX);
	ck_assert(decode_double(STRING(DBL_MIN)) == DBL_MIN);
	ck_assert(decode_double(STRING(DBL_EPSILON)) == DBL_EPSILON);
}
END_TEST


START_TEST(test_decode_zero)
{
	ck_assert(decode_double("0") == 0);
	ck_assert(decode_double("-0") == 0);
	ck_assert(copysign(1, decode_double("-0") == -1));

	// https://bugs.r-project.org/bugzilla/show_bug.cgi?id=15976
	ck_assert(decode_double("0E4932") == 0);
	ck_assert(decode_double("0E4933") == 0);
	ck_assert(decode_double("0E-4932") == 0);
	ck_assert(decode_double("0E-4933") == 0);

	ck_assert(decode_double("-0e9999") == 0);
	ck_assert(copysign(1, decode_double("-0e9999")) == -1);
}
END_TEST


START_TEST(test_decode_huge_exponent)
{
	// https://bugs.r-project.org/bugzilla/show_bug.cgi?id=16358
	ck_assert(decode_double("1e99") == 1e99);
	ck_assert(decode_double("1e99999999999") == INFINITY);
	ck_assert(decode_double("1e999999999999") == INFINITY);
	ck_assert(decode_double("1e9999999999999") == INFINITY);
	ck_assert(decode_double("1e99999999999999") == INFINITY);
	ck_assert(decode_double("-1e99") == -1e99);
	ck_assert(decode_double("-1e99999999999") == -INFINITY);
	ck_assert(decode_double("-1e999999999999") == -INFINITY);
	ck_assert(decode_double("-1e9999999999999") == -INFINITY);
	ck_assert(decode_double("-1e99999999999999") == -INFINITY);
}
END_TEST


START_TEST(test_decode_huge_mantissa)
{
	char buf[10240];
	int i, ndig = 9999;

	ck_assert(decode_double("144115188075855877")
		  == 144115188075855877);
	ck_assert(decode_double("2.328306436538696289075e+22")
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
	ck_assert(decode_double(buf) == 1);

	buf[0] = '.';
	buf[ndig] = '1';
	buf[ndig + 2] = '+';
	ck_assert(decode_double(buf) == 1);
}
END_TEST


START_TEST(test_decode_leading_zeroes)
{
	ck_assert(decode_double("0000000000000000000001") == 1);
	ck_assert(decode_double("0000000000000000000001.0") == 1);
	ck_assert(decode_double(".0000000000000000000001") == 1e-22);
	ck_assert(decode_double("00000000.000000000000001") == 1e-15);
}
END_TEST


START_TEST(test_decode_subnormal)
{
	ck_assert(decode_double("123456789012345678e-300")
		  == 123456789012345678e-300);

	ck_assert(decode_double("123456789012345679e-300")
		  == 123456789012345679e-300);

	ck_assert(decode_double("4.940656458412465441766e-324")
		  == DBL_EPSILON * DBL_MIN);

	ck_assert(decode_double("2.2250738585072012e-308")
		  == 2.2250738585072012e-308);
}
END_TEST


START_TEST(test_decode_nonfinite)
{
	ck_assert(decode_double("Infinity") == INFINITY);
	ck_assert(decode_double("-Infinity") == -INFINITY);
	ck_assert(isnan(decode_double("NaN")));
	ck_assert(isnan(decode_double("-NaN")));
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
	ck_assert(get_type("[1, 2, 3]") == Array(3, Integer));
	ck_assert(get_type("[[1], [2], [3]]") == Array(3, Array(1, Integer)));
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
		  == Record(2, "x", Integer, "y", Boolean));

	// field order doesn't matter
	ck_assert(get_type("{\"y\":false, \"x\":-1e14}")
		  == Record(2, "x", Real, "y", Boolean));
}
END_TEST


START_TEST(test_equal_record)
{
	ck_assert(Record(1, "a", Text) == Record(1, "a", Text));
	ck_assert(Record(2, "x", Real, "y", Boolean)
		  == Record(2, "x", Real, "y", Boolean));
	ck_assert(Record(2, "x", Real, "y", Boolean)
		  == Record(2, "y", Boolean, "x", Real));
}
END_TEST


START_TEST(test_nested_record)
{
	assert_types_equal(get_type("{ \"outer_a\": true, \
			      \"outer_b\": { \"re\": 0.0, \"im\": null } }"),
			   Record(2, "outer_a", Boolean, "outer_b",
				     Record(2, "re", Real, "im", Null)));

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
	ck_assert(Union(Null, Integer) == Integer);
	ck_assert(Union(Integer, Null) == Integer);
	ck_assert(Union(Array(5, Real), Null) == Array(5, Real));
}
END_TEST


START_TEST(test_union_any)
{
	ck_assert(Union(Null, Any) == Any);
	ck_assert(Union(Text, Real) == Any);
	ck_assert(Union(Any, Real) == Any);
	ck_assert(Union(Array(5, Integer), Text) == Any);
}
END_TEST


START_TEST(test_union_numeric)
{
	ck_assert(Union(Integer, Real) == Real);
	ck_assert(Union(Real, Integer) == Real);
	ck_assert(Union(Integer, Null) == Integer);
	ck_assert(Union(Array(5, Integer), Array(5, Real))
			== Array(5, Real));
}
END_TEST


START_TEST(test_union_array)
{
	ck_assert(Union(Array(3, Null), Array(3, Text)) == Array(3, Text));
	ck_assert(Union(Array(0, Text), Array(3, Text)) == Array(-1, Text));
	ck_assert(Union(Array(5, Array(2, Boolean)), Array(5, Array(2, Null)))
		  == Array(5, Array(2, Boolean)));
	ck_assert(Union(Array(5, Array(2, Boolean)), Array(5, Array(0, Null)))
		  == Array(5, Array(-1, Boolean)));
	ck_assert(Union(Array(5, Array(2, Boolean)), Array(4, Array(0, Null)))
		  == Array(-1, Array(-1, Boolean)));
}
END_TEST


START_TEST(test_union_record)
{
	ck_assert(Union(Record(0), Record(1, "a", Integer))
		  == Record(1, "a", Integer));
	ck_assert(Union(Record(1, "a", Text), Record(1, "a", Real))
		  == Record(1, "a", Any));
	ck_assert(Union(Record(2, "a", Text, "b", Null),
			Record(2, "b", Integer, "c", Boolean))
		  == Record(3, "a", Text, "b", Integer, "c", Boolean));
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

	tc = tcase_create("boolean");
        tcase_add_checked_fixture(tc, setup_data, teardown_data);
	tcase_add_test(tc, test_valid_boolean);
	tcase_add_test(tc, test_invalid_boolean);
	tcase_add_test(tc, test_decode_boolean);
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
	tcase_add_test(tc, test_union_numeric);
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

	//openlog("check_data", LOG_CONS | LOG_PERROR | LOG_PID, LOG_USER);
        //setlogmask(LOG_UPTO(LOG_INFO));
        //setlogmask(LOG_UPTO(LOG_DEBUG));

	s = data_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	nfail = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (nfail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
