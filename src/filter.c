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
#include <string.h>
#include "array.h"
#include "error.h"
#include "memory.h"
#include "table.h"
#include "text.h"
#include "textset.h"
#include "tree.h"
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

static int corpus_filter_advance_raw(struct corpus_filter *f, int *idptr);
static int corpus_filter_try_combine(struct corpus_filter *f, int *idptr);

static int corpus_filter_add_type(struct corpus_filter *f,
				  const struct corpus_text *type,
				  int *idptr);
static int corpus_filter_set_term(struct corpus_filter *f,
				  const struct corpus_text *term,
				  int term_type, int type_id, int *idptr);
static int corpus_filter_grow_terms(struct corpus_filter *f, int nadd);
static int corpus_filter_grow_types(struct corpus_filter *f, int size0,
				    int size);
static int corpus_filter_term_prop(const struct corpus_filter *f,
				   const struct corpus_text *term,
				   int term_type);
static int corpus_term_type(const struct corpus_text *term);


int corpus_filter_init(struct corpus_filter *f, int type_kind,
		       const char *stemmer, int flags)
{
	int err;

	if ((err = corpus_symtab_init(&f->symtab, type_kind, stemmer))) {
		corpus_log(err, "failed initializing symbol table");
		goto error_symtab;
	}

	if ((err = corpus_tree_init(&f->combine))) {
		corpus_log(err, "failed initializing combination tree");
		goto error_combine;
	}
	f->combine_rules = NULL;
	f->term_ids = NULL;
	f->type_ids = NULL;
	f->nterm = 0;
	f->nterm_max = 0;
	f->flags = flags;
	f->has_select = 0;
	f->has_scan = 0;
	f->error = 0;
	return 0;

error_combine:
	corpus_symtab_destroy(&f->symtab);

error_symtab:
	f->error = err;
	return err;
}


void corpus_filter_destroy(struct corpus_filter *f)
{
	corpus_free(f->type_ids);
	corpus_free(f->term_ids);
	corpus_free(f->combine_rules);
	corpus_tree_destroy(&f->combine);
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
	int type_id;

	CHECK_ERROR(NULL);
	assert(0 <= id && id < f->nterm);

	type_id = f->type_ids[id];
	return &f->symtab.types[type_id].text;
}


int corpus_filter_combine(struct corpus_filter *f,
			  const struct corpus_text *term)
{
	struct corpus_wordscan scan;
	int *rules;
	int err, has_scan, type_id, node_id, nnode0, nnode, parent_id,
	    size0, size, id = -1;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	// save the state of the current scan
	has_scan = f->has_scan;
	if (has_scan) {
		f->has_scan = 0;
		scan = f->scan;
	} else {
		memset(&scan, 0, sizeof(scan)); // not used; silence warning
	}

	// root the combination tree
	if (f->combine.nnode == 0) {
		if ((err = corpus_tree_root(&f->combine))) {
			goto out;
		}

		size = f->combine.nnode_max;
		assert(size > 0);

		rules = corpus_malloc(size * sizeof(*rules));
		if (!rules) {
			err = CORPUS_ERROR_NOMEM;
			goto out;
		}
		rules[0] = -1;
		f->combine_rules = rules;
	}

	// add a new type for the combined term
	if ((err = corpus_filter_add_type(f, term, &id))) {
		goto out;
	}

	node_id = 0;

	// iterate over all non-ignored types in the rule
	if ((err = corpus_filter_start(f, term))) {
		goto out;
	}

	while (corpus_filter_advance_raw(f, &type_id)) {
		if (f->term_ids[type_id] == CORPUS_FILTER_IGNORED) {
			continue;
		}

		parent_id = node_id;
		nnode0 = f->combine.nnode;
		size0 = f->combine.nnode_max;
		if ((err = corpus_tree_add(&f->combine, parent_id, type_id,
					   &node_id))) {
			goto out;
		}
		nnode = f->combine.nnode;

		// check whether a new node got added
		if (nnode0 < nnode) {
			// expand the rules array if necessary
			size = f->combine.nnode_max;
			if (size0 < size) {
				rules = f->combine_rules;
				rules = corpus_realloc(rules,
						       size * sizeof(*rules));
				if (!rules) {
					err = CORPUS_ERROR_NOMEM;
					goto out;
				}
				f->combine_rules = rules;
			}

			// set the new rule
			f->combine_rules[node_id] = -1;
		}
	}

	if (f->error) {
		err = f->error;
		goto out;
	}

	if (node_id > 0) {
		f->combine_rules[node_id] = id;
	}

	err = 0;

out:
	// restore the old scan if one existed
	if (has_scan) {
		f->scan = scan;
	}
	f->has_scan = has_scan;

	if (err) {
		corpus_log(err, "failed adding combination rule to filter");
		f->error = err;
	}

	return err;
}


