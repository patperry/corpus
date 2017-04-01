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
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <syslog.h>
#include "errcode.h"
#include "table.h"
#include "text.h"
#include "token.h"
#include "symtab.h"
#include "datatype.h"
#include "data.h"

double strntod_c(const char *string, size_t maxlen, char **endPtr);
intmax_t strntoimax(const char *string, size_t maxlen, char **endptr);

static void scan_spaces(const uint8_t **bufptr, const uint8_t *end);


int bool_assign(int *valptr, const uint8_t *ptr, size_t size)
{
	const uint8_t *end = ptr + size;
	int val;
	int err;

	scan_spaces(&ptr, end);
	if (ptr == end) {
		goto nullval;
	}

	switch (*ptr) {
	case 'f':
		val = 0;
		break;
	case 't':
		val = 1;
		break;
	default:
		goto nullval;
	}

	err = 0;
	goto out;

nullval:
	val = INT_MIN;
	err = ERROR_INVAL;

out:
	if (valptr) {
		*valptr = val;
	}
	return err;
}


int int_assign(int *valptr, const uint8_t *ptr, size_t size)
{
	const uint8_t *end = ptr + size;
	const uint8_t *begin;
	intmax_t lval;
	int val;
	int err;

	scan_spaces(&ptr, end);
	if (ptr == end) {
		goto nullval;
	}

	begin = ptr;
	errno = 0;
	lval = strntoimax((char *)ptr, end - ptr, (char **)&ptr);
	if (ptr != begin) {
		if (errno == ERANGE) {
			val = lval > 0 ? INT_MAX : INT_MIN;
			err = ERROR_OVERFLOW;
		} else if (lval > INT_MAX) {
			val = INT_MAX;
			err = ERROR_OVERFLOW;
		} else if (lval < INT_MIN) {
			val = INT_MIN;
			err = ERROR_OVERFLOW;
		} else {
			val = (int)lval;
			err = 0;
		}
		goto out;
	}

nullval:
	val = INT_MIN;
	err = ERROR_INVAL;

out:
	if (valptr) {
		*valptr = val;
	}
	return err;
}


int double_assign(double *valptr, const uint8_t *ptr, size_t size)
{
	const uint8_t *end = ptr + size;
	const uint8_t *begin;
	uint_fast8_t ch;
	double val;
	int err;
	int neg = 0;

	scan_spaces(&ptr, end);
	if (ptr == end) {
		goto nullval;
	}

	begin = ptr;
	val = strntod_c((char *)ptr, end - ptr, (char **)&ptr);
	if (ptr != begin) {
		if (!isfinite(val)) {
			err = ERROR_OVERFLOW;
		} else {
			err = 0;
		}
		goto out;
	}

	ptr = begin;
	ch = *ptr++;

	// skip over optional sign
	if (ch == '-') {
		assert(ptr != end); // missing number after negative sign
		neg = 1;
		ch = *ptr++;
	} else if (ch == '+') {
		assert(ptr != end); // missing number after positive sign
		ch = *ptr++;
	}

	switch (ch) {
	case 'I':
		val = neg ? -INFINITY : INFINITY;
		break;

	case 'N':
		val = neg ? copysign(NAN, -1) : NAN;
		break;

	default:
		goto nullval;
	}

	err = 0;
	goto out;

nullval:
	err = ERROR_INVAL;
	val = NAN;
	goto out;

out:
	if (valptr) {
		*valptr = val;
	}
	return err;
}


void scan_spaces(const uint8_t **bufptr, const uint8_t *end)
{
	const uint8_t *ptr = *bufptr;
	uint_fast8_t ch;

	while (ptr != end) {
		ch = *ptr;
		if (!isspace(ch)) {
			break;
		}
		ptr++;
	}

	*bufptr = ptr;
}
