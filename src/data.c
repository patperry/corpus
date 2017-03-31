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
#include <syslog.h>
#include "errcode.h"
#include "table.h"
#include "text.h"
#include "token.h"
#include "symtab.h"
#include "schema.h"
#include "data.h"

// compound types
static int scan_field(struct schema *s, const uint8_t **bufptr,
		      const uint8_t *end, int *name_idptr, int *type_idptr);
static int scan_value(struct schema *s, const uint8_t **bufptr,
		      const uint8_t *end, int *idptr);
static int scan_array(struct schema *s, const uint8_t **bufptr,
		      const uint8_t *end, int *idptr);
static int scan_record(struct schema *s, const uint8_t **bufptr,
		       const uint8_t *end, int *idptr);

// literals
static int scan_null(const uint8_t **bufptr, const uint8_t *end);
static int scan_false(const uint8_t **bufptr, const uint8_t *end);
static int scan_true(const uint8_t **bufptr, const uint8_t *end);
static int scan_text(const uint8_t **bufptr, const uint8_t *end,
		     struct text *text);
static int scan_number(uint_fast8_t c, const uint8_t **bufptr,
		       const uint8_t *end);
static int scan_infinity(const uint8_t **bufptr, const uint8_t *end);
static int scan_nan(const uint8_t **bufptr, const uint8_t *end);

// helpers
static void scan_digits(const uint8_t **bufptr, const uint8_t *end);
static void scan_spaces(const uint8_t **bufptr, const uint8_t *end);
static int scan_chars(const char *str, unsigned len, const uint8_t **bufptr,
		      const uint8_t *end);
static int scan_char(char c, const uint8_t **bufptr, const uint8_t *end);



int data_assign(struct data *d, struct schema *s, const uint8_t *ptr,
		size_t size)
{
	const uint8_t *input = ptr;
	const uint8_t *end = ptr + size;
	int err, id;

	// skip over leading whitespace
	scan_spaces(&ptr, end);

	// treat the empty string as null
	if (ptr == end) {
		id = DATATYPE_NULL;
		goto success;
	}

	if ((err = scan_value(s, &ptr, end, &id))) {
		goto error;
	}

	scan_spaces(&ptr, end);
	if (ptr != end) {
		syslog(LOG_ERR, "unexpected trailing characters");
		goto error;
	}

success:
	err = 0;
	goto out;

error:
	syslog(LOG_ERR, "failed parsing value (%.*s)", (unsigned)size, input);
	err = ERROR_INVAL;
	id = -1;
	goto out;

out:
	d->type_id = id;
	return err;
}


int scan_value(struct schema *s, const uint8_t **bufptr, const uint8_t *end,
	       int *idptr)
{
	struct text text;
	const uint8_t *ptr = *bufptr;
	uint_fast8_t ch;
	int err, id;

	ch = *ptr++;
	switch (ch) {
	case 'n':
		if ((err = scan_null(&ptr, end))) {
			goto error;
		}
		id = DATATYPE_NULL;
		break;

	case 'f':
		if ((err = scan_false(&ptr, end))) {
			goto error;
		}
		id = DATATYPE_BOOL;
		break;

	case 't':
		if ((err = scan_true(&ptr, end))) {
			goto error;
		}
		id = DATATYPE_BOOL;
		break;

	case '"':
		if ((err = scan_text(&ptr, end, &text))) {
			goto error;
		}
		id = DATATYPE_TEXT;
		break;

	case '[':
		if ((err = scan_array(s, &ptr, end, &id))) {
			goto error;
		}
		break;

	case '{':
		if ((err = scan_record(s, &ptr, end, &id))) {
			goto error;
		}
		break;

	default:
		if ((err = scan_number(ch, &ptr, end))) {
			goto error;
		}
		id = DATATYPE_NUMBER;
		break;
	}

	err = 0;
	goto out;

error:
	id = -1;
	goto out;

out:
	*bufptr = ptr;
	*idptr = id;

	return err;
}


