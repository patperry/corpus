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


int data_assign(struct data *d, struct schema *s, const uint8_t *ptr,
		size_t size)
{
	const uint8_t *end = ptr + size;
	int err, id;

	scan_spaces(&ptr, end);

	if ((err = schema_scan(s, ptr, end - ptr, &id))) {
		goto error;
	}
	goto out;

error:
	ptr = NULL;
	size = 0;
out:
	d->ptr = ptr;
	d->size = size;
	d->type_id = id;
	return err;
}


int data_bool(const struct data *d, int *valptr)
{
	int val;
	int err;

	if (d->type_id != DATATYPE_BOOLEAN) {
		val = INT_MIN;
		err = ERROR_INVAL;
	} else {
		val = *d->ptr == 't' ? 1 : 0;
		err = 0;
	}

	if (valptr) {
		*valptr = val;
	}

	return err;
}


int data_int(const struct data *d, int *valptr)
{
	intmax_t lval;
	int val;
	int err;

	if (d->type_id != DATATYPE_INTEGER) {
		goto nullval;
	}

	errno = 0;
	lval = strntoimax((char *)d->ptr, d->size, NULL);
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

nullval:
	val = INT_MIN;
	err = ERROR_INVAL;

out:
	if (valptr) {
		*valptr = val;
	}
	return err;
}


int data_double(const struct data *d, double *valptr)
{
	const uint8_t *ptr;
	uint_fast8_t ch;
	double val;
	int err;
	int neg = 0;

	if (!(d->type_id == DATATYPE_REAL || d->type_id == DATATYPE_INTEGER)) {
		goto nullval;
	}

	val = strntod_c((char *)d->ptr, d->size, (char **)&ptr);
	if (ptr != d->ptr) {
		if (!isfinite(val)) {
			err = ERROR_OVERFLOW;
		} else {
			err = 0;
		}
		goto out;
	}

	// strntod failed; parse Infinity/NaN
	ptr = d->ptr;
	ch = *ptr++;

	// skip over optional sign
	if (ch == '-') {
		neg = 1;
		ch = *ptr++;
	} else if (ch == '+') {
		ch = *ptr++;
	}

	switch (ch) {
	case 'I':
		val = neg ? -INFINITY : INFINITY;
		break;

	default:
		val = neg ? copysign(NAN, -1) : NAN;
		break;
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


int data_text(const struct data *d, struct text *valptr)
{
	struct text val;
	const uint8_t *ptr;
	const uint8_t *end;
	int err;

	if (d->type_id != DATATYPE_TEXT) {
		goto nullval;
	}
	err = 0;

	ptr = d->ptr;
	end = ptr + d->size;

	ptr++; // skip over leading quote (")

	// skip over trailing whitespace
	end--;
	while (*end != '"') {
		end--;
	}

	err = text_assign(&val, ptr, end - ptr, TEXT_NOVALIDATE);
	goto out;

nullval:
	val.ptr = NULL;
	val.attr = 0;
	err = ERROR_INVAL;
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
