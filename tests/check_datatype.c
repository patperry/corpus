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
#include <stdlib.h>
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


START_TEST(test_simple)
{
}
END_TEST


Suite *datatype_suite(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("datatype");

	tc = tcase_create("checking");
        tcase_add_checked_fixture(tc, setup_datatype, teardown_datatype);
	tcase_add_test(tc, test_simple);
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