int scan_array(struct schema *s, const uint8_t **bufptr,
	       const uint8_t *end, int *idptr)
{
	const uint8_t *ptr = *bufptr;
	int id, cur_id, next_id;
	int length;
	int err;

	length = 0;
	cur_id = DATATYPE_NULL;

	// handle empty array
	scan_spaces(&ptr, end);
	if (ptr == end) {
		goto error_inval_noclose;
	} else if (*ptr == ']') {
		goto close;
	}

	// read first element, replacing cur_id
	if ((err = scan_value(s, &ptr, end, &cur_id))) {
		goto error;
	}
	length++;

	while (1) {
		scan_spaces(&ptr, end);
		if (ptr == end) {
			goto error_inval_noclose;
		}

		switch (*ptr) {
		case ']':
			goto close;
		case ',':
			ptr++;

			scan_spaces(&ptr, end);
			if (ptr == end) {
				goto error_inval_noval;
			}

			if ((err = scan_value(s, &ptr, end, &next_id))) {
				goto error_inval_val;
			}

			if ((err = schema_union(s, cur_id, next_id,
							&cur_id))) {
				goto error;
			}

			length++;
			break;
		default:
			goto error_inval_nocomma;
		}
	}
close:
	ptr++;
	err = schema_array(s, cur_id, length, &id);
	goto out;

error_inval_noclose:
	syslog(LOG_ERR, "no closing bracket (]) at end of array");
	err = ERROR_INVAL;
	goto error;

error_inval_noval:
	syslog(LOG_ERR, "missing value at index %d in array", length);
	err = ERROR_INVAL;
	goto error;

error_inval_val:
	syslog(LOG_ERR, "failed parsing value at index %d in array", length);
	err = ERROR_INVAL;
	goto error;

error_inval_nocomma:
	syslog(LOG_ERR, "missing comma (,) after index %d in array",
	       length);
	err = ERROR_INVAL;
	goto error;

error:
	id = -1;

out:
	*bufptr = ptr;
	*idptr = id;
	return err;
}


int scan_record(struct schema *s, const uint8_t **bufptr,
		const uint8_t *end, int *idptr)
{
	const uint8_t *ptr = *bufptr;
	int name_id, type_id, id;
	int fstart;
	int nfield;
	int err;

	fstart = s->buffer.nfield;
	nfield = 0;

	scan_spaces(&ptr, end);
	if (ptr == end) {
		goto error_inval_noclose;
	}

	// handle empty record
	if (*ptr == '}') {
		goto close;
	}

	if (s->buffer.nfield == s->buffer.nfield_max) {
		if ((err = schema_buffer_grow(&s->buffer, 1))) {
			goto error;
		}
	}
	s->buffer.nfield++;

	if ((err = scan_field(s, &ptr, end, &name_id, &type_id))) {
		goto error;
	}

	s->buffer.name_ids[fstart + nfield] = name_id;
	s->buffer.type_ids[fstart + nfield] = type_id;
	nfield++;

	while (1) {
		scan_spaces(&ptr, end);
		if (ptr == end) {
			goto error_inval_noclose;
		}

		switch (*ptr) {
		case '}':
			goto close;

		case ',':
			ptr++;
			scan_spaces(&ptr, end);

			if (s->buffer.nfield == s->buffer.nfield_max) {
				if ((err = schema_buffer_grow(&s->buffer, 1))) {
					goto error;
				}
			}
			s->buffer.nfield++;

			if ((err = scan_field(s, &ptr, end, &name_id,
					      &type_id))) {
				goto error;
			}

			s->buffer.name_ids[fstart + nfield] = name_id;
			s->buffer.type_ids[fstart + nfield] = type_id;
			nfield++;

			break;

		default:
			goto error_inval_nocomma;
		}
	}
close:
	ptr++; // skip over closing bracket (})

	err = schema_record(s, s->buffer.type_ids + fstart,
			    s->buffer.name_ids + fstart, nfield, &id);
	goto out;

error_inval_noclose:
	syslog(LOG_ERR, "no closing bracket (}) at end of record");
	err = ERROR_INVAL;
	goto error;

error_inval_nocomma:
	syslog(LOG_ERR, "missing comma (,) in record");
	err = ERROR_INVAL;
	goto error;

error:
	id = -1;

out:
	s->buffer.nfield = fstart;
	*bufptr = ptr;
	*idptr = id;
	return err;
}


