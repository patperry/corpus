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
#include <stddef.h>
#include "error.h"
#include "memory.h"
#include "table.h"
#include "text.h"
#include "textset.h"
#include "typemap.h"
#include "symtab.h"
#include "wordscan.h"
#include "filter.h"

#define CORPUS_FILTER_IGNORED	-1
#define CORPUS_FILTER_DROPPED	-2
#define CORPUS_FILTER_EXCLUDED	-3

#define CHECK_ERROR(value) \
	do { \
		if (f->error) { \
			corpus_log(CORPUS_ERROR_INVAL, "an error occurred" \
				   " during a prior filter operation"); \
			return (value); \
		} \
	} while (0)


static int corpus_filter_add_type(struct corpus_filter *f,
				  const struct corpus_text *type,
				  int *idptr);
static int corpus_filter_grow_ids(struct corpus_filter *f, int size0,
				  int size);


int corpus_filter_init(struct corpus_filter *f, int type_kind,
		       const char *stemmer, int flags)
{
	int err;

	if ((err = corpus_symtab_init(&f->symtab, type_kind, stemmer))) {
		corpus_log(err, "failed initializing symbol table");
		goto error_symtab;
	}
	if ((err = corpus_textset_init(&f->select))) {
		corpus_log(err, "failed initializing term selection set");
		goto error_select;
	}

	f->ids = NULL;
	f->has_scan = 0;
	f->flags = flags;
	f->error = 0;
	return 0;

error_select:
	corpus_symtab_destroy(&f->symtab);
error_symtab:
	f->error = err;
	return err;
}


void corpus_filter_destroy(struct corpus_filter *f)
{
	corpus_free(f->ids);
	corpus_textset_destroy(&f->select);
	corpus_symtab_destroy(&f->symtab);
}


int corpus_filter_stem_except(struct corpus_filter *f,
			      const struct corpus_text *typ)
{
	int err;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	if ((err = corpus_symtab_stem_except(&f->symtab, typ))) {
		corpus_log(err, "failed adding stem exception to filter");
		f->error = err;
	}

	return err;
}


const struct corpus_text *corpus_filter_term(const struct corpus_filter *f,
					     int id)
{
	assert(id >= 0);

	CHECK_ERROR(NULL);

	if (f->select.nitem > 0) {
		assert(id < f->select.nitem);
		return &f->select.items[id];
	} else {
		assert(id < f->symtab.ntype);
		return &f->symtab.types[id].text;
	}
}


int corpus_filter_combine(struct corpus_filter *f,
			  const struct corpus_text *term,
			  int *idptr)
{
	CHECK_ERROR(CORPUS_ERROR_INVAL);

	(void)term;
	(void)idptr;
	return 0;
}


int corpus_filter_drop(struct corpus_filter *f,
		       const struct corpus_text *term)
{
	int err, type_id;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	if ((err = corpus_filter_add_type(f, term, &type_id))) {
		corpus_log(err, "failed adding term to drop list");
		f->error = err;
		goto out;
	}
	f->ids[type_id] = CORPUS_FILTER_DROPPED;
	err = 0;
out:
	return err;
}


int corpus_filter_select(struct corpus_filter *f,
			 const struct corpus_text *term, int *idptr)
{
	int empty, err, id, type_id, i, n;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	empty = f->select.nitem == 0;
	if ((err = corpus_textset_add(&f->select, term, &id))) {
		goto out;
	}

	if (empty) {
		n = f->symtab.ntype_max;
		for (i = 0; i < n; i++) {
			switch (f->ids[i]) {
			case CORPUS_FILTER_IGNORED:
			case CORPUS_FILTER_DROPPED:
				break;
			default:
				f->ids[i] = CORPUS_FILTER_EXCLUDED;
			}
		}
	}

	if (corpus_symtab_has_type(&f->symtab, term, &type_id)) {
		if (f->ids[type_id] == CORPUS_FILTER_EXCLUDED) {
			f->ids[type_id] = id;
		}
	}

	err = 0;
out:
	if (err) {
		corpus_log(err, "failed adding term to select list");
		f->error = err;
		id = -1;
	}

	if (idptr) {
		*idptr = id;
	}

	return err;
}


