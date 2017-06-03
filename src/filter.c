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

static int corpus_filter_add_symbol(struct corpus_filter *f,
				    const struct corpus_text *symbol,
				    int *idptr);
static int corpus_filter_set_type(struct corpus_filter *f,
				  const struct corpus_text *symbol,
				  int kind, int symbol_id, int *idptr);
static int corpus_filter_grow_types(struct corpus_filter *f, int nadd);
static int corpus_filter_grow_symbols(struct corpus_filter *f, int size0,
				      int size);
static int corpus_filter_symbol_prop(const struct corpus_filter *f,
				     const struct corpus_text *symbol,
				     int kind);
static int corpus_symbol_kind(const struct corpus_text *symbol);


int corpus_filter_init(struct corpus_filter *f, int symbol_kind,
		       const char *stemmer, int flags)
{
	int err;

	if ((err = corpus_symtab_init(&f->symtab, symbol_kind, stemmer))) {
		corpus_log(err, "failed initializing symbol table");
		goto error_symtab;
	}

	if ((err = corpus_tree_init(&f->combine))) {
		corpus_log(err, "failed initializing combination tree");
		goto error_combine;
	}
	f->combine_rules = NULL;
	f->type_ids = NULL;
	f->symbol_ids = NULL;
	f->ntype = 0;
	f->ntype_max = 0;
	f->flags = flags;
	f->has_select = 0;
	f->has_scan = 0;
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
	corpus_free(f->symbol_ids);
	corpus_free(f->type_ids);
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


const struct corpus_text *corpus_filter_type(const struct corpus_filter *f,
					     int id)
{
	int symbol_id;

	CHECK_ERROR(NULL);
	assert(0 <= id && id < f->ntype);

	symbol_id = f->symbol_ids[id];
	return &f->symtab.types[symbol_id].text;
}


int corpus_filter_combine(struct corpus_filter *f,
			  const struct corpus_text *type)
{
	struct corpus_wordscan scan;
	struct corpus_text scan_current;
	int *rules;
	int err, has_scan, symbol_id, node_id, nnode0, nnode, parent_id,
	    scan_type_id, size0, size, id = -1;

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

	// add a new symbol for the combined type
	if ((err = corpus_filter_add_symbol(f, type, &id))) {
		goto out;
	}

	// iterate over all non-ignored symbols in the type
	if ((err = corpus_filter_start(f, type))) {
		goto out;
	}

	node_id = CORPUS_TREE_NONE;

	while (corpus_filter_advance_raw(f, &symbol_id)) {
		if (f->type_ids[symbol_id] == CORPUS_FILTER_IGNORED) {
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
			f->combine_rules[node_id] = -1;
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
	int err, symbol_id, type_id, i, n;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	if ((err = corpus_filter_add_symbol(f, type, &symbol_id))) {
		goto out;
	}

	type_id = f->type_ids[symbol_id];

	switch (type_id) {
	case CORPUS_FILTER_IGNORED:
	case CORPUS_FILTER_DROPPED:
		break;

	case CORPUS_FILTER_EXCLUDED:
		f->type_ids[symbol_id] = CORPUS_FILTER_DROPPED;
		break;

	default:
		f->type_ids[symbol_id] = CORPUS_FILTER_DROPPED;

		// remove the existing type; update old type IDs
		n = f->ntype;
		for (i = type_id; i + 1 < n; i++) {
			f->symbol_ids[i] = f->symbol_ids[i + 1];
			f->type_ids[f->symbol_ids[i]] = i;
		}
		f->ntype--;

		break;
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
	int err, type_id, symbol_id;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	if ((err = corpus_filter_add_symbol(f, type, &symbol_id))) {
		goto out;
	}

	type_id = f->type_ids[symbol_id];

	if (type_id == CORPUS_FILTER_DROPPED) {
		if (f->has_select) {
			type_id = CORPUS_FILTER_EXCLUDED;
		} else {
			// add a new type
			if (f->ntype == f->ntype_max) {
				if ((err = corpus_filter_grow_types(f, 1))) {
					goto out;
				}
			}

			type_id = f->ntype;
			f->symbol_ids[type_id] = symbol_id;
			f->ntype++;
		}

		f->type_ids[symbol_id] = type_id;
	}

	err = 0;
out:
	if (err) {
		corpus_log(err, "failed adding type to drop exception list");
		f->error = err;
	}

	return err;
}



int corpus_filter_select(struct corpus_filter *f,
			 const struct corpus_text *type, int *idptr)
{
	int err, symbol_id, i, n, id = -1;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	if ((err = corpus_filter_add_symbol(f, type, &symbol_id))) {
		goto out;
	}

	if (!f->has_select) {
		n = f->symtab.ntype;

		for (i = 0; i < n; i++) {
			switch (f->type_ids[i]) {
			case CORPUS_FILTER_IGNORED:
			case CORPUS_FILTER_DROPPED:
				break;

			default:
				f->type_ids[i] = CORPUS_FILTER_EXCLUDED;
				break;
			}
		}

		f->has_select = 1;
		f->ntype = 0;
	}

	id = f->type_ids[symbol_id];

	// add the new type if it does not exist
	if (id == CORPUS_FILTER_EXCLUDED) {
		id = f->ntype;
		if (f->ntype == f->ntype_max) {
			if ((err = corpus_filter_grow_types(f, 1))) {
				goto out;
			}
		}
		f->symbol_ids[id] = symbol_id;
		f->type_ids[symbol_id] = id;
		f->ntype++;
	}

	err = 0;
out:
	if (err) {
		corpus_log(err, "failed adding type to select list");
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
	f->current.ptr = text->ptr;
	f->current.attr = 0;
	f->type_id = CORPUS_FILTER_NONE;
	return 0;
}


int corpus_filter_advance(struct corpus_filter *f)
{
	int symbol_id, id = CORPUS_FILTER_NONE;
	int err, ret;

	ret = corpus_filter_advance_raw(f, &symbol_id);
	f->current = f->scan.current;
	err = f->error;

	if (!ret || err) {
		goto out;
	}

	if ((err = corpus_filter_try_combine(f, &symbol_id))) {
		goto out;
	}

	id = f->type_ids[symbol_id];
	err = 0;

out:
	if (err) {
		f->error = err;
		id = CORPUS_FILTER_NONE;
	}

	f->type_id = id;

	return ret;
}


int corpus_filter_try_combine(struct corpus_filter *f, int *idptr)
{
	struct corpus_wordscan scan;
	struct corpus_text current;
	size_t attr, size;
	int err, id, symbol_id, type_id, node_id, parent_id;

	if (!f->combine.nnode) {
		return 0;
	}

	id = *idptr;
	type_id = f->type_ids[id];
	if (type_id == CORPUS_FILTER_IGNORED) {
		return 0;
	}

	parent_id = CORPUS_TREE_NONE;
	if (!corpus_tree_has(&f->combine, parent_id, id, &node_id)) {
		return 0;
	}

	// save the state of the current scan
	scan = f->scan;
	current = f->current;

	// check for a length-1 combine rule
	if (f->combine_rules[node_id] >= 0) {
		id = f->combine_rules[node_id];
	}

	size = CORPUS_TEXT_SIZE(&current);
	attr = CORPUS_TEXT_BITS(&current);

	while (corpus_filter_advance_raw(f, &symbol_id)) {
		size += CORPUS_TEXT_SIZE(&f->scan.current);
		attr |= CORPUS_TEXT_BITS(&f->scan.current);

		type_id = f->type_ids[symbol_id];
		if (type_id == CORPUS_FILTER_IGNORED) {
			continue;
		}

		parent_id = node_id;
		if (!corpus_tree_has(&f->combine, parent_id, symbol_id,
				     &node_id)) {
			// no more potential matches
			err = 0;
			goto out;
		}

		// found a longer match
		if (f->combine_rules[node_id] >= 0) {
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
	f->scan = scan;
	f->current = current;

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
	const struct corpus_text *token, *symbol;
	int err, token_id, id, nsym0, nsym, size0, size, symbol_id, ret;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	symbol_id = CORPUS_FILTER_NONE;
	ret = 0;
	err = 0;

	if (!f->has_scan) {
		goto out;
	}

	if (!corpus_wordscan_advance(&f->scan)) {
		f->has_scan = 0;
		goto out;
	}

	// add the token
	token = &f->scan.current;
	nsym0 = f->symtab.ntype;
	size0 = f->symtab.ntype_max;
	if ((err = corpus_symtab_add_token(&f->symtab, token, &token_id))) {
		goto out;
	}
	nsym = f->symtab.ntype;
	size = f->symtab.ntype_max;

	// grow the type id array if necessary
	if (size0 < size) {
		if ((err = corpus_filter_grow_symbols(f, size0, size))) {
			goto out;
		}
	}

	// get the symbol id
	symbol_id = f->symtab.tokens[token_id].type_id;

	// a new symbol got added
	if (nsym0 != nsym) {
		symbol = &f->symtab.types[symbol_id].text;
		if ((err = corpus_filter_set_type(f, symbol, f->scan.type,
						  symbol_id, &id))) {
			goto out;
		}
	}

	ret = 1;
	err = 0;
out:
	if (err) {
		corpus_log(err, "failed advancing text filter");
		f->error = err;
		symbol_id = CORPUS_FILTER_NONE;
		ret = 0;
	}

	if (idptr) {
		*idptr = symbol_id;
	}

	return ret;
}


int corpus_filter_add_symbol(struct corpus_filter *f,
			     const struct corpus_text *symbol, int *idptr)
{
	int err, id, nsym0, nsym, size0, size;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	nsym0 = f->symtab.ntype;
	size0 = f->symtab.ntype_max;
	if ((err = corpus_symtab_add_type(&f->symtab, symbol, &id))) {
		goto out;
	}
	nsym = f->symtab.ntype;

	// a new symbol got added
	if (nsym0 != nsym) {
		size = f->symtab.ntype_max;
		if (size0 < size) {
			if ((err = corpus_filter_grow_symbols(f, size0,
							      size))) {
				goto out;
			}
		}

		corpus_filter_set_type(f, symbol, -1, id, NULL);
	}

	err = 0;
out:
	if (err) {
		corpus_log(err, "failed adding symbol to filter");
		f->error = err;
		id = -1;
	}

	if (idptr) {
		*idptr = id;
	}

	return err;
}


int corpus_filter_set_type(struct corpus_filter *f,
			   const struct corpus_text *symbol,
			   int kind, int symbol_id,
			   int *idptr)
{
	int err, prop, id = -1;

	prop = corpus_filter_symbol_prop(f, symbol, kind);

	if (prop) {
		id = prop;
	} else if (f->has_select) {
		id = CORPUS_FILTER_EXCLUDED;
	} else {
		// a new type got added
		if (f->ntype == f->ntype_max) {
			if ((err = corpus_filter_grow_types(f, 1))) {
				goto out;
			}
		}
		id = f->ntype;
		f->symbol_ids[id] = symbol_id;
		f->ntype++;
	}

	f->type_ids[symbol_id] = id;
	err = 0;

out:
	if (err) {
		corpus_log(err, "failed setting type ID for symbol");
		id = -1;
	}

	if (idptr) {
		*idptr = id;
	}

	return err;
}


int corpus_filter_grow_types(struct corpus_filter *f, int nadd)
{
	void *base = f->symbol_ids;
	int size = f->ntype_max;
	int err;

	if ((err = corpus_array_grow(&base, &size, sizeof(*f->symbol_ids),
				     f->ntype, nadd))) {
		corpus_log(err, "failed allocating type array");
		f->error = err;
		return err;
	}

	f->symbol_ids = base;
	f->ntype_max = size;
	return 0;
}


int corpus_filter_grow_symbols(struct corpus_filter *f, int size0, int size)
{
	int *ids;
	int err;

	if (!(ids = corpus_realloc(f->type_ids, size * sizeof(*ids)))) {
		err = CORPUS_ERROR_NOMEM;
		goto out;
	}
	f->type_ids = ids;
	err = 0;

out:
	if (err) {
		corpus_log(err, "failed growing filter symbol id array");
		f->error = err;
	}

	(void)size0;
	return err;
}


int corpus_filter_symbol_prop(const struct corpus_filter *f,
			      const struct corpus_text *symbol, int kind)
{
	int drop, ignore, prop;

	if (kind < 0) {
		kind = corpus_symbol_kind(symbol);
	}

	ignore = 0;
	drop = 0;

	switch (kind) {
	case CORPUS_WORD_SPACE:
		ignore = (f->flags & CORPUS_FILTER_IGNORE_SPACE);
		break;

	case CORPUS_WORD_LETTER:
		drop = f->flags & CORPUS_FILTER_DROP_LETTER;
		break;

	case CORPUS_WORD_MARK:
		drop = (f->flags & CORPUS_FILTER_DROP_MARK);
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

	case CORPUS_WORD_OTHER:
		drop = f->flags & CORPUS_FILTER_DROP_OTHER;
		break;

	default:
		break;
	}

	if (ignore) {
		prop = CORPUS_FILTER_IGNORED;
	} else if (drop) {
		prop = CORPUS_FILTER_DROPPED;
	} else {
		prop = 0;
	}

	return prop;
}


int corpus_symbol_kind(const struct corpus_text *symbol)
{
	struct corpus_wordscan scan;
	int kind;

	corpus_wordscan_make(&scan, symbol);

	if (corpus_wordscan_advance(&scan)) {
		kind = scan.type;
	} else {
		kind = CORPUS_WORD_NONE;
	}

	return kind;
}
