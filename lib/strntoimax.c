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

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stddef.h>


intmax_t corpus_strntoimax(const char *string, size_t maxlen, char **endptr)
{
	const char *ptr = string;
	const char *end = ptr + maxlen;
	uintmax_t uval, uval1;
	intmax_t val;
	unsigned dig;
	int overflow, neg;

	// strip off leading whitespace
	while (ptr < end && isspace(*ptr)) {
		ptr++;
	}

	// check for leading sign
	if (ptr < end && *ptr == '-') {
		neg = 1;
		ptr++;
	} else {
		if (ptr < end && *ptr == '+') {
			ptr++;
		}
		neg = 0;
	}

	// read the absolute value
	uval = 0;
	overflow = 0;

	while (ptr < end && isdigit(*ptr)) {
		dig = (unsigned)((*ptr) - '0');
		uval1 = 10 * uval + dig;
		if (uval1 < uval) {
			overflow = 1;
		}
		uval = uval1;
		ptr++;
	}

	// cast to signed
	if (overflow) {
		val = neg ? INTMAX_MIN : INTMAX_MAX;
	} else if (uval > INTMAX_MAX) {
		if (neg) {
			if (uval - INTMAX_MAX != -(INTMAX_MIN + INTMAX_MAX)) {
				overflow = 1;
			}
			val = INTMAX_MIN;
		} else {
			overflow = 1;
			val = INTMAX_MAX;
		}
	} else {
		val = (intmax_t)uval;
		if (neg) {
			val = -val;
		}
	}

	if (endptr) {
		*endptr = (char *)ptr;
	}

	if (overflow) {
		errno = ERANGE;
	}

	return val;
}
