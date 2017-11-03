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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../lib/utf8lite/src/utf8lite.h"
#include "array.h"
#include "error.h"
#include "memory.h"
#include "table.h"
#include "textset.h"
#include "stem.h"
#include "symtab.h"
#include "data.h"
#include "datatype.h"

#define CORPUS_NUM_ATOMIC	5

static int corpus_schema_buffer_init(struct corpus_schema_buffer *buf);
static void corpus_schema_buffer_destroy(struct corpus_schema_buffer *buf);
static int corpus_schema_buffer_grow(struct corpus_schema_buffer *buf,
				     int nadd);

static int sorter_init(struct corpus_schema_sorter *sort);
static void sorter_destroy(struct corpus_schema_sorter *sort);
static int sorter_sort(struct corpus_schema_sorter *sort, const int *ptr,
		       int n);
static int sorter_reserve(struct corpus_schema_sorter *sort, int nfield);

static int corpus_schema_has_array(const struct corpus_schema *s, int type_id,
				   int length, int *idptr);
static int corpus_schema_has_record(const struct corpus_schema *s,
				    const int *type_ids, const int *name_ids,
				    int nfield, int *idptr);

static int corpus_schema_union_array(struct corpus_schema *s, int id1,
				     int id2, int *idptr);
static int corpus_schema_union_record(struct corpus_schema *s, int id1,
				      int id2, int *idptr);
static int corpus_schema_grow_types(struct corpus_schema *s, int nadd);
static void corpus_schema_rehash_arrays(struct corpus_schema *s);
static void corpus_schema_rehash_records(struct corpus_schema *s);

// compound types
static int scan_field(struct corpus_schema *s, const uint8_t **bufptr,
		      const uint8_t *end, int *name_idptr, int *type_idptr);
static int scan_value(struct corpus_schema *s, const uint8_t **bufptr,
		      const uint8_t *end, int *idptr);
static int scan_array(struct corpus_schema *s, const uint8_t **bufptr,
		      const uint8_t *end, int *idptr);
static int scan_record(struct corpus_schema *s, const uint8_t **bufptr,
		       const uint8_t *end, int *idptr);

// literals
static int scan_null(const uint8_t **bufptr, const uint8_t *end);
static int scan_false(const uint8_t **bufptr, const uint8_t *end);
static int scan_true(const uint8_t **bufptr, const uint8_t *end);
static int scan_text(const uint8_t **bufptr, const uint8_t *end,
		     struct utf8lite_text *text);
static int scan_numeric(const uint8_t **bufptr, const uint8_t *end,
			int *idptr);
static int scan_infinity(const uint8_t **bufptr, const uint8_t *end);
static int scan_nan(const uint8_t **bufptr, const uint8_t *end);

// helpers
static void scan_digits(const uint8_t **bufptr, const uint8_t *end);
static void scan_spaces(const uint8_t **bufptr, const uint8_t *end);
static int scan_chars(const char *str, unsigned len, const uint8_t **bufptr,
		      const uint8_t *end);
static int scan_char(char c, const uint8_t **bufptr, const uint8_t *end);

static int array_equals(const struct corpus_datatype_array *t1,
			 const struct corpus_datatype_array *t2);
static int record_equals(const struct corpus_datatype_record *t1,
			 const struct corpus_datatype_record *t2);

static unsigned array_hash(const struct corpus_datatype_array *t);
static unsigned record_hash(const struct corpus_datatype_record *t);
static unsigned hash_combine(unsigned seed, unsigned hash);


int corpus_schema_buffer_init(struct corpus_schema_buffer *buf)
{
	buf->type_ids = NULL;
	buf->name_ids = NULL;
	buf->nfield = 0;
	buf->nfield_max = 0;
	return 0;
}


void corpus_schema_buffer_destroy(struct corpus_schema_buffer *buf)
{
	corpus_free(buf->name_ids);
	corpus_free(buf->type_ids);
}


int corpus_schema_buffer_grow(struct corpus_schema_buffer *buf, int nadd)
{
	void *tbase = buf->type_ids;
	int *nbase = buf->name_ids;
	int size = buf->nfield_max;
	int err;

	if ((err = corpus_array_grow(&tbase, &size, sizeof(*buf->type_ids),
				     buf->nfield, nadd))) {
		corpus_log(err, "failed allocating schema buffer");
		return err;
	}
	buf->type_ids = tbase;

	if (size) {
		nbase = corpus_realloc(nbase, (size_t)size * sizeof(*nbase));
		if (!nbase) {
			err = CORPUS_ERROR_NOMEM;
			corpus_log(err, "failed allocating schema buffer");
			return err;
		}
		buf->name_ids = nbase;
	}

	buf->nfield_max = size;
	return 0;
}


int sorter_init(struct corpus_schema_sorter *sort)
{
	sort->idptrs = NULL;
	sort->size = 0;
	return 0;
}


void sorter_destroy(struct corpus_schema_sorter *sort)
{
	corpus_free(sort->idptrs);
}