int corpus_filter_start(struct corpus_filter *f,
			const struct corpus_text *text)
{
	CHECK_ERROR(CORPUS_ERROR_INVAL);

	corpus_wordscan_make(&f->scan, text);
	f->has_scan = 1;
	return 0;
}


int corpus_filter_advance(struct corpus_filter *f, int *idptr)
{
	const struct corpus_text *token, *type;
	int err, token_id, type_id, size0, size, drop, ignore, id = -1;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	if (!f->has_scan) {
		return 0;
	}

ignored:

	if (!corpus_wordscan_advance(&f->scan)) {
		f->has_scan = 0;
		return 0;
	}

	// add the token
	token = &f->scan.current;
	size0 = f->symtab.ntype_max;
	if ((err = corpus_symtab_add_token(&f->symtab, token, &token_id))) {
		goto out;
	}
	size = f->symtab.ntype_max;

	// grow the term id array if necessary
	if (size0 < size) {
		if ((err = corpus_filter_grow_ids(f, size0, size))) {
			goto out;
		}
	}

	// get the type id
	type_id = f->symtab.tokens[token_id].type_id;

	// a new type got added
	if (type_id + 1 == f->symtab.ntype) {
		type = &f->symtab.types[type_id].text;

		// check whether to ignore or drop the new type
		if (CORPUS_TEXT_SIZE(type) == 0
				&& (f->flags & CORPUS_FILTER_IGNORE_EMPTY)) {
			ignore = 1;
			drop = 0;
		} else {
			ignore = 0;

			switch (f->scan.type) {
			case CORPUS_WORD_NUMBER:
				drop = f->flags & CORPUS_FILTER_DROP_NUMBER;
				break;

			case CORPUS_WORD_LETTER:
				drop = f->flags & CORPUS_FILTER_DROP_LETTER;
				break;

			case CORPUS_WORD_KANA:
				drop = f->flags & CORPUS_FILTER_DROP_KANA;
				break;

			case CORPUS_WORD_IDEO:
				drop = f->flags & CORPUS_FILTER_DROP_IDEO;
				break;

			default:
				drop = f->flags & CORPUS_FILTER_DROP_SYMBOL;
				break;
			}
		}

		// set the new term id
		if (ignore) {
			id = CORPUS_FILTER_IGNORED;
		} else if (drop) {
			id = CORPUS_FILTER_DROPPED;
		} else if (f->select.nitem) {
			if (!corpus_textset_has(&f->select, type, &id)) {
				id = CORPUS_FILTER_EXCLUDED;
			}
		} else {
			id = type_id;
		}

		f->ids[type_id] = id;
	} else {
		id = f->ids[type_id];
	}

	if (id == CORPUS_FILTER_IGNORED) {
		goto ignored;
	}

	err = 0;
out:
	if (err) {
		corpus_log(err, "failed advancing text filter");
		f->error = err;
		id = -1;
	}
	if (idptr) {
		*idptr = id;
	}
	return err ? 0 : 1;
}


int corpus_filter_add_type(struct corpus_filter *f,
			   const struct corpus_text *type, int *idptr)
{
	int err, id, size0, size;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	size0 = f->symtab.ntype_max;
	if ((err = corpus_symtab_add_type(&f->symtab, type, &id))) {
		goto out;
	}
	size = f->symtab.ntype_max;

	if (size0 < size) {
		corpus_filter_grow_ids(f, size0, size);
	}

	err = 0;
out:
	if (err) {
		corpus_log(err, "failed adding type to filter");
		f->error = err;
		id = -1;
	}

	if (idptr) {
		*idptr = id;
	}

	return err;
}


int corpus_filter_grow_ids(struct corpus_filter *f, int size0, int size)
{
	int *ids;
	int err, i;

	if (!(ids = corpus_realloc(f->ids, size * sizeof(*ids)))) {
		err = CORPUS_ERROR_NOMEM;
		goto out;
	}
	f->ids = ids;

	for (i = size0; i < size; i++) {
		if (f->select.nitem > 0) {
			f->ids[i] = CORPUS_FILTER_EXCLUDED;
		} else {
			f->ids[i] = i;
		}
	}
	err = 0;

out:
	if (err) {
		corpus_log(err, "failed growing filter type id array");
		f->error = err;
	}

	return err;
}
