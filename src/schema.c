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

#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "array.h"
#include "errcode.h"
#include "table.h"
#include "text.h"
#include "token.h"
#include "symtab.h"
#include "xalloc.h"
#include "data.h"
#include "schema.h"

#define NUM_ATOMIC	5

static int schema_buffer_init(struct schema_buffer *buf);
static void schema_buffer_destroy(struct schema_buffer *buf);

static int sorter_init(struct schema_sorter *sort);
static void sorter_destroy(struct schema_sorter *sort);
static int sorter_sort(struct schema_sorter *sort, const int *ptr, int n);
static int sorter_reserve(struct schema_sorter *sort, int nfield);

static int schema_has_array(const struct schema *s, int type_id, int length,
			    int *idptr);
static int schema_has_record(const struct schema *s, const int *type_ids,
			     const int *name_ids, int nfield, int *idptr);

static int schema_union_array(struct schema *s, int id1, int id2, int *idptr);
static int schema_union_record(struct schema *s, int id1, int id2, int *idptr);
static int schema_grow_types(struct schema *s, int nadd);

static int is_sorted(const int *ids, int n);


int schema_buffer_init(struct schema_buffer *buf)
{
	buf->type_ids = NULL;
	buf->name_ids = NULL;
	buf->nfield = 0;
	buf->nfield_max = 0;
	return 0;
}


void schema_buffer_destroy(struct schema_buffer *buf)
{
	free(buf->name_ids);
	free(buf->type_ids);
}


int schema_buffer_grow(struct schema_buffer *buf, int nadd)
{
	void *tbase = buf->type_ids;
	int *nbase = buf->name_ids;
	int size = buf->nfield_max;
	int err;

	if ((err = array_grow(&tbase, &size, sizeof(*buf->type_ids),
			      buf->nfield, nadd))) {
		syslog(LOG_ERR, "failed allocating schema buffer");
		return err;
	}
	buf->type_ids = tbase;

	if (size) {
		if (!(nbase = xrealloc(nbase, size * sizeof(*nbase)))) {
			syslog(LOG_ERR, "failed allocating schema buffer");
			return ERROR_NOMEM;
		}
		buf->name_ids = nbase;
	}

	if (buf->nfield > INT_MAX - nadd) {
		syslog(LOG_ERR, "schema buffer size exceeds maximum (%d)",
		       INT_MAX);
		return ERROR_OVERFLOW;
	}

	buf->nfield_max = size;
	return 0;
}


int sorter_init(struct schema_sorter *sort)
{
	sort->idptrs = NULL;
	sort->size = 0;
	return 0;
}


void sorter_destroy(struct schema_sorter *sort)
{
	free(sort->idptrs);
}


static int idptr_cmp(const void *x1, const void *x2)
{
	int id1 = **(const int **)x1;
	int id2 = **(const int **)x2;
	return id1 - id2;
}


int sorter_sort(struct schema_sorter *sort, const int *ptr, int n)
{
	int i;
	int err;

	if ((err = sorter_reserve(sort, n))) {
		goto out;
	}

	for (i = 0; i < n; i++) {
		sort->idptrs[i] = ptr + i;
	}

	qsort(sort->idptrs, n, sizeof(*sort->idptrs), idptr_cmp);
out:
	return err;
}


int sorter_reserve(struct schema_sorter *sort, int length)
{
	void *base = sort->idptrs;
	int n = sort->size;
	int nadd;
	int err;

	if (n >= length) {
		return 0;
	}

	nadd = length - n;

	if ((err = array_grow(&base, &n, sizeof(*sort->idptrs), n, nadd))) {
		syslog(LOG_ERR, "failed allocating schema sorter");
		return err;
	}
	sort->idptrs = base;
	sort->size = n;
	return 0;
}


int schema_init(struct schema *s)
{
	int i, n;
	int err;

	if ((err = schema_buffer_init(&s->buffer))) {
		goto error_buffer;
	}

	if ((err = sorter_init(&s->sorter))) {
		goto error_sorter;
	}

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
	sorter_destroy(&s->sorter);
error_sorter:
	schema_buffer_destroy(&s->buffer);
error_buffer:
	return err;
}