static int idptr_cmp(const void *x1, const void *x2)
{
	int id1 = **(int * const *)x1;
	int id2 = **(int * const *)x2;
	return id1 - id2;
}


int sorter_sort(struct corpus_schema_sorter *sort, const int *ptr, int n)
{
	int i;
	int err;

	assert(n >= 0);

	if ((err = sorter_reserve(sort, n))) {
		goto out;
	}

	for (i = 0; i < n; i++) {
		sort->idptrs[i] = ptr + i;
	}

	qsort(sort->idptrs, (size_t)n, sizeof(*sort->idptrs), idptr_cmp);
out:
	return err;
}


int sorter_reserve(struct corpus_schema_sorter *sort, int length)
{
	void *base = sort->idptrs;
	int n = sort->size;
	int nadd;
	int err;

	if (n >= length) {
		return 0;
	}

	nadd = length - n;

	err = corpus_array_grow(&base, &n, sizeof(*sort->idptrs), n, nadd);
	if (err) {
		corpus_log(err, "failed allocating schema sorter");
		return err;
	}

	sort->idptrs = base;
	sort->size = n;
	return 0;
}


int corpus_schema_init(struct corpus_schema *s)
{
	int i, n;
	int err;

	if ((err = corpus_schema_buffer_init(&s->buffer))) {
		goto error_buffer;
	}

	if ((err = sorter_init(&s->sorter))) {
		goto error_sorter;
	}

	if ((err = corpus_symtab_init(&s->names, 0))) {
		goto error_names;
	}

	if ((err = corpus_table_init(&s->arrays))) {
		goto error_arrays;
	}

	if ((err = corpus_table_init(&s->records))) {
		goto error_records;
	}

	// initialize primitive types
	n = CORPUS_NUM_ATOMIC;
	if (!(s->types = corpus_malloc((size_t)n * sizeof(*s->types)))) {
		goto error_types;
	}
	s->ntype = n;
	s->narray = 0;
	s->nrecord = 0;
	s->ntype_max = n;

	for (i = 0; i < n; i++) {
		s->types[i].kind = i;
	}

	return 0;

error_types:
	corpus_table_destroy(&s->records);
error_records:
	corpus_table_destroy(&s->arrays);
error_arrays:
	corpus_symtab_destroy(&s->names);
error_names:
	sorter_destroy(&s->sorter);
error_sorter:
	corpus_schema_buffer_destroy(&s->buffer);
error_buffer:
	return err;
}


void corpus_schema_destroy(struct corpus_schema *s)
{
	corpus_schema_clear(s);

	corpus_free(s->types);
	corpus_symtab_destroy(&s->names);
	corpus_table_destroy(&s->records);
	corpus_table_destroy(&s->arrays);
	sorter_destroy(&s->sorter);
	corpus_schema_buffer_destroy(&s->buffer);
}


void corpus_schema_clear(struct corpus_schema *s)
{
	const struct corpus_datatype *t;
	int i = s->ntype;

	while (i-- > 0) {
		t = &s->types[i];
		if (t->kind == CORPUS_DATATYPE_RECORD) {
			corpus_free(t->meta.record.name_ids);
			corpus_free(t->meta.record.type_ids);
		}
	}
	s->ntype = CORPUS_NUM_ATOMIC;
	s->narray = 0;
	s->nrecord = 0;

	corpus_table_clear(&s->arrays);
	corpus_table_clear(&s->records);
	corpus_symtab_clear(&s->names);
}


int corpus_schema_name(struct corpus_schema *s,
		       const struct utf8lite_text *name, int *idptr)
{
	int tokid, id;
	int err;

	if ((err = corpus_symtab_add_token(&s->names, name, &tokid))) {
		goto error;
	}

	id = s->names.tokens[tokid].type_id;
	err = 0;
	goto out;

error:
	corpus_log(err, "failed adding name to schema");
	id = -1;

out:
	if (idptr) {
		*idptr = id;
	}

	return err;
}


int corpus_schema_array(struct corpus_schema *s, int type_id, int length,
			int *idptr)
{
	struct corpus_datatype *t;
	int id, pos, rehash;
	int err;

	rehash = 0;

	if (corpus_schema_has_array(s, type_id, length, &id)) {
		err = 0;
		goto out;
	}
	pos = id;	// table id
	id = s->ntype;	// new type id

	// grow types array if necessary
	if (s->ntype == s->ntype_max) {
		if ((err = corpus_schema_grow_types(s, 1))) {
			goto error;
		}
	}

	// grow array table if necessary
	if (s->narray == s->arrays.capacity) {
		if ((err = corpus_table_reinit(&s->arrays, s->narray + 1))) {
			goto error;
		}
		rehash = 1;
	}

	// add the new type
	t = &s->types[id];
	t->kind = CORPUS_DATATYPE_ARRAY;
	t->meta.array.type_id = type_id;
	t->meta.array.length = length;
	s->ntype++;
	s->narray++;

	// set the table entry
	if (rehash) {
		corpus_schema_rehash_arrays(s);
	} else {
		s->arrays.items[pos] = id;
	}

	err = 0;
	goto out;

error:
	corpus_log(err, "failed adding array type");
	id = CORPUS_DATATYPE_ANY;
	if (rehash) {
		corpus_schema_rehash_arrays(s);
	}

out:
	if (idptr) {
		*idptr = id;
	}
	return err;
}


