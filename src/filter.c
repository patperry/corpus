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
#include "stem.h"
#include "typemap.h"
#include "symtab.h"
#include "wordscan.h"
#include "filter.h"


#define CHECK_ERROR(value) \
	do { \
		if (f->error) { \
			corpus_log(CORPUS_ERROR_INVAL, "an error occurred" \
				   " during a prior filter operation"); \
			return (value); \
		} \
	} while (0)

static int corpus_filter_advance_word(struct corpus_filter *f, int *idptr);
static int corpus_filter_try_combine(struct corpus_filter *f, int *idptr);

static int corpus_filter_add_type(struct corpus_filter *f,
				  const struct corpus_text *type, int *idptr);
static int corpus_filter_grow_types(struct corpus_filter *f, int size0,
				    int size);
static int corpus_filter_get_drop(const struct corpus_filter *f,
				  const struct corpus_text *type, int kind);
static int corpus_type_kind(const struct corpus_text *type);


int corpus_filter_init(struct corpus_filter *f, int flags, int type_kind,
		       corpus_stem_func stemmer, void *context)
{
	int err;

	if ((err = corpus_symtab_init(&f->symtab, type_kind, stemmer,
				      context))) {
		corpus_log(err, "failed initializing symbol table");
		goto error_symtab;
	}

	if ((err = corpus_tree_init(&f->combine))) {
		corpus_log(err, "failed initializing combination tree");
		goto error_combine;
	}

	f->combine_rules = NULL;
	f->drop = NULL;
	f->flags = flags;
	f->has_scan = 0;
	f->scan_type = 0;
	f->current.ptr = NULL;
	f->current.attr = 0;
	f->type_id = CORPUS_FILTER_NONE;
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
	corpus_free(f->drop);
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


int corpus_filter_combine(struct corpus_filter *f,
			  const struct corpus_text *type)
{
	struct corpus_wordscan scan;
	struct corpus_text scan_current;
	int *rules;
	int err, has_scan, symbol_id, node_id, nnode0, nnode, parent_id,
	    scan_type_id, size0, size, id = CORPUS_FILTER_NONE;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	// save the state of the current scan
	has_scan = f->has_scan;
	if (has_scan) {
		f->has_scan = 0;
		scan = f->scan;
		scan_current = f->current;
		scan_type_id = f->type_id;
	} else {
		memset(&scan, 0, sizeof(scan)); // not used; silence warning
		memset(&scan_current, 0, sizeof(scan_current)); // ditto
		scan_type_id = CORPUS_FILTER_NONE;
	}

	// add a new type for the combined type
	if ((err = corpus_filter_add_type(f, type, &id))) {
		goto out;
	}

	// iterate over all non-ignored words in the type
	if ((err = corpus_filter_start(f, type, CORPUS_FILTER_SCAN_TYPES))) {
		goto out;
	}

	node_id = CORPUS_TREE_NONE;
	symbol_id = CORPUS_FILTER_NONE;

	while (corpus_filter_advance_word(f, &symbol_id)) {
		if (symbol_id == CORPUS_FILTER_NONE) {
			continue;
		}

		parent_id = node_id;
		nnode0 = f->combine.nnode;
		size0 = f->combine.nnode_max;
		if ((err = corpus_tree_add(&f->combine, parent_id, symbol_id,
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
			f->combine_rules[node_id] = CORPUS_FILTER_NONE;
		}
	}

	if (f->error) {
		err = f->error;
		goto out;
	}

	if (node_id >= 0) {
		f->combine_rules[node_id] = id;
	}

	err = 0;

out:
	// restore the old scan if one existed
	if (has_scan) {
		f->scan = scan;
		f->current = scan_current;
		f->type_id = scan_type_id;
	}
	f->has_scan = has_scan;

	if (err) {
		corpus_log(err, "failed adding combination rule to filter");
		f->error = err;
	}

	return err;
}


int corpus_filter_drop(struct corpus_filter *f,
		       const struct corpus_text *type)
{
	int err, type_id;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	if ((err = corpus_filter_add_type(f, type, &type_id))) {
		goto out;
	}

	if (type_id >= 0) {
		f->drop[type_id] = 1;
	}

	err = 0;
out:
	if (err) {
		corpus_log(err, "failed adding type to drop list");
		f->error = err;
	}

	return err;
}


int corpus_filter_drop_except(struct corpus_filter *f,
			      const struct corpus_text *type)
{
	int err, type_id;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	if ((err = corpus_filter_add_type(f, type, &type_id))) {
		goto out;
	}

	if (type_id >= 0) {
		f->drop[type_id] = 0;
	}

	err = 0;
out:
	if (err) {
		corpus_log(err, "failed adding type to drop exception list");
		f->error = err;
	}

	return err;
}


int corpus_filter_start(struct corpus_filter *f,
			const struct corpus_text *text, int type)
{
	CHECK_ERROR(CORPUS_ERROR_INVAL);

	corpus_wordscan_make(&f->scan, text);
	f->has_scan = 1;
	f->scan_type = type;
	f->current.ptr = text->ptr;
	f->current.attr = 0;
	f->type_id = CORPUS_FILTER_NONE;
	return 0;
}


int corpus_filter_advance(struct corpus_filter *f)
{
	int type_id = CORPUS_FILTER_NONE;
	int err, ret;

	ret = corpus_filter_advance_word(f, &type_id);
	f->current = f->scan.current;
	err = f->error;

	if (!ret || err) {
		goto out;
	}

	if ((err = corpus_filter_try_combine(f, &type_id))) {
		goto out;
	}

	if (f->drop[type_id]) {
		type_id = CORPUS_FILTER_NONE;
	}
	err = 0;

out:
	if (err) {
		f->error = err;
		type_id = CORPUS_FILTER_NONE;
	}

	f->type_id = type_id;

	return ret;
}


int corpus_filter_try_combine(struct corpus_filter *f, int *idptr)
{
	struct corpus_wordscan scan;
	struct corpus_text current;
	size_t attr, size;
	int err, has_scan, id, type_id, node_id, parent_id;

	if (!f->combine.nnode) {
		return 0;
	}

	id = *idptr;
	if (id == CORPUS_FILTER_NONE) {
		return 0;
	}

	parent_id = CORPUS_TREE_NONE;
	if (!corpus_tree_has(&f->combine, parent_id, id, &node_id)) {
		return 0;
	}

	// save the state of the current scan
	has_scan = f->has_scan;
	scan = f->scan;
	current = f->current;

	// check for a length-1 combine rule
	if (f->combine_rules[node_id] >= 0) {
		id = f->combine_rules[node_id];
	}

	size = CORPUS_TEXT_SIZE(&current);
	attr = CORPUS_TEXT_BITS(&current);
	type_id = CORPUS_FILTER_NONE;

	while (corpus_filter_advance_word(f, &type_id)) {
		size += CORPUS_TEXT_SIZE(&f->scan.current);
		attr |= CORPUS_TEXT_BITS(&f->scan.current);

		if (type_id == CORPUS_FILTER_NONE) {
			continue;
		}

		parent_id = node_id;
		if (!corpus_tree_has(&f->combine, parent_id, type_id,
				     &node_id)) {
			// no more potential matches
			err = 0;
			goto out;
		}

		// found a longer match
		if (f->combine_rules[node_id] >= 0) {
			has_scan = f->has_scan;
			scan = f->scan;
			current.attr = size | attr;
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
	f->has_scan = has_scan;
	f->scan = scan;
	f->current = current;

	if (err) {
		corpus_log(err, "failed trying filter combination rule");
		f->error = err;
		id = CORPUS_FILTER_NONE;
	}

	if (idptr) {
		*idptr = id;
	}

	return err;
}


int corpus_filter_advance_word(struct corpus_filter *f, int *idptr)
{
	const struct corpus_text *token, *type;
	int err, kind, token_id, n0, n, size0, size, type_id, drop, ret;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	type_id = CORPUS_FILTER_NONE;
	ret = 0;
	err = 0;

	if (!f->has_scan) {
		goto out;
	}

	if (!corpus_wordscan_advance(&f->scan)) {
		f->has_scan = 0;
		goto out;
	}

	if (f->scan.type == CORPUS_WORD_NONE) {
		type_id = CORPUS_FILTER_NONE;
		ret = 1;
		goto out;
	}

	token = &f->scan.current;
	n0 = f->symtab.ntype;
	size0 = f->symtab.ntype_max;

	if (f->scan_type == CORPUS_FILTER_SCAN_TOKENS) {
		// add the token
		if ((err = corpus_symtab_add_token(&f->symtab, token,
						   &token_id))) {
			goto out;
		}
		type_id = f->symtab.tokens[token_id].type_id;
	} else {
		// add the type
		if ((err = corpus_symtab_add_type(&f->symtab, token,
						  &type_id))) {
			goto out;
		}
	}
	n = f->symtab.ntype;
	size = f->symtab.ntype_max;

	// grow the type id array if necessary
	if (size0 < size) {
		if ((err = corpus_filter_grow_types(f, size0, size))) {
			goto out;
		}
	}

	// new types got added
	while (n0 < n) {
		type = &f->symtab.types[n0].text;
		kind = corpus_type_kind(type);
		drop = corpus_filter_get_drop(f, type, kind);
		f->drop[n0] = drop;
		n0++;
	}

	ret = 1;
	err = 0;
out:
	if (err) {
		corpus_log(err, "failed advancing text filter");
		f->error = err;
		type_id = CORPUS_FILTER_NONE;
		ret = 0;
	}

	if (idptr) {
		*idptr = type_id;
	}

	return ret;
}


int corpus_filter_add_type(struct corpus_filter *f,
			   const struct corpus_text *type, int *idptr)
{
	int err, id, kind, nsym0, nsym, size0, size;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	nsym0 = f->symtab.ntype;
	size0 = f->symtab.ntype_max;
	if ((err = corpus_symtab_add_type(&f->symtab, type, &id))) {
		goto out;
	}
	nsym = f->symtab.ntype;

	// a new type got added
	if (nsym0 != nsym) {
		size = f->symtab.ntype_max;
		if (size0 < size) {
			if ((err = corpus_filter_grow_types(f, size0, size))) {
				goto out;
			}
		}

		kind = corpus_type_kind(type);
		f->drop[id] = corpus_filter_get_drop(f, type, kind);
	}

	err = 0;
out:
	if (err) {
		corpus_log(err, "failed adding symbol to filter");
		f->error = err;
		id = CORPUS_FILTER_NONE;
	}

	if (idptr) {
		*idptr = id;
	}

	return err;
}


int corpus_filter_grow_types(struct corpus_filter *f, int size0, int size)
{
	int *drop;
	int err;

	if (!(drop = corpus_realloc(f->drop, size * sizeof(*drop)))) {
		err = CORPUS_ERROR_NOMEM;
		goto out;
	}
	f->drop = drop;
	err = 0;

out:
	if (err) {
		corpus_log(err, "failed growing filter type array");
		f->error = err;
	}

	(void)size0;
	return err;
}


int corpus_filter_get_drop(const struct corpus_filter *f,
			   const struct corpus_text *type, int kind)
{
	int drop;

	if (kind < 0) {
		kind = corpus_type_kind(type);
	}

	drop = 0;

	switch (kind) {
	case CORPUS_WORD_LETTER:
		drop = f->flags & CORPUS_FILTER_DROP_LETTER;
		break;

	case CORPUS_WORD_NUMBER:
		drop = f->flags & CORPUS_FILTER_DROP_NUMBER;
		break;

	case CORPUS_WORD_PUNCT:
		drop = f->flags & CORPUS_FILTER_DROP_PUNCT;
		break;

	case CORPUS_WORD_SYMBOL:
		drop = f->flags & CORPUS_FILTER_DROP_SYMBOL;
		break;

	default:
		drop = 1;
		break;
	}

	return drop;
}


int corpus_type_kind(const struct corpus_text *type)
{
	struct corpus_wordscan scan;
	int kind;

	corpus_wordscan_make(&scan, type);

	if (corpus_wordscan_advance(&scan)) {
		kind = scan.type;
	} else {
		kind = CORPUS_WORD_NONE;
	}

	return kind;
}
