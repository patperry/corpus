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
#include <stdlib.h>
#include <syslog.h>
#include "array.h"
#include "errcode.h"
#include "table.h"
#include "text.h"
#include "token.h"
#include "symtab.h"
#include "xalloc.h"
#include "datatype.h"

#define NUM_ATOMIC	4

static int schema_has_name(const struct schema *s, const struct text *name,
			   int *idptr);
static int schema_has_array(const struct schema *s, int type_id, int length,
			    int *idptr);
static int schema_has_record(const struct schema *s, const int *type_ids,
			     const int *name_ids, int nfield, int *idptr);

static int schema_union_array(struct schema *s, int type_id1, int type_id2,
			      int *idptr);
static int schema_union_record(struct schema *s, int type_id1, int type_id2,
			       int *idptr);
static int schema_grow_types(struct schema *s, int nadd);

// compound types
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
static int scan_text(const uint8_t **bufptr, const uint8_t *end);
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


int schema_init(struct schema *s)
{
	int i, n;
	int err;

	if ((err = symtab_init(&s->names, 0))) {
		goto error_names;
	}

	// initialize primitive types
	n = NUM_ATOMIC;
	if (!(s->types = xmalloc(n * sizeof(*s->types)))) {
		goto error_types;
	}
	s->ntype = n;
	s->ntype_max = n;

	for (i = 0; i < n; i++) {
		s->types[i].kind = i;
	}

	return 0;

error_types:
	symtab_destroy(&s->names);
error_names:
	return err;
}


void schema_destroy(struct schema *s)
{
	schema_clear(s);
	free(s->types);
	symtab_destroy(&s->names);
}


void schema_clear(struct schema *s)
{
	const struct datatype *t;
	int i = s->ntype;

	while (i-- > 0) {
		t = &s->types[i];
		if (t->kind == DATATYPE_RECORD) {
			free(t->meta.record.name_ids);
			free(t->meta.record.type_ids);
		}
	}
	s->ntype = NUM_ATOMIC;

	symtab_clear(&s->names);
}


int schema_array(struct schema *s, int type_id, int length,
			int *idptr)
{
	struct datatype *t;
	int id;
	int err;

	if (schema_has_array(s, type_id, length, &id)) {
		err = 0;
		goto out;
	}

	// grow types array if necessary
	if (s->ntype == s->ntype_max) {
		if ((err = schema_grow_types(s, 1))) {
			goto error;
		}
	}

	// add the new type
	id = s->ntype;
	t = &s->types[id];
	t->kind = DATATYPE_ARRAY;
	t->meta.array.type_id = type_id;
	t->meta.array.length = length;
	s->ntype++;

	err = 0;
	goto out;

error:
	syslog(LOG_ERR, "failed adding array type");

out:
	if (idptr) {
		*idptr = id;
	}
	return err;
}


int schema_has_array(const struct schema *s, int type_id,
			int length, int *idptr)
{
	const struct datatype *t;
	int id = s->ntype;
	int found;

	while (id-- > 0) {
		t = &s->types[id];
		if (t->kind != DATATYPE_ARRAY) {
			continue;
		}
		if (t->meta.array.type_id == type_id
				&& t->meta.array.length == length) {
			found = 1;
			goto out;
		}
	}
	found = 0;
	id = -1;
out:
	if (idptr) {
		*idptr = id;
	}
	return found;
}


int schema_union(struct schema *s, int type_id1, int type_id2,
		    int *idptr)
{
	int kind1, kind2;
	int id;
	int err = 0;

	if (type_id1 == DATATYPE_NULL || type_id1 == type_id2) {
		id = type_id2;
		goto out;
	} else if (type_id2 == DATATYPE_NULL) {
		id = type_id1;
		goto out;
	} else if (type_id1 == DATATYPE_ANY || type_id2 == DATATYPE_ANY) {
		id = DATATYPE_ANY;
		goto out;
	}

	// if we got here, then neither kind is Null or Any
	kind1 = s->types[type_id1].kind;
	kind2 = s->types[type_id2].kind;

	if (kind1 != kind2) {
		id = DATATYPE_ANY;
	} else if (kind1 == DATATYPE_ARRAY) {
		err = schema_union_array(s, type_id1, type_id2, &id);
	} else if (kind1 == DATATYPE_RECORD) {
		err = schema_union_record(s, type_id1, type_id2, &id);
	} else {
		id = DATATYPE_ANY;
	}

out:
	if (idptr) {
		*idptr = id;
	}
	return err;
}