int corpus_schema_has_array(const struct corpus_schema *s, int type_id,
			    int length, int *idptr)
{
	struct corpus_table_probe probe;
	struct corpus_datatype_array key = {
		.type_id = type_id,
		.length = length
	};
	unsigned hash = array_hash(&key);
	int id = -1;
	int found = 0;

	corpus_table_probe_make(&probe, &s->arrays, hash);
	while (corpus_table_probe_advance(&probe)) {
		id = probe.current;
		if (array_equals(&key, &s->types[id].meta.array)) {
			found = 1;
			goto out;
		}
	}
out:
	if (idptr) {
		*idptr = found ? id : probe.index;
	}
	return found;
}


static int is_sorted(const int *ids, int n)
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


void corpus_schema_rehash_arrays(struct corpus_schema *s)
{
	const struct corpus_datatype_array *t;
	int i, n = s->ntype;
	unsigned hash;

	corpus_table_clear(&s->arrays);

	for (i = 0; i < n; i++) {
		if (s->types[i].kind != CORPUS_DATATYPE_ARRAY) {
			continue;
		}
		t = &s->types[i].meta.array;
		hash = array_hash(t);
		corpus_table_add(&s->arrays, hash, i);
	}
}


void corpus_schema_rehash_records(struct corpus_schema *s)
{
	const struct corpus_datatype_record *t;
	int i, n = s->ntype;
	unsigned hash;

	corpus_table_clear(&s->records);

	for (i = 0; i < n; i++) {
		if (s->types[i].kind != CORPUS_DATATYPE_RECORD) {
			continue;
		}
		t = &s->types[i].meta.record;
		hash = record_hash(t);
		corpus_table_add(&s->records, hash, i);
	}
}


int corpus_schema_record(struct corpus_schema *s, const int *type_ids,
			 const int *name_ids, int nfield, int *idptr)
{
	struct corpus_datatype *t;
	int i, id, index, fstart, did_copy, pos, rehash;
	int err;

	did_copy = 0;
	rehash = 0;
	fstart = -1;

	if (is_sorted(name_ids, nfield)) {
		goto sorted;
	}

	// make space for the name_ids on the stack
	if (s->buffer.nfield > s->buffer.nfield_max - nfield) {
		const int *base = s->buffer.type_ids;
		const int *top = base + s->buffer.nfield;
		int argstart, on_stack;

		if (base <= type_ids && type_ids <= top) {
			on_stack = 1;
			argstart = (int)(type_ids - base);
			assert(argstart == name_ids - s->buffer.name_ids);
		} else {
			on_stack = 0;
			argstart = -1;
		}

		if ((err = corpus_schema_buffer_grow(&s->buffer, nfield))) {
			goto error;
		}
		if (on_stack && s->buffer.type_ids != base) {
			assert(argstart != -1);
			type_ids = s->buffer.type_ids + argstart;
			name_ids = s->buffer.name_ids + argstart;
		}
	}
	assert(s->buffer.nfield + nfield <= s->buffer.nfield_max);

	// sort the name ids
	if ((err = sorter_sort(&s->sorter, name_ids, nfield))) {
		goto error;
	}

	fstart = s->buffer.nfield;
	s->buffer.nfield += nfield;
	did_copy = 1;

	// push the sorted (name,type) pairs onto the stack
	for (i = 0; i < nfield; i++) {
		index = (int)(s->sorter.idptrs[i] - name_ids);
		s->buffer.type_ids[fstart + i] = type_ids[index];
		s->buffer.name_ids[fstart + i] = name_ids[index];

		if (i > 0 && s->buffer.name_ids[fstart + i] ==
				s->buffer.name_ids[fstart + i - 1]) {
			goto error_duplicate;
		}
	}

	// switch to the stack for the types and names
	type_ids = s->buffer.type_ids + fstart;
	name_ids = s->buffer.name_ids + fstart;

sorted:
	if (corpus_schema_has_record(s, type_ids, name_ids, nfield, &id)) {
		err = 0;
		goto out;
	}
	pos = id;	// table position
	id = s->ntype;	// new type id

	// grow types array if necessary
	if (s->ntype == s->ntype_max) {
		if ((err = corpus_schema_grow_types(s, 1))) {
			goto error;
		}
	}

	// grow the record table if necessary
	if (s->nrecord == s->records.capacity) {
		if ((err = corpus_table_reinit(&s->records, s->nrecord + 1))) {
			goto error;
		}
		rehash = 1;
	}

	// add the new type
	t = &s->types[id];
	t->kind = CORPUS_DATATYPE_RECORD;
	if (nfield == 0) {
		t->meta.record.type_ids = NULL;
		t->meta.record.name_ids = NULL;
	} else {
		t->meta.record.type_ids = corpus_malloc((size_t)nfield
							* sizeof(*type_ids));
		t->meta.record.name_ids = corpus_malloc((size_t)nfield
							* sizeof(*name_ids));
		if (!t->meta.record.type_ids || !t->meta.record.type_ids) {
			corpus_free(t->meta.record.type_ids);
			corpus_free(t->meta.record.name_ids);
			err = CORPUS_ERROR_NOMEM;
			goto error;
		}
		memcpy(t->meta.record.type_ids, type_ids,
			(size_t)nfield * sizeof(*type_ids));
		memcpy(t->meta.record.name_ids, name_ids,
			(size_t)nfield * sizeof(*name_ids));
	}
	t->meta.record.nfield = nfield;
	s->ntype++;
	s->nrecord++;

	// set the table entry
	if (rehash) {
		corpus_schema_rehash_records(s);
	} else {
		s->records.items[pos] = id;
	}

	err = 0;
	goto out;

error_duplicate:
	err = CORPUS_ERROR_INVAL;
	corpus_log(err, "duplicate field name \"%.*s\" in record",
		   (int)UTF8LITE_TEXT_SIZE(&s->names.types[name_ids[index]].text),
		   s->names.types[name_ids[index]].text.ptr);
	goto error;

error:
	corpus_log(err, "failed adding record type");
	id = CORPUS_DATATYPE_ANY;
	if (rehash) {
		corpus_schema_rehash_records(s);
	}

out:
	if (did_copy) {
		s->buffer.nfield = fstart;
	}
	if (idptr) {
		*idptr = id;
	}
	return err;
}