void schema_destroy(struct schema *s)
{
	schema_clear(s);
	free(s->types);
	symtab_destroy(&s->names);
	sorter_destroy(&s->sorter);
	schema_buffer_destroy(&s->buffer);
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


int schema_name(struct schema *s, const struct text *name, int *idptr)
{
	int tokid, id;
	int err;

	if ((err = symtab_add_token(&s->names, name, &tokid))) {
		goto error;
	}

	id = s->names.tokens[tokid].type_id;
	err = 0;
	goto out;

error:
	syslog(LOG_ERR, "failed adding name to schema");
	id = -1;

out:
	if (idptr) {
		*idptr = id;
	}

	return err;
}


int schema_array(struct schema *s, int type_id, int length, int *idptr)
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
	id = DATATYPE_ANY;

out:
	if (idptr) {
		*idptr = id;
	}
	return err;
}


int schema_has_array(const struct schema *s, int type_id, int length,
		     int *idptr)
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


int schema_record(struct schema *s, const int *type_ids, const int *name_ids,
		  int nfield, int *idptr)
{
	struct datatype *t;
	int i, id, index, fstart, did_copy = 0;
	int err;

	if (is_sorted(name_ids, nfield)) {
		goto sorted;
	}

	if ((err = sorter_sort(&s->sorter, name_ids, nfield))) {
		goto error;
	}

	if ((err = schema_buffer_grow(&s->buffer, nfield))) {
		goto error;
	}

	fstart = s->buffer.nfield;
	s->buffer.nfield += nfield;
	did_copy = 1;

	for (i = 0; i < nfield; i++) {
		index = (int)(s->sorter.idptrs[i] - name_ids);
		s->buffer.type_ids[fstart + i] = type_ids[index];
		s->buffer.name_ids[fstart + i] = name_ids[index];

		if (i > 0 && s->buffer.name_ids[fstart + i] ==
				s->buffer.name_ids[fstart + i - 1]) {
			goto error_duplicate;
		}
	}

	type_ids = s->buffer.type_ids + fstart;
	name_ids = s->buffer.name_ids + fstart;

sorted:
	if (schema_has_record(s, type_ids, name_ids, nfield, &id)) {
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
	t->kind = DATATYPE_RECORD;
	if (nfield == 0) {
		t->meta.record.type_ids = NULL;
		t->meta.record.name_ids = NULL;
	} else {
		t->meta.record.type_ids = xmalloc(nfield * sizeof(*type_ids));
		t->meta.record.name_ids = xmalloc(nfield * sizeof(*name_ids));
		if (!t->meta.record.type_ids || !t->meta.record.type_ids) {
			free(t->meta.record.type_ids);
			free(t->meta.record.name_ids);
			err = ERROR_NOMEM;
			goto error;
		}
		memcpy(t->meta.record.type_ids, type_ids,
			nfield * sizeof(*type_ids));
		memcpy(t->meta.record.name_ids, name_ids,
			nfield * sizeof(*name_ids));
	}
	t->meta.record.nfield = nfield;
	s->ntype++;

	err = 0;
	goto out;

error_duplicate:
	syslog(LOG_ERR, "duplicate field name \"%.*s\" in record",
		(int)TEXT_SIZE(&s->names.types[name_ids[index]].text),
		s->names.types[name_ids[index]].text.ptr);
	err = ERROR_INVAL;
	goto error;

error:
	syslog(LOG_ERR, "failed adding record type");
	id = DATATYPE_ANY;

out:
	if (did_copy) {
		s->buffer.nfield = fstart;
	}
	if (idptr) {
		*idptr = id;
	}
	return err;
}


int schema_has_record(const struct schema *s, const int *type_ids,
		      const int *name_ids, int nfield, int *idptr)
{
	const struct datatype *t;
	int id = s->ntype;
	int found;

	while (id-- > 0) {
		t = &s->types[id];
		if (t->kind != DATATYPE_RECORD) {
			continue;
		}
		if (t->meta.record.nfield == nfield
			&& memcmp(t->meta.record.type_ids, type_ids,
				  nfield * sizeof(*type_ids)) == 0
			&& memcmp(t->meta.record.name_ids, name_ids,
				  nfield * sizeof(*name_ids)) == 0) {
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


int schema_union(struct schema *s, int id1, int id2, int *idptr)
{
	int kind1, kind2;
	int id;
	int err = 0;

	if (id1 == id2 || id2 == DATATYPE_NULL) {
		id = id1;
		goto out;
	}

	if (id1 == DATATYPE_ANY || id2 == DATATYPE_ANY) {
		id = DATATYPE_ANY;
		goto out;
	}

	switch (id1) {
	case DATATYPE_NULL:
		id = id2;
		goto out;

	case DATATYPE_INTEGER:
		if (id2 == DATATYPE_REAL) {
			id = DATATYPE_REAL;
			goto out;
		}
		break;

	case DATATYPE_REAL:
		if (id2 == DATATYPE_INTEGER) {
			id = DATATYPE_REAL;
			goto out;
		}
		break;
	}

	// if we got here, then neither kind is Null or Any
	kind1 = s->types[id1].kind;
	kind2 = s->types[id2].kind;

	if (kind1 != kind2) {
		id = DATATYPE_ANY;
	} else if (kind1 == DATATYPE_ARRAY) {
		err = schema_union_array(s, id1, id2, &id);
	} else if (kind1 == DATATYPE_RECORD) {
		err = schema_union_record(s, id1, id2, &id);
	} else {
		id = DATATYPE_ANY;
	}

out:
	if (idptr) {
		*idptr = id;
	}
	return err;
}



int schema_union_array(struct schema *s, int id1, int id2, int *idptr)
{
	const struct datatype_array *t1, *t2;
	int len;
	int elt, id;
	int err;

	t1 = &s->types[id1].meta.array;
	t2 = &s->types[id2].meta.array;

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
	syslog(LOG_ERR, "failed computing union of array types");
	id = DATATYPE_ANY;

out:
	if (idptr) {
		*idptr = id;
	}

	return err;
}


int schema_union_record(struct schema *s, int id1, int id2, int *idptr)
{
	const struct datatype_record *t1, *t2;
	int fstart, i1, i2, n1, n2, name_id, type_id, nfield;
	int id = DATATYPE_ANY;
	int err;

	fstart = s->buffer.nfield;
	t1 = &s->types[id1].meta.record;
	t2 = &s->types[id2].meta.record;
	n1 = t1->nfield;
	n2 = t2->nfield;

	nfield = 0;
	i1 = 0;
	i2 = 0;

	while (i1 < n1 && i2 < n2) {
		if (t1->name_ids[i1] == t2->name_ids[i2]) {
			name_id = t1->name_ids[i1];
			if ((err = schema_union(s, t1->type_ids[i1],
						t2->type_ids[i2], &type_id))) {
				goto error;
			}
			i1++;
			i2++;
		} else if (t1->name_ids[i1] < t2->name_ids[i2]) {
			name_id = t1->name_ids[i1];
			type_id = t1->type_ids[i1];
			i1++;
		} else {
			name_id = t2->name_ids[i2];
			type_id = t2->type_ids[i2];
			i2++;
		}
		if ((err = schema_buffer_grow(&s->buffer, 1))) {
			goto error;
		}

		s->buffer.nfield++;
		s->buffer.name_ids[fstart + nfield] = name_id;
		s->buffer.type_ids[fstart + nfield] = type_id;
		nfield++;
	}

	if (i1 < n1) {
		if ((err = schema_buffer_grow(&s->buffer, n1 - i1))) {
			goto error;
		}
		s->buffer.nfield += n1 - i1;
		memcpy(s->buffer.name_ids + fstart + nfield,
			t1->name_ids + i1, (n1 - i1) * sizeof(*t1->name_ids));
		memcpy(s->buffer.type_ids + fstart + nfield,
			t1->type_ids + i1, (n1 - i1) * sizeof(*t1->type_ids));
		nfield += n1 - i1;
	}

	if (i2 < n2) {
		if ((err = schema_buffer_grow(&s->buffer, n2 - i2))) {
			goto error;
		}
		s->buffer.nfield += n2 - i2;
		memcpy(s->buffer.name_ids + fstart + nfield,
			t2->name_ids + i2, (n2 - i2) * sizeof(*t2->name_ids));
		memcpy(s->buffer.type_ids + fstart + nfield,
			t2->type_ids + i2, (n2 - i2) * sizeof(*t2->type_ids));
		nfield += n2 - i2;
	}

	err = schema_record(s, s->buffer.type_ids + fstart,
			    s->buffer.name_ids + fstart, nfield, &id);
	goto out;

error:
	syslog(LOG_ERR, "failed computing union of record types");
	id = DATATYPE_ANY;
	goto out;

out:
	if (idptr) {
		*idptr = id;
	}
	s->buffer.nfield = fstart;
	return err;
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


int write_datatype(FILE *stream, const struct schema *s, int id)
{
	const struct text *name;
	const struct datatype *t;
	int name_id, type_id;
	int i, n;
	int err;

	if (id < 0) {
		if (fprintf(stream, "any") < 0) {
			goto error_os;
		}
		err = 0;
		goto out;
	}

	t = &s->types[id];

	switch (t->kind) {
	case DATATYPE_NULL:
		if (fprintf(stream, "null") < 0) {
			goto error_os;
		}
		break;

	case DATATYPE_BOOLEAN:
		if (fprintf(stream, "boolean") < 0) {
			goto error_os;
		}
		break;

	case DATATYPE_INTEGER:
		if (fprintf(stream, "integer") < 0) {
			goto error_os;
		}
		break;

	case DATATYPE_REAL:
		if (fprintf(stream, "real") < 0) {
			goto error_os;
		}
		break;

	case DATATYPE_TEXT:
		if (fprintf(stream, "text") < 0) {
			goto error_os;
		}
		break;

	case DATATYPE_ARRAY:
		if (fprintf(stream, "[") < 0) {
			goto error_os;
		}
		if ((err = write_datatype(stream, s, t->meta.array.type_id))) {
			goto error;
		}
		if (t->meta.array.length >= 0) {
			if (fprintf(stream, "; %d",
				    t->meta.array.length) < 0) {
				goto error_os;
			}
		}
		if (fprintf(stream, "] (#%d)", id) < 0) {
			goto error_os;
		}
		break;

	case DATATYPE_RECORD:
		if (fprintf(stream, "{") < 0) {
			goto error_os;
		}

		n = t->meta.record.nfield;
		for (i = 0; i < n; i++) {
			if (i > 0 && fprintf(stream, ", ") < 0) {
				goto error_os;
			}

			name_id = t->meta.record.name_ids[i];
			name = &s->names.types[name_id].text;
			if (fprintf(stream, "\"%.*s\": ",
				    (unsigned)TEXT_SIZE(name),
				    name->ptr) < 0) {
				goto error_os;
			}

			type_id = t->meta.record.type_ids[i];
			if ((err = write_datatype(stream, s, type_id))) {
				goto error;
			}
		}

		if (fprintf(stream, "} (#%d)", id) < 0) {
			goto error_os;
		}
		break;

	default:
		syslog(LOG_ERR, "internal error: invalid datatype kind");
		err = ERROR_INTERNAL;
		goto error;
	}

	err = 0;
	goto out;

error_os:
	err = ERROR_OS;
error:
out:
	return err;
}


int is_sorted(const int *ids, int n)
{
	int i;

	if (n > 1) {
		for (i = 0; i < n - 1; i++) {
			if (ids[i] >= ids[i+1]) {
				return 0;
			}
		}
	}

	return 1;
}