int schema_union_array(struct schema *s, int type_id1, int type_id2,
			  int *idptr)
{
	const struct datatype_array *t1, *t2;
	int len;
	int elt, id;
	int err;

	t1 = &s->types[type_id1].meta.array;
	t2 = &s->types[type_id2].meta.array;

	if ((err = schema_union(s, t1->type_id, t2->type_id, &elt))) {
		goto error;
	}

	if (t1->length == t2->length) {
		len = t1->length;
	} else {
		len = -1;
	}

	if ((err = schema_array(s, elt, len, &id))) {
		goto error;
	}

	goto out;

error:
	syslog(LOG_ERR, "failed computing union of array type");
	id = DATATYPE_ANY;

out:
	if (idptr) {
		*idptr = id;
	}

	return err;
}


int schema_union_record(struct schema *s, int type_id1,
			   int type_id2, int *idptr)
{
	int id = DATATYPE_ANY;
	(void)s;
	(void)type_id1;
	(void)type_id2;
	*idptr = id;
	return 0;
}


int schema_grow_types(struct schema *s, int nadd)
{
	void *base = s->types;
	int size = s->ntype_max;
	int err;

	if ((err = array_grow(&base, &size, sizeof(*s->types),
			      s->ntype, nadd))) {
		syslog(LOG_ERR, "failed allocating type array");
		return err;
	}

	if (s->ntype > INT_MAX - nadd) {
		syslog(LOG_ERR, "type count exceeds maximum (%d)", INT_MAX);
		return ERROR_OVERFLOW;
	}

	s->types = base;
	s->ntype_max = size;
	return 0;
}


int schema_scan(struct schema *s, const uint8_t *ptr, size_t len,
		   int *idptr)
{
	const uint8_t *input = ptr;
	const uint8_t *end = ptr + len;
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
	syslog(LOG_ERR, "failed parsing value (%.*s)", (unsigned)len, input);
	err = ERROR_INVAL;
	id = -1;
	goto out;

out:
	if (idptr) {
		*idptr = id;
	}
	return err;
}


int scan_value(struct schema *s, const uint8_t **bufptr,
	       const uint8_t *end, int *idptr)
{
	const uint8_t *ptr = *bufptr;
	uint_fast8_t ch;
	int err, id;

	if (ptr == end) {
		syslog(LOG_ERR, "missing value");
		err = ERROR_INVAL;
		goto error;
	}

	ch = *ptr++;
	switch (ch) {
	case 'n':
		if ((err = scan_null(&ptr, end))) {
			syslog(LOG_ERR, "failed parsing 'null' constant");
			goto error;
		}
		id = DATATYPE_NULL;
		break;

	case 'f':
		if ((err = scan_false(&ptr, end))) {
			syslog(LOG_ERR, "failed parsing 'false' constant");
			goto error;
		}
		id = DATATYPE_BOOL;
		break;

	case 't':
		if ((err = scan_true(&ptr, end))) {
			syslog(LOG_ERR, "failed parsing 'true' constant");
			goto error;
		}
		id = DATATYPE_BOOL;
		break;

	case '"':
		if ((err = scan_text(&ptr, end))) {
			syslog(LOG_ERR, "failed parsing text literal");
			goto error;
		}
		id = DATATYPE_TEXT;
		break;

	case '[':
		if ((err = scan_array(s, &ptr, end, &id))) {
			syslog(LOG_ERR, "failed parsing array");
			goto error;
		}
		break;

	case '{':
		if ((err = scan_record(s, &ptr, end, &id))) {
			syslog(LOG_ERR, "failed parsing record");
			goto error;
		}
		break;

	default:
		if ((err = scan_number(ch, &ptr, end))) {
			syslog(LOG_ERR, "failed parsing number literal");
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
	if (*ptr == ']') {
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

			if ((err = scan_value(s, &ptr, end, &next_id))) {
				goto error;
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
	syslog(LOG_ERR, "no closing bracket (]) at end of array value");
	err = ERROR_INVAL;
	goto error;

error_inval_nocomma:
	syslog(LOG_ERR, "missing comma (,) in array value");
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
	(void)s;
	(void)bufptr;
	(void)end;
	(void)idptr;
	return 0;
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

int scan_text(const uint8_t **bufptr, const uint8_t *end)
{
	const uint8_t *input = *bufptr;
	const uint8_t *ptr = input;
	struct text text;
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
	if ((err = text_assign(&text, input, ptr - input, 0))) {
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
