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
#include <string.h>
#include <check.h>
#include "../src/table.h"
#include "../src/tree.h"
#include "../src/termset.h"
#include "testutil.h"



struct corpus_termset set;
int has_termset;


void setup_termset(void)
{
	setup();
	has_termset = 0;
}


void teardown_termset(void)
{
	if (has_termset) {
		corpus_termset_destroy(&set);
		has_termset = 0;
	}
	teardown();
}


void init(void)
{
	ck_assert(!has_termset);
	ck_assert(!corpus_termset_init(&set));
	has_termset = 1;
}


int add(const char *str)
{
	int buf[32];
	int i, n = (int)strlen(str);
	int id;

	for (i = 0; i < n; i++) {
		buf[i] = (int)str[i];
	}
	ck_assert(!corpus_termset_add(&set, buf, n, &id));
	ck_assert_int_eq(set.items[id].length, n);
	for (i = 0; i < n; i++) {
		ck_assert_int_eq(set.items[id].type_ids[i], buf[i]);
	}
	return id;
}


int has(const char *str)
{
	int buf[32];
	int i, n = (int)strlen(str);
	int id, has;

	for (i = 0; i < n; i++) {
		buf[i] = (int)str[i];
	}

	has = corpus_termset_has(&set, buf, n, &id);

	if (has) {
		ck_assert_int_eq(set.items[id].length, n);
		for (i = 0; i < n; i++) {
			ck_assert_int_eq(set.items[id].type_ids[i], buf[i]);
		}
	}

	return has;
}


START_TEST(test_basic)
{
	init();
	add("a");
	add("b");
	add("ba");
	ck_assert(has("a"));
	ck_assert(has("b"));
	ck_assert(has("ba"));
}
END_TEST


START_TEST(test_random_trigram)
{
	int id3[10][10][10];
	int id2[10][10];
	int id1[10];
	int id0, id;
	int buf[3];
	int a, nadd = 2000;
	int n;
	int i, j, k, len;

	srand(0);
	init();
	n = 10;

	for (i = 0; i < n; i++) {
		id1[i] = -1;
	}

	for (j = 0; j < n; j++) {
		for (i = 0; i < n; i++) {
			id2[i][j] = -1;
		}
	}

	for (k = 0; k < n; k++) {
		for (j = 0; j < n; j++) {
			for (i = 0; i < n; i++) {
				id3[i][j][k] = -1;
			}
		}
	}

	for (a = 0; a < nadd; a++) {
		len = 1 + (rand() % 3);
		for (k = 0; k < len; k++) {
			buf[k] = rand() % n;
		}

		if (len == 1) {
			i = buf[0];
			id0 = id1[i];
		} else if (len == 2) {
			i = buf[0];
			j = buf[1];
			id0 = id2[i][j];
		} else {
			i = buf[0];
			j = buf[1];
			k = buf[2];
			id0 = id3[i][j][k];
		}

		if (id0 >= 0) {
			ck_assert(corpus_termset_has(&set, buf, len, &id));
			ck_assert_int_eq(id, id0);
		} else {
			ck_assert(!corpus_termset_has(&set, buf, len, &id));
		}

		ck_assert(!corpus_termset_add(&set, buf, len, &id));
		if (id0 >= 0) {
			ck_assert_int_eq(id, id0);
		}

		if (len == 1) {
			id1[i] = id;
		} else if (len == 2) {
			id2[i][j] = id;
		} else {
			id3[i][j][k] = id;
		}
	}

	for (i = 0; i < n; i++) {
		buf[0] = i;
		len = 1;
		id0 = id1[i];

		if (id0 >= 0) {
			ck_assert(corpus_termset_has(&set, buf, len, &id));
			ck_assert_int_eq(id, id0);
		} else {
			ck_assert(!corpus_termset_has(&set, buf, len, &id));
		}
	}

	for (j = 0; j < n; j++) {
		for (i = 0; i < n; i++) {
			buf[0] = i;
			buf[1] = j;
			len = 2;
			id0 = id2[i][j];
			
			if (id0 >= 0) {
				ck_assert(corpus_termset_has(&set, buf, len,
							   &id));
				ck_assert_int_eq(id, id0);
			} else {
				ck_assert(!corpus_termset_has(&set, buf, len,
							      &id));
			}
		}
	}

	for (k = 0; k < n; k++) {
		for (j = 0; j < n; j++) {
			for (i = 0; i < n; i++) {
				buf[0] = i;
				buf[1] = j;
				buf[2] = k;
				len = 3;
				id0 = id3[i][j][k]; 

				if (id0 >= 0) {
					ck_assert(corpus_termset_has(&set,
								     buf, len,
								     &id));
					ck_assert_int_eq(id, id0);
				} else {
					ck_assert(!corpus_termset_has(&set,
								      buf, len,
								      &id));
				}
			}
		}
	}
}
END_TEST


Suite *termset_suite(void)
{
        Suite *s;
        TCase *tc;

        s = suite_create("termset");

	tc = tcase_create("basic");
	tcase_add_test(tc, test_basic);
        suite_add_tcase(s, tc);

	tc = tcase_create("random");
        tcase_add_checked_fixture(tc, setup_termset, teardown_termset);
        tcase_add_test(tc, test_random_trigram);
        suite_add_tcase(s, tc);

	return s;
}


int main(void)
{
        int number_failed;
        Suite *s;
        SRunner *sr;

        s = termset_suite();
        sr = srunner_create(s);

        srunner_run_all(sr, CK_NORMAL);
        number_failed = srunner_ntests_failed(sr);
        srunner_free(sr);

        return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