int scan_field(struct schema *s, const uint8_t **bufptr, const uint8_t *end,
	       int *name_idptr, int *type_idptr)
{
	struct text name;
	const uint8_t *ptr = *bufptr;
	int err, name_id, type_id;

	// leading "
	if (*ptr != '"') {
		goto error_inval_noname;
	}
	ptr++;

	// field name
	if ((err = scan_text(&ptr, end, &name))) {
		goto error;
	}
	if ((err = schema_name(s, &name, &name_id))) {
		goto error;
	}

	// colon
	scan_spaces(&ptr, end);
	if (ptr == end || *ptr != ':') {
		goto error_inval_nocolon;
	}
	ptr++;

	// field value
	scan_spaces(&ptr, end);
	if (ptr == end) {
		goto error_inval_noval;
	}
	if ((err = scan_value(s, &ptr, end, &type_id))) {
		goto error_inval_val;
	}

	err = 0;
	goto out;

error_inval_noname:
	syslog(LOG_ERR, "missing field name in record");
	err = ERROR_INVAL;
	goto error;

error_inval_nocolon:
	syslog(LOG_ERR, "missing colon after field name \"%.*s\" in record",
	       (unsigned)TEXT_SIZE(&name), name.ptr);
	err = ERROR_INVAL;
	goto error;

error_inval_noval:
	syslog(LOG_ERR, "missing value for field \"%.*s\" in record",
	       (unsigned)TEXT_SIZE(&name), name.ptr);
	err = ERROR_INVAL;
	goto error;

error_inval_val:
	syslog(LOG_ERR, "failed parsing value for field \"%.*s\" in record",
	       (unsigned)TEXT_SIZE(&name), name.ptr);
	err = ERROR_INVAL;
	goto error;

error:
	name_id = -1;
	type_id = -1;

out:
	*bufptr = ptr;
	*name_idptr = name_id;
	*type_idptr = type_id;
	return err;
}


int scan_number(uint_fast8_t ch, const uint8_t **bufptr, const uint8_t *end)
{
	const uint8_t *ptr = *bufptr;
	int err;

	// skip over optional negative sign
	if (ch == '-') {
		if (ptr == end) {
			syslog(LOG_ERR, "missing number after negative sign");
			goto error_inval;
		}
		ch = *ptr++;
	}

	// parse integer part, Infinity, or NaN
	switch (ch) {
	case '0':
		break;
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		scan_digits(&ptr, end);
		break;
	case 'I':
		if ((err = scan_infinity(&ptr, end))) {
			goto error_inval;
		}
		break;
	case 'N':
		if ((err = scan_nan(&ptr, end))) {
			goto error_inval;
		}
		break;
	default:
		goto error_inval_start;
	}

	if (ptr == end) {
		err = 0;
		goto out;
	}

	// look ahead for optional fractional part
	if (*ptr == '.') {
		ptr++;
		if (ptr == end) {
			syslog(LOG_ERR, "missing number after decimal point");
			goto error_inval;
		}
		ch = *ptr++;
		if (!isdigit(ch)) {
			goto error_inval_char;
		}
		scan_digits(&ptr, end);
	}

	if (ptr == end) {
		err = 0;
		goto out;
	}

	// look ahead for optional exponent
	if (*ptr == 'e' || *ptr == 'E') {
		ptr++;
		if (ptr == end) {
			goto error_inval_exp;
		}
		ch = *ptr++;

		// optional sign
		if (ch == '-' || ch == '+') {
			ptr++;
			if (ptr == end) {
				goto error_inval_exp;
			}
			ch = *ptr++;
		}

		// mantissa
		if (!isdigit(ch)) {
			goto error_inval_char;
		}
		scan_digits(&ptr, end);
	}

	err = 0;
	goto out;

error_inval_start:
	if (isprint(ch)) {
		syslog(LOG_ERR, "invalid character (%c) at start of value",
		       (char)ch);
	} else {
		syslog(LOG_ERR, "invalid character (0x%02x) at start of value",
		       (unsigned char)ch);
	}
	goto error_inval;

error_inval_char:
	if (isprint(ch)) {
		syslog(LOG_ERR, "invalid character (%c) in number",
		       (char)ch);
	} else {
		syslog(LOG_ERR, "invalid character (0x%02x) in number",
		       (unsigned char)ch);
	}
	goto error_inval;

error_inval_exp:
	syslog(LOG_ERR, "missing exponent at end of number");
	goto error_inval;

error_inval:
	err = ERROR_INVAL;

out:
	*bufptr = ptr;
	return err;
}