int corpus_schema_has_record(const struct corpus_schema *s,
			     const int *type_ids, const int *name_ids,
			     int nfield, int *idptr)
{
	struct corpus_table_probe probe;
	struct corpus_datatype_record key = {
		.type_ids = (int *)type_ids,
		.name_ids = (int *)name_ids,
		.nfield = nfield
	};
	unsigned hash = record_hash(&key);
	int id = -1;
	int found = 0;

	corpus_table_probe_make(&probe, &s->records, hash);
	while (corpus_table_probe_advance(&probe)) {
		id = probe.current;
		if (record_equals(&key, &s->types[id].meta.record)) {
			found = 1;
			goto out;
		}
	}
out:
	if (idptr) {
		*idptr = found ? id : probe.index;
	}
	return found;
}


int corpus_schema_union(struct corpus_schema *s, int id1, int id2, int *idptr)
{
	int kind1, kind2;
	int id;
	int err = 0;

	if (id1 == id2 || id2 == CORPUS_DATATYPE_NULL) {
		id = id1;
		goto out;
	}

	if (id1 == CORPUS_DATATYPE_ANY || id2 == CORPUS_DATATYPE_ANY) {
		id = CORPUS_DATATYPE_ANY;
		goto out;
	}

	switch (id1) {
	case CORPUS_DATATYPE_NULL:
		id = id2;
		goto out;

	case CORPUS_DATATYPE_INTEGER:
		if (id2 == CORPUS_DATATYPE_REAL) {
			id = CORPUS_DATATYPE_REAL;
			goto out;
		}
		break;

	case CORPUS_DATATYPE_REAL:
		if (id2 == CORPUS_DATATYPE_INTEGER) {
			id = CORPUS_DATATYPE_REAL;
			goto out;
		}
		break;
	}

	// if we got here, then neither kind is Null or Any
	kind1 = s->types[id1].kind;
	kind2 = s->types[id2].kind;

	if (kind1 != kind2) {
		id = CORPUS_DATATYPE_ANY;
	} else if (kind1 == CORPUS_DATATYPE_ARRAY) {
		err = corpus_schema_union_array(s, id1, id2, &id);
	} else if (kind1 == CORPUS_DATATYPE_RECORD) {
		err = corpus_schema_union_record(s, id1, id2, &id);
	} else {
		id = CORPUS_DATATYPE_ANY;
	}

out:
	if (idptr) {
		*idptr = id;
	}
	return err;
}



int corpus_schema_union_array(struct corpus_schema *s, int id1, int id2,
			      int *idptr)
{
	const struct corpus_datatype_array *t1, *t2;
	int len;
	int elt, id;
	int err;

	t1 = &s->types[id1].meta.array;
	t2 = &s->types[id2].meta.array;

	if ((err = corpus_schema_union(s, t1->type_id, t2->type_id, &elt))) {
		goto error;
	}

	if (t1->length == t2->length) {
		len = t1->length;
	} else {
		len = -1;
	}

	if ((err = corpus_schema_array(s, elt, len, &id))) {
		goto error;
	}

	goto out;

error:
	corpus_log(err, "failed computing union of array types");
	id = CORPUS_DATATYPE_ANY;

out:
	if (idptr) {
		*idptr = id;
	}

	return err;
}


