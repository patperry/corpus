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
#include <stdlib.h>
#include "../lib/utf8lite/src/utf8lite.h"
#include "error.h"
#include "table.h"
#include "textset.h"
#include "stem.h"
#include "symtab.h"
#include "datatype.h"
#include "data.h"

double corpus_strntod(const char *string, size_t maxlen, const char **endPtr);
intmax_t corpus_strntoimax(const char *string, size_t maxlen, char **endptr);

static void scan_value(const uint8_t **bufptr);
static void scan_numeric(const uint8_t **bufptr);
static void scan_text(const uint8_t **bufptr);
static void scan_spaces(const uint8_t **bufptr);
static void scan_spaces_safe(const uint8_t **bufptr, const uint8_t *end);


int corpus_data_assign(struct corpus_data *d, struct corpus_schema *s,
		       const uint8_t *ptr, size_t size)
{
	const uint8_t *end = ptr + size;
	int err, id;

	scan_spaces_safe(&ptr, end);

	if ((err = corpus_schema_scan(s, ptr, (size_t)(end - ptr), &id))) {
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


int corpus_data_bool(const struct corpus_data *d, int *valptr)
{
	int val;
	int err;

	if (d->type_id != CORPUS_DATATYPE_BOOLEAN
			|| d->size == 0 || *d->ptr == 'n') {
		val = INT_MIN;
		err = CORPUS_ERROR_INVAL;
	} else {
		val = *d->ptr == 't' ? 1 : 0;
		err = 0;
	}

	if (valptr) {
		*valptr = val;
	}

	return err;
}


int corpus_data_int(const struct corpus_data *d, int *valptr)
{
	intmax_t lval;
	int val;
	int err;

	if (d->type_id != CORPUS_DATATYPE_INTEGER
			|| d->size == 0 || *d->ptr == 'n') {
		goto nullval;
	}

	errno = 0;
	lval = corpus_strntoimax((const char *)d->ptr, d->size, NULL);
	if (errno == ERANGE) {
		val = lval > 0 ? INT_MAX : INT_MIN;
		err = CORPUS_ERROR_RANGE;
	} else if (lval > INT_MAX) {
		val = INT_MAX;
		err = CORPUS_ERROR_RANGE;
	} else if (lval < INT_MIN) {
		val = INT_MIN;
		err = CORPUS_ERROR_RANGE;
	} else {
		val = (int)lval;
		err = 0;
	}

	goto out;

nullval:
	val = INT_MIN;
	err = CORPUS_ERROR_INVAL;

out:
	if (valptr) {
		*valptr = val;
	}
	return err;
}


int corpus_data_double(const struct corpus_data *d, double *valptr)
{
	const uint8_t *ptr;
	uint_fast8_t ch;
	double val;
	int err;
	int neg = 0;

	if (!(d->type_id == CORPUS_DATATYPE_REAL
				|| d->type_id == CORPUS_DATATYPE_INTEGER)
			|| d->size == 0 || *d->ptr == 'n') {
		goto nullval;
	}

	errno = 0;
	val = corpus_strntod((const char *)d->ptr, d->size,
			     (const char **)&ptr);
	if (ptr != d->ptr) {
		err = (errno == ERANGE) ? CORPUS_ERROR_RANGE : 0;
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
		val = neg ? (double)-INFINITY : (double)INFINITY;
		break;

	default:
		val = neg ? copysign((double)NAN, -1) : (double)NAN;
		break;
	}

	err = 0;
	goto out;

nullval:
	err = CORPUS_ERROR_INVAL;
	val = (double)NAN;
	goto out;

out:
	if (valptr) {
		*valptr = val;
	}
	return err;
}


int corpus_data_text(const struct corpus_data *d, struct utf8lite_text *valptr)
{
	struct utf8lite_text val;
	const uint8_t *ptr;
	const uint8_t *end;
	int err;

	if (d->type_id != CORPUS_DATATYPE_TEXT
			|| d->size == 0 || *d->ptr == 'n') {
		goto nullval;
	}

	ptr = d->ptr;
	end = ptr + d->size;

	ptr++; // skip over leading quote (")

	// skip over trailing whitespace
	end--;
	while (*end != '"') {
		end--;
	}

	err = utf8lite_text_assign(&val, ptr, (size_t)(end - ptr),
			UTF8LITE_TEXT_VALID | UTF8LITE_TEXT_UNESCAPE, NULL);
	goto out;

nullval:
	val.ptr = NULL;
	val.attr = 0;
	err = CORPUS_ERROR_INVAL;
	goto out;

out:
	if (valptr) {
		*valptr = val;
	}
	return err;
}


static void corpus_data_items_make(struct corpus_data_items *it,
				   const struct corpus_schema *s,
				   const uint8_t *ptr,
				   const struct corpus_datatype_array *type)
{
	it->schema = s;
	it->item_type = type->type_id;
	if (it->item_type >= 0) {
		it->item_kind = s->types[it->item_type].kind;
	} else {
		it->item_kind = CORPUS_DATATYPE_ANY;
	}
	it->length = type->length;
	it->ptr = ptr;
	corpus_data_items_reset(it);
}


static void corpus_data_fields_make(struct corpus_data_fields *it,
				    const struct corpus_schema *s,
				    const uint8_t *ptr,
				    const struct corpus_datatype_record *type)
{
	it->schema = s;
	it->field_types = type->type_ids;
	it->field_names = type->name_ids;
	it->nfield = type->nfield;
	it->ptr = ptr;
	corpus_data_fields_reset(it);
}


void corpus_data_items_reset(struct corpus_data_items *it)
{
	it->index = -1;
	it->current.ptr = NULL;
	it->current.size = 0;
	it->current.type_id = CORPUS_DATATYPE_NULL;
}


void corpus_data_fields_reset(struct corpus_data_fields *it)
{
	it->name_id = -1;
	it->current.ptr = NULL;
	it->current.size = 0;
	it->current.type_id = CORPUS_DATATYPE_NULL;
}


int corpus_data_items_advance(struct corpus_data_items *it)
{
	const uint8_t *ptr;
	const uint8_t *end;

	if (it->index == -1) {
		ptr = it->ptr;
		ptr++; // opening ([)

		scan_spaces(&ptr);
		if (*ptr == ']') {
			it->index = 0;
			goto end;
		}
	} else {
		ptr = it->current.ptr + it->current.size;
		scan_spaces(&ptr);
		if (*ptr == ']') {
			// if not already at the end of the array,
			// increment the index
			if (it->current.size > 0) {
				it->index++;
			}
			goto end;
		}

		ptr++; // comma (,)
		scan_spaces(&ptr);
	}
	end = ptr;
	scan_value(&end);

	if (it->item_type == CORPUS_DATATYPE_ANY) {
		// the call to data_assign won't fail because we already
		// have enough space in the schema buffer to parse the
		// array item
		corpus_data_assign(&it->current,
				   (struct corpus_schema *)it->schema,
				   ptr, (size_t)(end - ptr));
	} else {
		it->current.ptr = ptr;
		it->current.size = (size_t)(end - ptr);
		it->current.type_id = it->item_type;
	}
	it->index++;
	return 1;

end:
	it->current.ptr = ptr;
	it->current.size = 0;
	it->current.type_id = CORPUS_DATATYPE_NULL;
	return 0;
}


static int compare_int(const void *x1, const void *x2)
{
	int y1 = *(const int *)x1;
	int y2 = *(const int *)x2;
	int ret;

	if (y1 < y2) {
		ret = -1;
	} else if (y1 > y2) {
		ret = +1;
	} else {
		ret = 0;
	}

	return ret;
}


int corpus_data_fields_advance(struct corpus_data_fields *it)
{
	struct utf8lite_text name;
	const uint8_t *begin;
	const uint8_t *ptr;
	const uint8_t *end;
	int flags, name_id, type_id;
	int *idptr;

	if (it->name_id == -1) {
		ptr = it->ptr;
		ptr++; // opening ({)

		scan_spaces(&ptr);
		if (*ptr == '}') {
			goto end;
		}
	} else {
		ptr = it->current.ptr + it->current.size;
		scan_spaces(&ptr);
		if (*ptr == '}') {
			goto end;
		}

		ptr++; // comma (,)
		scan_spaces(&ptr);
	}

	// "
	ptr++;

	// name
	begin = ptr;
	flags = 0;
	while (*ptr != '"') {
		if (*ptr == '\\') {
			flags = UTF8LITE_TEXT_UNESCAPE;
			ptr++;
		}
		ptr++;
	}
	utf8lite_text_assign(&name, begin, (size_t)(ptr - begin),
			     flags | UTF8LITE_TEXT_VALID, NULL);

	// the call to schema_name always succeeds and does not
	// create a new name, because the field name already
	// exists as part of the type
	corpus_schema_name((struct corpus_schema *)it->schema, &name,
			   &name_id);
	it->name_id = name_id;

	// "
	ptr++;

	// ws
	scan_spaces(&ptr);

	// :
	ptr++;

	// ws
	scan_spaces(&ptr);

	end = ptr;
	scan_value(&end);

	idptr = bsearch(&name_id, it->field_names, (size_t)it->nfield,
			sizeof(*it->field_names), compare_int);
	assert(idptr); // name exists in the record
	type_id = it->field_types[idptr - it->field_names];

	if (type_id == CORPUS_DATATYPE_ANY) {
		// this won't fail, because we already have enough
		// space in the schema buffer to parse the array item
		corpus_data_assign(&it->current,
				   (struct corpus_schema *)it->schema,
				   ptr, (size_t)(end - ptr));
	} else {
		it->current.ptr = ptr;
		it->current.size = (size_t)(end - ptr);
		it->current.type_id = type_id;
	}
	return 1;

end:
	it->current.ptr = ptr;
	it->current.size = 0;
	it->current.type_id = CORPUS_DATATYPE_NULL;
	return 0;
}


int corpus_data_items(const struct corpus_data *d,
		      const struct corpus_schema *s,
		      struct corpus_data_items *valptr)
{
	struct corpus_data_items it;
	const uint8_t *ptr = d->ptr;
	int err;

	if (d->type_id < 0
		|| s->types[d->type_id].kind != CORPUS_DATATYPE_ARRAY
		|| d->size == 0 || *d->ptr == 'n') {
		goto nullval;
	}

	scan_spaces(&ptr);

	corpus_data_items_make(&it, s, ptr, &s->types[d->type_id].meta.array);
	err = 0;
	goto out;

nullval:
	it.schema = NULL;
	it.item_type = CORPUS_DATATYPE_NULL;
	it.length = -1;
	it.ptr = NULL;
	it.current.ptr = NULL;
	it.current.size = 0;
	it.current.type_id = CORPUS_DATATYPE_NULL;
	it.index = -1;
	err = CORPUS_ERROR_INVAL;
out:
	if (valptr) {
		*valptr = it;
	}
	return err;
}


int corpus_data_nitem(const struct corpus_data *d,
		      const struct corpus_schema *s, int *nitemptr)
{
	struct corpus_data_items it;
	int err, nitem;

	if (d->type_id < 0
		|| s->types[d->type_id].kind != CORPUS_DATATYPE_ARRAY
		|| d->size == 0 || *d->ptr == 'n') {
		goto nullval;
	}

	nitem = s->types[d->type_id].meta.array.length;

	if (nitem < 0) {
		nitem = 0;
		corpus_data_items(d, s, &it);
		while (corpus_data_items_advance(&it)) {
			nitem++;
		}
	}
	err = 0;
	goto out;

nullval:
	nitem = -1;
	err = CORPUS_ERROR_INVAL;
out:
	if (nitemptr) {
		*nitemptr = nitem;
	}
	return err;
}


int corpus_data_nfield(const struct corpus_data *d,
		       const struct corpus_schema *s, int *nfieldptr)
{
	struct corpus_data_fields it;
	int err, nfield;

	if (d->type_id < 0
		|| s->types[d->type_id].kind != CORPUS_DATATYPE_RECORD
		|| d->size == 0 || *d->ptr == 'n') {
		goto nullval;
	}

	// the following won't work:
	//     nfield = s->types[d->type_id].meta.record.nfield;
	// (the data type might be that of a supertype with more fields)

	nfield = 0;
	corpus_data_fields(d, s, &it);
	while (corpus_data_fields_advance(&it)) {
		nfield++;
	}

	err = 0;
	goto out;

nullval:
	nfield = -1;
	err = CORPUS_ERROR_INVAL;
out:
	if (nfieldptr) {
		*nfieldptr = nfield;
	}
	return err;
}


int corpus_data_fields(const struct corpus_data *d,
		       const struct corpus_schema *s,
		       struct corpus_data_fields *valptr)
{
	struct corpus_data_fields it;
	const uint8_t *ptr = d->ptr;
	int err;

	if (d->type_id < 0
		|| s->types[d->type_id].kind != CORPUS_DATATYPE_RECORD
		|| d->size == 0 || *d->ptr == 'n') {
		goto nullval;
	}

	scan_spaces(&ptr);

	corpus_data_fields_make(&it, s, ptr, &s->types[d->type_id].meta.record);
	err = 0;
	goto out;

nullval:
	it.schema = NULL;
	it.field_types = NULL;
	it.field_names = NULL;
	it.nfield = 0;
	it.ptr = NULL;
	it.current.ptr = NULL;
	it.current.size = 0;
	it.current.type_id = CORPUS_DATATYPE_NULL;
	it.name_id = -1;
	err = CORPUS_ERROR_INVAL;
out:
	if (valptr) {
		*valptr = it;
	}
	return err;
}


int corpus_data_field(const struct corpus_data *d,
		      const struct corpus_schema *s,
		      int name_id,
		      struct corpus_data *valptr)
{
	const struct corpus_datatype_record *rec;
	struct corpus_data val;
	const uint8_t *begin;
	const uint8_t *ptr = d->ptr;
	const int *idptr;
	struct utf8lite_text name;
	int err, flags, id, type_id;

	if (d->type_id < 0
		|| s->types[d->type_id].kind != CORPUS_DATATYPE_RECORD
		|| d->size == 0 || *d->ptr == 'n') {
		goto nullval;
	}

	rec = &s->types[d->type_id].meta.record;
	idptr = bsearch(&name_id, rec->name_ids, (size_t)rec->nfield,
			sizeof(*rec->name_ids), compare_int);
	if (idptr == NULL) {
		goto nullval;
	}
	type_id = rec->type_ids[idptr - rec->name_ids];

	// {
	ptr++;

	// ws
	scan_spaces(&ptr);

	if (*ptr == '}') {
		goto nullval;
	}

	while (1) {
		// "
		ptr++;

		// name
		begin = ptr;
		flags = 0;
		while (*ptr != '"') {
			if (*ptr == '\\') {
				flags = UTF8LITE_TEXT_UNESCAPE;
				ptr++;
			}
			ptr++;
		}
		utf8lite_text_assign(&name, begin, (size_t)(ptr - begin),
				flags | UTF8LITE_TEXT_VALID, NULL);

		// the call to schema_name always succeeds and does not
		// create a new name, because the field name already
		// exists as part of the type
		corpus_schema_name((struct corpus_schema *)s, &name, &id);

		// "
		ptr++;

		// ws
		scan_spaces(&ptr);

		// :
		ptr++;

		// ws
		scan_spaces(&ptr);

		if (id == name_id) {
			goto found;
		}

		// value
		scan_value(&ptr);

		// ws
		scan_spaces(&ptr);

		if (*ptr == '}') {
			goto nullval;
		}

		// ,
		ptr++;

		// ws
		scan_spaces(&ptr);
	}
found:
	val.ptr = ptr;
	scan_value(&ptr);
	val.size = (size_t)(ptr - val.ptr);
	val.type_id = type_id;
	err = 0;
	goto out;

nullval:
	val.ptr = NULL;
	val.size = 0;
	val.type_id = CORPUS_DATATYPE_NULL;
	err = CORPUS_ERROR_INVAL;
out:
	if (valptr) {
		*valptr = val;
	}
	return err;
}


void scan_value(const uint8_t **bufptr)
{
	const uint8_t *ptr = *bufptr;
	uint_fast8_t ch;
	int depth;

	ch = *ptr++;
	switch (ch) {
	case 'n':
		ptr += 3; // ull
		break;

	case 'f':
		ptr += 4; // alse
		break;

	case 't':
		ptr += 3; // rue
		break;

	case '"':
		scan_text(&ptr);
		break;

	case '[':
		depth = 1;
		while (depth > 0) {
			ch = *ptr++;
			switch (ch) {
			case '[':
				depth++;
				break;
			case ']':
				depth--;
				break;
			case '"':
				scan_text(&ptr);
				break;
			default:
				break;
			}
		}
		break;

	case '{':
		depth = 1;
		while (depth > 0) {
			ch = *ptr++;
			switch (ch) {
			case '{':
				depth++;
				break;
			case '}':
				depth--;
				break;
			case '"':
				scan_text(&ptr);
				break;
			default:
				break;
			}
		}
		break;

	default:
		ptr--;
		scan_numeric(&ptr);
		break;
	}

	*bufptr = ptr;
}


void scan_numeric(const uint8_t **bufptr)
{
	const uint8_t *ptr = *bufptr;

	// leading sign
	if (*ptr == '-' || *ptr == '+') {
		ptr++;
	}

	if (isdigit(*ptr) || *ptr == '.') {
		while (isdigit(*ptr)) {
			ptr++;
		}
		if (*ptr == '.') {
			ptr++;
		}
		while (isdigit(*ptr)) {
			ptr++;
		}
		if (*ptr == 'e' || *ptr == 'E') {
			ptr++;
			if (*ptr == '-' || *ptr == '+') {
				ptr++;
			}
			while (isdigit(*ptr)) {
				ptr++;
			}
		}
	} else if (*ptr == 'I') {
		ptr += 8; // Infinity
	} else {
		ptr += 3; // NaN
	}

	*bufptr = ptr;
}


void scan_text(const uint8_t **bufptr)
{
	const uint8_t *ptr = *bufptr;

	while (*ptr != '"') {
		if (*ptr == '\\') {
			ptr++;
		}
		ptr++;
	}
	ptr++; // trailing "

	*bufptr = ptr;
}


void scan_spaces(const uint8_t **bufptr)
{
	const uint8_t *ptr = *bufptr;
	while (isspace(*ptr)) {
		ptr++;
	}
	*bufptr = ptr;
}


void scan_spaces_safe(const uint8_t **bufptr, const uint8_t *end)
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
