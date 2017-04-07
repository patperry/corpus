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
#include <limits.h>
#include <stdint.h>
#include <syslog.h>
#include "errcode.h"
#include "xalloc.h"
#include "array.h"


/* Default initial size for nonempty dynamic arrays. Must be positive. */
#define ARRAY_SIZE_INIT	32

/* Growth factor for dynamic arrays. Must be greater than 1.
 *
 *     https://en.wikipedia.org/wiki/Dynamic_array#Growth_factor
 */
#define ARRAY_GROW 1.618 /* Golden Ratio, (1 + sqrt(5)) / 2 */


/**
 * Determine the capacity for an array that needs to grow.
 *
 * \param count the minimum capacity
 * \param size the current capacity
 *
 * \returns the new capacity
 */
static int array_grow_size(int count, int size)
{
	double n1;

	assert(ARRAY_SIZE_INIT > 0);
	assert(ARRAY_GROW > 1);

	if (size < ARRAY_SIZE_INIT && count > 0) {
		size = ARRAY_SIZE_INIT;
	}

	while (size < count) {
		n1 = ARRAY_GROW * size;
		if (n1 > INT_MAX) {
			size = INT_MAX;
		} else {
			size = (int)n1;
		}
	}

	return size;
}


int array_grow(void **baseptr, int *sizeptr, size_t width, int count, int nadd)
{
	void *base = *baseptr;
	int size = *sizeptr;
	int max = size;

	assert(count >= 0);
	assert(size >= 0);

	if (nadd <= 0) {
		return 0;
	}

	if (count > INT_MAX - nadd) {
		syslog(LOG_ERR, "array count exceeds maximum (%d)", INT_MAX);
		return ERROR_OVERFLOW;
	}
	count = count + nadd;

	if (count <= max) {
		return 0;
	}

	if ((size_t)count > SIZE_MAX / width) {
		syslog(LOG_ERR, "array size (%zu) exceeds maximum (%zu)",
		       (size_t)count, (size_t)SIZE_MAX / width);
		return ERROR_OVERFLOW;
	}

	size = array_grow_size(count, size);
	if ((size_t)size > SIZE_MAX / width) {
		size = count;
		assert((size_t)size <= SIZE_MAX / width);
	}

	if (!(base = xrealloc(base, ((size_t)size) * width))) {
		syslog(LOG_ERR, "failed allocating array");
		return ERROR_NOMEM;
	}

	*baseptr = base;
	*sizeptr = size;
	return 0;
}