int corpus_schema_union_record(struct corpus_schema *s, int id1, int id2,
			       int *idptr)
{
	const struct corpus_datatype_record t1 = s->types[id1].meta.record;
	const struct corpus_datatype_record t2 = s->types[id2].meta.record;
	int fstart = s->buffer.nfield;
	int i1, i2, n1, n2, name_id, type_id, nfield;
	int id = CORPUS_DATATYPE_ANY;
	int err;

	n1 = t1.nfield;
	n2 = t2.nfield;

	nfield = 0;
	i1 = 0;
	i2 = 0;

	while (i1 < n1 && i2 < n2) {
		if (t1.name_ids[i1] == t2.name_ids[i2]) {
			name_id = t1.name_ids[i1];
			if ((err = corpus_schema_union(s, t1.type_ids[i1],
						t2.type_ids[i2], &type_id))) {
				goto error;
			}
			i1++;
			i2++;
		} else if (t1.name_ids[i1] < t2.name_ids[i2]) {
			name_id = t1.name_ids[i1];
			type_id = t1.type_ids[i1];
			i1++;
		} else {
			name_id = t2.name_ids[i2];
			type_id = t2.type_ids[i2];
			i2++;
		}
		if ((err = corpus_schema_buffer_grow(&s->buffer, 1))) {
			goto error;
		}

		s->buffer.nfield++;
		s->buffer.name_ids[fstart + nfield] = name_id;
		s->buffer.type_ids[fstart + nfield] = type_id;
		nfield++;
	}

	if (i1 < n1) {
		if ((err = corpus_schema_buffer_grow(&s->buffer, n1 - i1))) {
			goto error;
		}
		s->buffer.nfield += n1 - i1;
		memcpy(s->buffer.name_ids + fstart + nfield,
			t1.name_ids + i1,
			(size_t)(n1 - i1) * sizeof(*t1.name_ids));
		memcpy(s->buffer.type_ids + fstart + nfield,
			t1.type_ids + i1,
			(size_t)(n1 - i1) * sizeof(*t1.type_ids));
		nfield += n1 - i1;
	}

	if (i2 < n2) {
		if ((err = corpus_schema_buffer_grow(&s->buffer, n2 - i2))) {
			goto error;
		}
		s->buffer.nfield += n2 - i2;
		memcpy(s->buffer.name_ids + fstart + nfield,
			t2.name_ids + i2,
			(size_t)(n2 - i2) * sizeof(*t2.name_ids));
		memcpy(s->buffer.type_ids + fstart + nfield,
			t2.type_ids + i2,
			(size_t)(n2 - i2) * sizeof(*t2.type_ids));
		nfield += n2 - i2;
	}

	if ((err = corpus_schema_record(s, s->buffer.type_ids + fstart,
				 s->buffer.name_ids + fstart, nfield, &id))) {
		goto error;
	}

	goto out;

error:
	corpus_log(err, "failed computing union of record types");
	id = CORPUS_DATATYPE_ANY;
	goto out;

out:
	if (idptr) {
		*idptr = id;
	}
	s->buffer.nfield = fstart;
	return err;
}


int corpus_schema_grow_types(struct corpus_schema *s, int nadd)
{
	void *base = s->types;
	int size = s->ntype_max;
	int err;

	if ((err = corpus_array_grow(&base, &size, sizeof(*s->types),
				     s->ntype, nadd))) {
		corpus_log(err, "failed allocating type array");
		return err;
	}

	s->types = base;
	s->ntype_max = size;
	return 0;
}


void corpus_render_datatype(struct utf8lite_render *r,
			    const struct corpus_schema *s, int id)
{
	const struct utf8lite_text *name;
	const struct corpus_datatype *t;
	int flags, name_id, type_id;
	int i, n;

	if (id < 0) {
		utf8lite_render_string(r, "any");
		return;
	}

	t = &s->types[id];

	flags = (r->flags & ~UTF8LITE_ENCODE_C) | UTF8LITE_ENCODE_JSON;
	flags = utf8lite_render_set_flags(r, flags);

	switch (t->kind) {
	case CORPUS_DATATYPE_NULL:
		utf8lite_render_string(r, "null");
		break;

	case CORPUS_DATATYPE_BOOLEAN:
		utf8lite_render_string(r, "boolean");
		break;

	case CORPUS_DATATYPE_INTEGER:
		utf8lite_render_string(r, "integer");
		break;

	case CORPUS_DATATYPE_REAL:
		utf8lite_render_string(r, "real");
		break;

	case CORPUS_DATATYPE_TEXT:
		utf8lite_render_string(r, "text");
		break;

	case CORPUS_DATATYPE_ARRAY:
		utf8lite_render_char(r, '[');
		corpus_render_datatype(r, s, t->meta.array.type_id);
		if (t->meta.array.length >= 0) {
			utf8lite_render_printf(r, "; %d", t->meta.array.length);
		}
		utf8lite_render_char(r, ']');
		break;

	case CORPUS_DATATYPE_RECORD:
		utf8lite_render_char(r, '{');
		utf8lite_render_indent(r, +1);

		n = t->meta.record.nfield;
		for (i = 0; i < n; i++) {
			if (i > 0) {
				utf8lite_render_string(r, ",");
			}
			utf8lite_render_newlines(r, 1);

			name_id = t->meta.record.name_ids[i];
			name = &s->names.types[name_id].text;
			utf8lite_render_char(r, '"');
			utf8lite_render_text(r, name);
			utf8lite_render_string(r, "\": ");

			type_id = t->meta.record.type_ids[i];
			corpus_render_datatype(r, s, type_id);
		}

		utf8lite_render_indent(r, -1);
		utf8lite_render_newlines(r, 1);
		utf8lite_render_char(r, '}');
		break;

	default:
		corpus_log(CORPUS_ERROR_INTERNAL,
			   "internal error: invalid datatype kind");
	}

	utf8lite_render_set_flags(r, flags);
}