int corpus_filter_drop(struct corpus_filter *f,
		       const struct corpus_text *term)
{
	int err, type_id, term_id, i, n;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	if ((err = corpus_filter_add_type(f, term, &type_id))) {
		goto out;
	}

	term_id = f->term_ids[type_id];

	switch (term_id) {
	case CORPUS_FILTER_IGNORED:
	case CORPUS_FILTER_DROPPED:
		break;

	case CORPUS_FILTER_EXCLUDED:
		f->term_ids[type_id] = CORPUS_FILTER_DROPPED;
		break;

	default:
		f->term_ids[type_id] = CORPUS_FILTER_DROPPED;

		// remove the existing term; update old term IDs
		n = f->nterm;
		for (i = term_id; i + 1 < n; i++) {
			f->type_ids[i] = f->type_ids[i + 1];
			f->term_ids[f->type_ids[i]] = i;
		}
		f->nterm--;

		break;
	}

	err = 0;
out:
	if (err) {
		corpus_log(err, "failed adding term to drop list");
		f->error = err;
	}

	return err;
}


int corpus_filter_drop_except(struct corpus_filter *f,
			      const struct corpus_text *term)
{
	int err, term_id, type_id;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	if ((err = corpus_filter_add_type(f, term, &type_id))) {
		goto out;
	}

	term_id = f->term_ids[type_id];

	if (term_id == CORPUS_FILTER_DROPPED) {
		if (f->has_select) {
			term_id = CORPUS_FILTER_EXCLUDED;
		} else {
			// add a new term
			if (f->nterm == f->nterm_max) {
				if ((err = corpus_filter_grow_terms(f, 1))) {
					goto out;
				}
			}

			term_id = f->nterm;
			f->type_ids[term_id] = type_id;
			f->nterm++;
		}

		f->term_ids[type_id] = term_id;
	}

	err = 0;
out:
	if (err) {
		corpus_log(err, "failed adding term to drop exception list");
		f->error = err;
	}

	return err;
}



