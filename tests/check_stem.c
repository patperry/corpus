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
#include "../lib/utf8lite/src/utf8lite.h"
#include "../src/table.h"
#include "../src/textset.h"
#include "../src/stem.h"
#include "testutil.h"


struct utf8lite_text *stem(const struct utf8lite_text *tok, const char *alg)
{
	struct utf8lite_text *typ;
	struct corpus_stem stem;
	struct corpus_stem_snowball sb;
	size_t size;

	ck_assert(!corpus_stem_snowball_init(&sb, alg));
	ck_assert(!corpus_stem_init(&stem, corpus_stem_snowball, &sb));
	ck_assert(!corpus_stem_set(&stem, tok));

	if (!stem.has_type) {
		typ = NULL;
	} else {
		size = UTF8LITE_TEXT_SIZE(&stem.type);
		typ = alloc(sizeof(*typ));
		typ->ptr = alloc(size + 1);
		memcpy(typ->ptr, stem.type.ptr, size);
		typ->ptr[size] = '\0';
		typ->attr = stem.type.attr;
	}

	corpus_stem_destroy(&stem);
	corpus_stem_snowball_destroy(&sb);

	return typ;
}


struct utf8lite_text *stem_en(const struct utf8lite_text *tok)
{
	return stem(tok, "english");
}


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


Suite *stem_suite(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("stem");
	tc = tcase_create("english");
	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_test(tc, test_stem_en);
	suite_add_tcase(s, tc);
	return s;
}


int main(void)
{
        int number_failed;
        Suite *s;
        SRunner *sr;

        s = stem_suite();
        sr = srunner_create(s);

        srunner_run_all(sr, CK_NORMAL);
        number_failed = srunner_ntests_failed(sr);
        srunner_free(sr);

        return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