int corpus_write_datatype(FILE *stream, const struct corpus_schema *s, int id)
{
	struct utf8lite_render r;
	int err;

	if ((err = utf8lite_render_init(&r,
					UTF8LITE_ESCAPE_CONTROL
					| UTF8LITE_ESCAPE_UTF8
					| UTF8LITE_ENCODE_JSON))) {
		goto error_init;
	}

	corpus_render_datatype(&r, s, id);
	if (r.error) {
		goto error_render;
	}

	if (fwrite(r.string, 1, (size_t)r.length, stream) < (size_t)r.length
			&& r.length) {
		err = CORPUS_ERROR_OS;
		corpus_log(err, "failed writing to output stream: %s",
			   strerror(errno));
		goto error_fwrite;
	}

	err = 0;

error_fwrite:
error_render:
	utf8lite_render_destroy(&r);
error_init:
	if (err) {
		corpus_log(err, "failed writing datatype to output stream");
	}

	return err;
}



int corpus_schema_scan(struct corpus_schema *s, const uint8_t *ptr,
		       size_t size, int *idptr)
{
	struct utf8lite_text text;
	const uint8_t *input = ptr;
	const uint8_t *end = ptr + size;
	uint_fast8_t ch;
	int err, id;

	// skip over leading whitespace
	scan_spaces(&ptr, end);

	// treat the empty string as null
	if (ptr == end) {
		id = CORPUS_DATATYPE_NULL;
		goto success;
	}

	ch = *ptr++;
	switch (ch) {
	case 'n':
		if ((err = scan_null(&ptr, end))) {
			goto error;
		}
		id = CORPUS_DATATYPE_NULL;
		break;

	case 'f':
		if ((err = scan_false(&ptr, end))) {
			goto error;
		}
		id = CORPUS_DATATYPE_BOOLEAN;
		break;

	case 't':
		if ((err = scan_true(&ptr, end))) {
			goto error;
		}
		id = CORPUS_DATATYPE_BOOLEAN;
		break;

	case '"':
		if ((err = scan_text(&ptr, end, &text))) {
			goto error;
		}
		id = CORPUS_DATATYPE_TEXT;
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
		ptr--;
		if ((err = scan_numeric(&ptr, end, &id))) {
			goto error;
		}
		break;
	}

	scan_spaces(&ptr, end);
	if (ptr != end) {
		err = CORPUS_ERROR_INVAL;
		corpus_log(err, "unexpected trailing characters");
		goto error;
	}

success:
	err = 0;
	goto out;

error:
	corpus_log(err, "failed parsing value (%.*s)", (unsigned)size, input);
	id = CORPUS_DATATYPE_ANY;
	goto out;

out:
	if (idptr) {
		*idptr = id;
	}
	return err;
}




int scan_value(struct corpus_schema *s, const uint8_t **bufptr,
	       const uint8_t *end, int *idptr)
{
	struct utf8lite_text text;
	const uint8_t *ptr = *bufptr;
	uint_fast8_t ch;
	int err, id;

	ch = *ptr++;
	switch (ch) {
	case 'n':
		if ((err = scan_null(&ptr, end))) {
			goto error;
		}
		id = CORPUS_DATATYPE_NULL;
		break;

	case 'f':
		if ((err = scan_false(&ptr, end))) {
			goto error;
		}
		id = CORPUS_DATATYPE_BOOLEAN;
		break;

	case 't':
		if ((err = scan_true(&ptr, end))) {
			goto error;
		}
		id = CORPUS_DATATYPE_BOOLEAN;
		break;

	case '"':
		if ((err = scan_text(&ptr, end, &text))) {
			goto error;
		}
		id = CORPUS_DATATYPE_TEXT;
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
		ptr--;
		if ((err = scan_numeric(&ptr, end, &id))) {
			goto error;
		}
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


int scan_array(struct corpus_schema *s, const uint8_t **bufptr,
	       const uint8_t *end, int *idptr)
{
	const uint8_t *ptr = *bufptr;
	int id, cur_id, next_id;
	int length;
	int err;

	length = 0;
	cur_id = CORPUS_DATATYPE_NULL;

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

			if ((err = corpus_schema_union(s, cur_id, next_id,
							&cur_id))) {
				goto error;
			}

			if (length == INT_MAX) {
				goto error_inval_length;
			}

			length++;
			break;
		default:
			goto error_inval_nocomma;
		}
	}
close:
	ptr++;
	err = corpus_schema_array(s, cur_id, length, &id);
	goto out;

error_inval_length:
	err = CORPUS_ERROR_INVAL;
	corpus_log(err, "array length exceeds maximum (%d)", INT_MAX);
	goto error;

error_inval_noclose:
	err = CORPUS_ERROR_INVAL;
	corpus_log(err, "no closing bracket (]) at end of array");
	goto error;

error_inval_noval:
	err = CORPUS_ERROR_INVAL;
	corpus_log(err, "missing value at index %d in array", length);
	goto error;

error_inval_val:
	err = CORPUS_ERROR_INVAL;
	corpus_log(err, "failed parsing value at index %d in array", length);
	goto error;

error_inval_nocomma:
	err = CORPUS_ERROR_INVAL;
	corpus_log(err, "missing comma (,) after index %d in array",
	       length);
	goto error;

error:
	id = -1;

out:
	*bufptr = ptr;
	*idptr = id;
	return err;
}