int scan_nan(const uint8_t **bufptr, const uint8_t *end)
{
	return scan_chars("aN", 2, bufptr, end);
}


int scan_infinity(const uint8_t **bufptr, const uint8_t *end)
{
	return scan_chars("nfinity", 7, bufptr, end);
}


int scan_text(const uint8_t **bufptr, const uint8_t *end,
	      struct text *text)
{
	const uint8_t *input = *bufptr;
	const uint8_t *ptr = input;
	uint_fast8_t ch;
	int err;

	while (ptr != end) {
		ch = *ptr;
		if (ch == '"') {
			goto close;
		} else if (ch == '\'') {
			if (ptr == end) {
				goto error_noclose;
			}
			ptr++; // skip over next character
		}
		ptr++;
	}

error_noclose:
	syslog(LOG_ERR, "no trailing quote (\") at end of text value");
	err = ERROR_INVAL;
	goto out;

close:
	if ((err = text_assign(text, input, ptr - input, 0))) {
		err = ERROR_INVAL;
		goto out;
	}

	ptr++; // skip over close quote
	err = 0;
out:
	*bufptr = ptr;
	return err;
}


int scan_true(const uint8_t **bufptr, const uint8_t *end)
{
	return scan_chars("rue", 3, bufptr, end);
}


int scan_false(const uint8_t **bufptr, const uint8_t *end)
{
	return scan_chars("alse", 4, bufptr, end);
}


int scan_null(const uint8_t **bufptr, const uint8_t *end)
{
	return scan_chars("ull", 3, bufptr, end);
}

/**
 * Skip over ASCII digits.
 */
void scan_digits(const uint8_t **bufptr, const uint8_t *end)
{
	const uint8_t *ptr = *bufptr;
	uint_fast8_t ch;

	while (ptr != end) {
		ch = *ptr;
		if (!isdigit(ch)) {
			break;
		}
		ptr++;
	}

	*bufptr = ptr;
}

/**
 * Skip over ASCII whitespace characters. This is more permissive than
 * strict JSON, which only allows ' ', '\t', '\r', and '\n'.
 */
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


int scan_chars(const char *str, unsigned len, const uint8_t **bufptr,
	       const uint8_t *end)
{
	unsigned i;
	int err;

	for (i = 0; i < len; i++) {
		if ((err = scan_char(str[i], bufptr, end))) {
			return err;
		}
	}

	return 0;
}


int scan_char(char c, const uint8_t **bufptr, const uint8_t *end)
{
	const uint8_t *ptr = *bufptr;
	uint_fast8_t ch;

	if (ptr == end) {
		syslog(LOG_ERR, "expecting '%c' but input ended", c);
		return ERROR_INVAL;
	}

	ch = *ptr;
	if (ch != c) {
		if (isprint(ch)) {
			syslog(LOG_ERR, "expecting '%c' but got '%c'", c,
			       (char)ch);
		} else {
			syslog(LOG_ERR, "expecting '%c' but got '0x%02x'", c,
			       (unsigned char)ch);
		}
		return ERROR_INVAL;
	}

	*bufptr = ptr + 1;
	return 0;
}
