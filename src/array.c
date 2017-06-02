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

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include "error.h"
#include "memory.h"
#include "array.h"


/* Default initial size for nonempty dynamic arrays. Must be positive. */
#define CORPUS_ARRAY_SIZE_INIT	32

/* Growth factor for dynamic arrays. Must be greater than 1.
 *
 *     https://en.wikipedia.org/wiki/Dynamic_array#Growth_factor
 */
#define CORPUS_ARRAY_GROW 1.618 /* Golden Ratio, (1 + sqrt(5)) / 2 */


/**
 * Determine the capacity for an array that needs to grow.
 *
 * \param count the minimum capacity
 * \param size the current capacity
 *
 * \returns the new capacity
 */
static int corpus_array_grow_size(int count, int size)
{
	double n1;

	assert(CORPUS_ARRAY_SIZE_INIT > 0);
	assert(CORPUS_ARRAY_GROW > 1);

	if (size < CORPUS_ARRAY_SIZE_INIT && count > 0) {
		size = CORPUS_ARRAY_SIZE_INIT;
	}

	while (size < count) {
		n1 = CORPUS_ARRAY_GROW * size;
		if (n1 > INT_MAX) {
			size = INT_MAX;
		} else {
			size = (int)n1;
		}
	}

	return size;
}


static size_t corpus_bigarray_grow_size(size_t count, size_t size)
{
	double n1;

	assert(CORPUS_ARRAY_SIZE_INIT > 0);
	assert(CORPUS_ARRAY_GROW > 1);

	if (size < CORPUS_ARRAY_SIZE_INIT && count > 0) {
		size = CORPUS_ARRAY_SIZE_INIT;
	}

	while (size < count) {
		n1 = CORPUS_ARRAY_GROW * size;
		if (n1 > SIZE_MAX) {
			size = SIZE_MAX;
		} else {
			size = (size_t)n1;
		}
	}

	return size;
}


int corpus_array_grow(void **baseptr, int *sizeptr, size_t width, int count,
		      int nadd)
{
	void *base = *baseptr;
	int size = *sizeptr;
	int max = size;
	int err;

	assert(count >= 0);
	assert(size >= 0);

	if (nadd <= 0) {
		return 0;
	}

	if (count > INT_MAX - nadd) {
		err = CORPUS_ERROR_OVERFLOW;
		corpus_log(err, "array count exceeds maximum (%d)", INT_MAX);
		return err;
	}
	count = count + nadd;

	if (count <= max) {
		return 0;
	}

	if ((size_t)count > SIZE_MAX / width) {
		err = CORPUS_ERROR_OVERFLOW;
		corpus_log(err, "array size (%d) exceeds maximum (%"PRIu64")",
			   count, (uint64_t)SIZE_MAX / width);
		return err;
	}

	size = corpus_array_grow_size(count, size);
	if ((size_t)size > SIZE_MAX / width) {
		size = count;
		assert((size_t)size <= SIZE_MAX / width);
	}

	if (!(base = corpus_realloc(base, ((size_t)size) * width))) {
		err = CORPUS_ERROR_NOMEM;
		corpus_log(err, "failed allocating array");
		return err;
	}

	*baseptr = base;
	*sizeptr = size;
	return 0;
}


int corpus_bigarray_grow(void **baseptr, size_t *sizeptr, size_t width,
			 size_t count, size_t nadd)
{
	void *base = *baseptr;
	size_t size = *sizeptr;
	size_t max = size;
	int err;

	if (nadd <= 0) {
		return 0;
	}

	if (count > SIZE_MAX - nadd) {
		err = CORPUS_ERROR_OVERFLOW;
		corpus_log(err, "array count exceeds maximum (%"PRIu64")",
			   (uint64_t)SIZE_MAX);
		return err;
	}
	count = count + nadd;

	if (count <= max) {
		return 0;
	}

	if (count > SIZE_MAX / width) {
		err = CORPUS_ERROR_OVERFLOW;
		corpus_log(err, "array size (%"PRIu64")"
			   " exceeds maximum (%"PRIu64")",
			   (uint64_t)count, (uint64_t)SIZE_MAX / width);
		return err;
	}

	size = corpus_bigarray_grow_size(count, size);
	if (size > SIZE_MAX / width) {
		size = count;
		assert(size <= SIZE_MAX / width);
	}

	if (!(base = corpus_realloc(base, size * width))) {
		err = CORPUS_ERROR_NOMEM;
		corpus_log(err, "failed allocating array");
		return err;
	}

	*baseptr = base;
	*sizeptr = size;
	return 0;
}