int scan_record(struct corpus_schema *s, const uint8_t **bufptr,
		const uint8_t *end, int *idptr)
{
	const uint8_t *ptr = *bufptr;
	int name_id, type_id, id;
	int fstart;
	int nfield;
	int err;

	nfield = 0;
	fstart = s->buffer.nfield;

	scan_spaces(&ptr, end);
	if (ptr == end) {
		goto error_inval_noclose;
	}

	// handle empty record
	if (*ptr == '}') {
		goto close;
	}

	if ((err = scan_field(s, &ptr, end, &name_id, &type_id))) {
		goto error;
	}

	if (s->buffer.nfield == s->buffer.nfield_max) {
		if ((err = corpus_schema_buffer_grow(&s->buffer, 1))) {
			goto error;
		}
	}

	s->buffer.nfield++;
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

			if ((err = scan_field(s, &ptr, end, &name_id,
					      &type_id))) {
				goto error;
			}

			if (nfield == INT_MAX) {
				goto error_inval_nfield;
			}

			if (s->buffer.nfield == s->buffer.nfield_max) {
				err = corpus_schema_buffer_grow(&s->buffer,
								1);
				if (err) {
					goto error;
				}
			}

			s->buffer.nfield++;
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

	err = corpus_schema_record(s, s->buffer.type_ids + fstart,
			    s->buffer.name_ids + fstart, nfield, &id);
	goto out;

error_inval_nfield:
	err = CORPUS_ERROR_INVAL;
	corpus_log(err, "number of record fields exceeds maximum (%d)",
		   INT_MAX);
	goto error;

error_inval_noclose:
	err = CORPUS_ERROR_INVAL;
	corpus_log(err, "no closing bracket (}) at end of record");
	goto error;

error_inval_nocomma:
	err = CORPUS_ERROR_INVAL;
	corpus_log(err, "missing comma (,) in record");
	goto error;

error:
	id = -1;

out:
	s->buffer.nfield = fstart;
	*bufptr = ptr;
	*idptr = id;
	return err;
}


