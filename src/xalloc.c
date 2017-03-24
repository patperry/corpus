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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "xalloc.h"


void (*xalloc_fail_func) (void) = NULL;


void *xcalloc(size_t count, size_t size)
{
	void *mem = calloc(count, size);

	if (count && size && !mem) {
		syslog(LOG_CRIT, "failed to allocate %zu objects of size %zu",
		       count, size);
		if (xalloc_fail_func) {
			xalloc_fail_func();
		}
	}

	return mem;
}


void *xmalloc(size_t size)
{
	void *mem = malloc(size);

	if (size && !mem) {
		syslog(LOG_CRIT, "failed to allocate %zu bytes", size);
		if (xalloc_fail_func) {
			xalloc_fail_func();
		}
	}

	return mem;
}


void *xrealloc(void *ptr, size_t size)
{
	void *mem = realloc(ptr, size);

	if (size && !mem) {
		syslog(LOG_CRIT, "failed to allocate %zu bytes", size);
		if (xalloc_fail_func) {
			xalloc_fail_func();
		}
	}

	return mem;
}


char *xstrdup(const char *s1)
{
	size_t n = strlen(s1);
	char *s;

	if (!(s = xmalloc(n + 1))) {
		return NULL;
	}

	return strcpy(s, s1);
}