int corpus_filter_select(struct corpus_filter *f,
			 const struct corpus_text *term, int *idptr)
{
	int err, type_id, i, n, id = -1;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	if ((err = corpus_filter_add_type(f, term, &type_id))) {
		goto out;
	}

	if (!f->has_select) {
		n = f->symtab.ntype;

		for (i = 0; i < n; i++) {
			switch (f->term_ids[i]) {
			case CORPUS_FILTER_IGNORED:
			case CORPUS_FILTER_DROPPED:
				break;

			default:
				f->term_ids[i] = CORPUS_FILTER_EXCLUDED;
				break;
			}
		}

		f->has_select = 1;
		f->nterm = 0;
	}

	id = f->term_ids[type_id];

	// add the new term if it does not exist
	if (id == CORPUS_FILTER_EXCLUDED) {
		id = f->nterm;
		if (f->nterm == f->nterm_max) {
			if ((err = corpus_filter_grow_terms(f, 1))) {
				goto out;
			}
		}
		f->type_ids[id] = type_id;
		f->term_ids[type_id] = id;
		f->nterm++;
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
	int type_id, id = -1;
	int err, ret;

	ret = corpus_filter_advance_raw(f, &type_id);
	err = f->error;

	if (!ret || err) {
		goto out;
	}

	if ((err = corpus_filter_try_combine(f, &type_id))) {
		goto out;
	}

	id = f->term_ids[type_id];
	err = 0;

out:
	if (err) {
		f->error = err;
		id = -1;
	}

	if (idptr) {
		*idptr = id;
	}

	return ret;
}


int corpus_filter_try_combine(struct corpus_filter *f, int *idptr)
{
	struct corpus_wordscan scan;
	int err, has, id, type_id, node_id, parent_id;

	if (!f->combine.nnode) {
		return 0;
	}

	parent_id = 0;
	id = *idptr;
	if (!corpus_tree_has(&f->combine, parent_id, id, &node_id)) {
		return 0;
	}

	// save the state of the current scan
	scan = f->scan;

	if (f->combine_rules[node_id] >= 0) {
		has = 1;
		id = f->combine_rules[node_id];
	} else {
		has = 0;
	}

	while (corpus_filter_advance_raw(f, &type_id)) {
		parent_id = node_id;
		if (!corpus_tree_has(&f->combine, parent_id, type_id,
				     &node_id)) {
			// no more potential matches
			err = 0;
			goto out;
		}

		// found a longer match
		if (f->combine_rules[node_id] >= 0) {
			scan = f->scan;
			has = 1;
			id = f->combine_rules[node_id];
		}
	}

	if (f->error) {
		err = f->error;
		goto out;
	}

	err = 0;

out:
	// restore the state of the scan after the longest match
	f->scan = scan;

	if (err) {
		corpus_log(err, "failed trying filter combination rule");
		f->error = err;
		id = -1;
	}

	if (idptr) {
		*idptr = id;
	}

	return err;
}


int corpus_filter_advance_raw(struct corpus_filter *f, int *idptr)
{
	const struct corpus_text *token, *type;
	int err, token_id, id, ntype0, ntype, size0, size, type_id = -1;

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
	ntype0 = f->symtab.ntype;
	size0 = f->symtab.ntype_max;
	if ((err = corpus_symtab_add_token(&f->symtab, token, &token_id))) {
		goto out;
	}
	ntype = f->symtab.ntype;
	size = f->symtab.ntype_max;

	// grow the term id array if necessary
	if (size0 < size) {
		if ((err = corpus_filter_grow_types(f, size0, size))) {
			goto out;
		}
	}

	// get the type id
	type_id = f->symtab.tokens[token_id].type_id;

	// a new type got added
	if (ntype0 != ntype) {
		type = &f->symtab.types[type_id].text;
		if ((err = corpus_filter_set_term(f, type, f->scan.type,
						  type_id, &id))) {
			goto out;
		}
	} else {
		id = f->term_ids[type_id];
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
		*idptr = type_id;
	}
	return err ? 0 : 1;
}


int corpus_filter_add_type(struct corpus_filter *f,
			   const struct corpus_text *type, int *idptr)
{
	int err, id, ntype0, ntype, size0, size;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	ntype0 = f->symtab.ntype;
	size0 = f->symtab.ntype_max;
	if ((err = corpus_symtab_add_type(&f->symtab, type, &id))) {
		goto out;
	}
	ntype = f->symtab.ntype;

	// a new type got added
	if (ntype0 != ntype) {
		size = f->symtab.ntype_max;
		if (size0 < size) {
			if ((err = corpus_filter_grow_types(f, size0,
							    size))) {
				goto out;
			}
		}

		corpus_filter_set_term(f, type, -1, id, NULL);
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


int corpus_filter_set_term(struct corpus_filter *f,
			   const struct corpus_text *term,
			   int term_type, int type_id,
			   int *idptr)
{
	int err, prop, id = -1;

	prop = corpus_filter_term_prop(f, term, term_type);

	if (prop) {
		id = prop;
	} else if (f->has_select) {
		id = CORPUS_FILTER_EXCLUDED;
	} else {
		// a new term got added
		if (f->nterm == f->nterm_max) {
			if ((err = corpus_filter_grow_terms(f, 1))) {
				goto out;
			}
		}
		id = f->nterm;
		f->type_ids[id] = type_id;
		f->nterm++;
	}

	f->term_ids[type_id] = id;
	err = 0;

out:
	if (err) {
		corpus_log(err, "failed setting term ID for type");
		id = -1;
	}

	if (idptr) {
		*idptr = id;
	}

	return err;
}


int corpus_filter_grow_terms(struct corpus_filter *f, int nadd)
{
	void *base = f->type_ids;
	int size = f->nterm_max;
	int err;

	if ((err = corpus_array_grow(&base, &size, sizeof(*f->type_ids),
				     f->nterm, nadd))) {
		corpus_log(err, "failed allocating term array");
		f->error = err;
		return err;
	}

	f->type_ids = base;
	f->nterm_max = size;
	return 0;
}


int corpus_filter_grow_types(struct corpus_filter *f, int size0, int size)
{
	int *ids;
	int err;

	if (!(ids = corpus_realloc(f->term_ids, size * sizeof(*ids)))) {
		err = CORPUS_ERROR_NOMEM;
		goto out;
	}
	f->term_ids = ids;
	err = 0;

out:
	if (err) {
		corpus_log(err, "failed growing filter type id array");
		f->error = err;
	}

	(void)size0;
	return err;
}


int corpus_filter_term_prop(const struct corpus_filter *f,
			    const struct corpus_text *term, int term_type)
{
	int drop, prop;

	prop = 0;

	if (CORPUS_TEXT_SIZE(term) == 0
			&& (f->flags & CORPUS_FILTER_IGNORE_EMPTY)) {
		prop = CORPUS_FILTER_IGNORED;
	} else {
		if (term_type < 0) {
			term_type = corpus_term_type(term);
		}

		switch (term_type) {
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

		if (drop) {
			prop = CORPUS_FILTER_DROPPED;
		}
	}

	return prop;
}


int corpus_term_type(const struct corpus_text *term)
{
	struct corpus_wordscan scan;
	int type;

	corpus_wordscan_make(&scan, term);

	if (corpus_wordscan_advance(&scan)) {
		type = scan.type;
	} else {
		type = CORPUS_WORD_NONE;
	}

	return type;
}
