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

#ifndef TESTUTIL_H
#define TESTUTIL_H

#include <stddef.h>

struct corpus_text;

#define assert_text_eq(X, Y) do { \
	const struct corpus_text * _ck_x = (X); \
	const struct corpus_text * _ck_y = (Y); \
	ck_assert_msg(corpus_text_equals(_ck_y, _ck_x), \
		"Assertion '%s == %s' failed: %s==\"%s\" (0x%zx)," \
		" %s==\"%s\" (0x%zx)", \
		#X, #Y, #X, _ck_x->ptr, _ck_x->attr, \
		#Y, _ck_y->ptr, _ck_y->attr); \
} while (0)

#define assert_text_ne(X, Y) do { \
	const struct corpus_text * _ck_x = (X); \
	const struct corpus_text * _ck_y = (Y); \
	ck_assert_msg(!corpus_text_equals(_ck_y, _ck_x), \
		"Assertion '%s != %s' failed: %s==\"%s\" (0x%zx)," \
		" %s==\"%s\" (0x%zx)", \
		#X, #Y, #X, _ck_x->ptr, _ck_x->attr, \
		#Y, _ck_y->ptr, _ck_y->attr); \
} while (0)


/**
 * Common test framework set up.
 */
void setup(void);

/**
 * Common test framework tear down.
 */ 
void teardown(void);

/**
 * Allocate memory.
 */
void *alloc(size_t size);

/**
 * Allocate a text object, interpreting escape codes.
 */
struct corpus_text *T(const char *str);

/**
 * Cast a raw string as a text object, ignoring escape codes.
 */
struct corpus_text *S(const char *str);

#endif /* TESTUTIL_H */