int scan_field(struct corpus_schema *s, const uint8_t **bufptr,
	       const uint8_t *end, int *name_idptr, int *type_idptr)
{
	struct utf8lite_text name;
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
	if ((err = corpus_schema_name(s, &name, &name_id))) {
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
	err = CORPUS_ERROR_INVAL;
	corpus_log(err, "missing field name in record");
	goto error;

error_inval_nocolon:
	err = CORPUS_ERROR_INVAL;
	corpus_log(err, "missing colon after field name \"%.*s\" in record",
		   (unsigned)UTF8LITE_TEXT_SIZE(&name), name.ptr);
	goto error;

error_inval_noval:
	err = CORPUS_ERROR_INVAL;
	corpus_log(err, "missing value for field \"%.*s\" in record",
		   (unsigned)UTF8LITE_TEXT_SIZE(&name), name.ptr);
	goto error;

error_inval_val:
	err = CORPUS_ERROR_INVAL;
	corpus_log(err, "failed parsing value for field \"%.*s\" in record",
		   (unsigned)UTF8LITE_TEXT_SIZE(&name), name.ptr);
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


int scan_numeric(const uint8_t **bufptr, const uint8_t *end, int *idptr)
{
	const uint8_t *ptr = *bufptr;
	uint_fast8_t ch;
	int id;
	int err;

	id = CORPUS_DATATYPE_INTEGER;
	ch = *ptr++;

	// skip over optional sign
	if (ch == '-' || ch == '+') {
		if (ptr == end) {
			err = CORPUS_ERROR_INVAL;
			corpus_log(err, "missing number after (%c) sign",
				   (char)ch);
			goto error_inval;
		}
		ch = *ptr++;
	}

	// parse integer part, Infinity, or NaN
	switch (ch) {
	case '0':
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

	case '.':
		ptr--;
		break;

	case 'I':
		id = CORPUS_DATATYPE_REAL;
		if ((err = scan_infinity(&ptr, end))) {
			goto error_inval;
		}
		break;

	case 'N':
		id = CORPUS_DATATYPE_REAL;
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
		id = CORPUS_DATATYPE_REAL;
		ptr++;
		scan_digits(&ptr, end);
	}

	if (ptr == end) {
		err = 0;
		goto out;
	}

	// look ahead for optional exponent
	if (*ptr == 'e' || *ptr == 'E') {
		id = CORPUS_DATATYPE_REAL;
		ptr++;
		if (ptr == end) {
			goto error_inval_exp;
		}
		ch = *ptr++;

		// optional sign
		if (ch == '-' || ch == '+') {
			if (ptr == end) {
				goto error_inval_exp;
			}
			ch = *ptr++;
		}

		// exponent
		if (!isdigit(ch)) {
			goto error_inval_char;
		}
		scan_digits(&ptr, end);
	}

	err = 0;
	goto out;

error_inval_start:
	err = CORPUS_ERROR_INVAL;
	if (isprint(ch)) {
		corpus_log(err, "invalid character (%c) at start of value",
			   (char)ch);
	} else {
		corpus_log(err, "invalid character (0x%02x) at start of value",
			   (unsigned char)ch);
	}
	goto error_inval;

error_inval_char:
	err = CORPUS_ERROR_INVAL;
	if (isprint(ch)) {
		corpus_log(err, "invalid character (%c) in number",
			   (char)ch);
	} else {
		corpus_log(err, "invalid character (0x%02x) in number",
			   (unsigned char)ch);
	}
	goto error_inval;

error_inval_exp:
	err = CORPUS_ERROR_INVAL;
	corpus_log(err, "missing exponent at end of number");
	goto error_inval;

error_inval:
	//corpus_log(err, "failed attempting to parse numeric value");

out:
	*bufptr = ptr;
	*idptr = id;
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
	      struct utf8lite_text *text)
{
	struct utf8lite_message msg;
	const uint8_t *input = *bufptr;
	const uint8_t *ptr = input;
	uint_fast8_t ch;
	int err, flags;

	flags = 0;
	while (ptr != end) {
		ch = *ptr;
		if (ch == '"') {
			goto close;
		} else if (ch == '\\') {
			flags = UTF8LITE_TEXT_UNESCAPE;
			if (ptr == end) {
				goto error_noclose;
			}
			ptr++; // skip over next character
		}
		ptr++;
	}

error_noclose:
	err = CORPUS_ERROR_INVAL;
	corpus_log(err, "no trailing quote (\") at end of text value");
	goto out;

close:
	if ((err = utf8lite_text_assign(text, input, (size_t)(ptr - input),
					flags, &msg))) {
		err = CORPUS_ERROR_INVAL;
		corpus_log(err, "invalid JSON string: %s", msg.string);
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
 * strict JSON, which only allows ' ', '\\t', '\\r', and '\\n'.
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
	int err;

	if (ptr == end) {
		err = CORPUS_ERROR_INVAL;
		corpus_log(err, "expecting '%c' but input ended", c);
		return err;
	}

	ch = *ptr;
	if (ch != c) {
		err = CORPUS_ERROR_INVAL;
		if (isprint(ch)) {
			corpus_log(err, "expecting '%c' but got '%c'", c,
				   (char)ch);
		} else {
			corpus_log(err, "expecting '%c' but got '0x%02x'", c,
				   (unsigned char)ch);
		}
		return err;
	}

	*bufptr = ptr + 1;
	return 0;
}


int array_equals(const struct corpus_datatype_array *t1,
		 const struct corpus_datatype_array *t2)
{
	int eq = 0;

	if (t1->type_id != t2->type_id) {
		eq = 0;
	} else if (t1->length != t2->length) {
		eq = 0;
	} else {
		eq = 1;
	}

	return eq;
}


int record_equals(const struct corpus_datatype_record *t1,
		  const struct corpus_datatype_record *t2)
{
	int n = t1->nfield;
	int eq = 0;

	if (t1->nfield != t2->nfield) {
		eq = 0;
	} else if (n == 0) {
		eq = 1;
	} else if (memcmp(t1->type_ids, t2->type_ids,
			  (size_t)n * sizeof(*t1->type_ids))) {
		eq = 0;
	} else if (memcmp(t1->name_ids, t2->name_ids,
			  (size_t)n * sizeof(*t1->name_ids))) {
		eq = 0;
	} else {
		eq = 1;
	}

	return eq;
}


unsigned array_hash(const struct corpus_datatype_array *t)
{
	unsigned hash = 0;

	hash = hash_combine(hash, (unsigned)t->type_id);
	hash = hash_combine(hash, (unsigned)t->length);

	return hash;
}


unsigned record_hash(const struct corpus_datatype_record *t)
{
	int i;
	unsigned hash = 0;

	for (i = 0; i < t->nfield; i++) {
		hash = hash_combine(hash, (unsigned)t->name_ids[i]);
		hash = hash_combine(hash, (unsigned)t->type_ids[i]);
	}

	return hash;
}

/* adapted from boost/functional/hash/hash.hpp (Boost License, Version 1.0) */
unsigned hash_combine(unsigned seed, unsigned hash)
{
	seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	return seed;
}
