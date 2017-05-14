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

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "memory.h"


void (*corpus_alloc_fail_func) (void) = NULL;


void *corpus_calloc(size_t count, size_t size)
{
	void *mem = calloc(count, size);

	if (count && size && !mem) {
		corpus_log(CORPUS_ERROR_NOMEM,
			   "failed to allocate %"PRIu64" objects"
			   " of size %"PRIu64,
			   (uint64_t)count, (uint64_t)size);

		if (corpus_alloc_fail_func) {
			corpus_alloc_fail_func();
		}
	}

	return mem;
}


void corpus_free(void *ptr)
{
	free(ptr);
}


void *corpus_malloc(size_t size)
{
	void *mem = malloc(size);

	if (size && !mem) {
		corpus_log(CORPUS_ERROR_NOMEM,
			   "failed to allocate %"PRIu64" bytes",
			   (uint64_t)size);

		if (corpus_alloc_fail_func) {
			corpus_alloc_fail_func();
		}
	}

	return mem;
}


void *corpus_realloc(void *ptr, size_t size)
{
	void *mem = realloc(ptr, size);

	if (size && !mem) {
		corpus_log(CORPUS_ERROR_NOMEM,
			   "failed to allocate %"PRIu64" bytes",
			   (uint64_t)size);

		if (corpus_alloc_fail_func) {
			corpus_alloc_fail_func();
		}
	}

	return mem;
}


char *corpus_strdup(const char *s1)
{
	size_t n = strlen(s1);
	char *s;

	if (!(s = corpus_malloc(n + 1))) {
		return NULL;
	}

	return strcpy(s, s1);
}
