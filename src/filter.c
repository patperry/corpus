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
#include "../lib/utf8lite/src/utf8lite.h"
#include "array.h"
#include "error.h"
#include "memory.h"
#include "table.h"
#include "textset.h"
#include "tree.h"
#include "stem.h"
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


struct corpus_filter_state {
	int has_scan;
	struct corpus_wordscan scan;
	struct utf8lite_text current;
	int type_id;
};


static void corpus_filter_state_push(struct corpus_filter *f,
				     struct corpus_filter_state *state);
static void corpus_filter_state_pop(struct corpus_filter *f,
				    struct corpus_filter_state *state);

static int corpus_filter_advance_word(struct corpus_filter *f, int *idptr);
static int corpus_filter_try_combine(struct corpus_filter *f, int *idptr);
static int corpus_filter_stem(struct corpus_filter *f, int *idptr);
static int corpus_filter_unspace(struct corpus_filter *f, int *idptr);

static int corpus_filter_grow_types(struct corpus_filter *f, int size);
static int corpus_filter_get_drop(const struct corpus_filter *f, int kind);
static int corpus_type_kind(const struct utf8lite_text *type);


int corpus_filter_init(struct corpus_filter *f, int flags, int type_kind,
		       int32_t connector, corpus_stem_func stemmer,
		       void *context)
{
	int err;

	if ((err = corpus_symtab_init(&f->symtab, type_kind))) {
		corpus_log(err, "failed initializing symbol table");
		goto error_symtab;
	}

	if ((err = utf8lite_render_init(&f->render, UTF8LITE_ESCAPE_NONE))) {
		corpus_log(err, "failed initializing type renderer");
		goto error_render;
	}

	if ((err = corpus_tree_init(&f->combine))) {
		corpus_log(err, "failed initializing combination tree");
		goto error_combine;
	}

	f->has_stemmer = 0;
	if (stemmer) {
		if ((err = corpus_stem_init(&f->stemmer, stemmer, context))) {
			corpus_log(err, "failed initializing stemmer");
			goto error_stemmer;
		}
		f->has_stemmer = 1;
	}

	f->combine_rules = NULL;
	f->props = NULL;
	f->flags = flags;
	f->connector = connector;
	f->has_scan = 0;
	f->current.ptr = NULL;
	f->current.attr = 0;
	f->type_id = CORPUS_TYPE_NONE;
	f->error = 0;

	return 0;

error_stemmer:
	corpus_tree_destroy(&f->combine);
error_combine:
	utf8lite_render_destroy(&f->render);
error_render:
	corpus_symtab_destroy(&f->symtab);
error_symtab:
	f->error = err;
	return err;
}


void corpus_filter_destroy(struct corpus_filter *f)
{
	corpus_free(f->props);
	corpus_free(f->combine_rules);
	if (f->has_stemmer) {
		corpus_stem_destroy(&f->stemmer);
	}
	corpus_tree_destroy(&f->combine);
	utf8lite_render_destroy(&f->render);
	corpus_symtab_destroy(&f->symtab);
}


int corpus_filter_stem_except(struct corpus_filter *f,
			      const struct utf8lite_text *typ)
{
	int err;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	if (!f->has_stemmer) {
		return 0;
	}

	if ((err = corpus_stem_except(&f->stemmer, typ))) {
		corpus_log(err, "failed adding stem exception to filter");
		f->error = err;
	}

	return err;
}


int corpus_filter_combine(struct corpus_filter *f,
			  const struct utf8lite_text *tokens)
{
	struct corpus_filter_state state;
	struct utf8lite_text rule;
	int *rules;
	int err, word_id, next_id, node_id, nnode0, nnode, parent_id,
	    size0, size, has_space, type_id = CORPUS_TYPE_NONE;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	corpus_filter_state_push(f, &state);

	// iterate over all non-ignored words in the type
	if ((err = corpus_filter_start(f, tokens))) {
		goto out;
	}

	word_id = CORPUS_TYPE_NONE;

	// find the first word in the pattern
	while (corpus_filter_advance_word(f, &word_id)) {
		if (word_id != CORPUS_TYPE_NONE) {
			break;
		}
	}

	// if error or no first word in the pattern, do nothing
	if ((err = f->error) || word_id == CORPUS_TYPE_NONE) {
		goto out;
	}

	next_id = CORPUS_TYPE_NONE;
	has_space = 0;

	node_id = CORPUS_TREE_NONE;
	nnode0 = f->combine.nnode;
	size0 = f->combine.nnode_max;

	// find the next word
	while (corpus_filter_advance_word(f, &next_id)) {
		if (next_id == CORPUS_TYPE_NONE) {
			has_space = 1;
			continue;
		}

		// add the first word if haven't already
		if (node_id == CORPUS_TREE_NONE) {
			utf8lite_render_text(&f->render,
					     &f->symtab.types[word_id].text);
			parent_id = node_id;
			if ((err = corpus_tree_add(&f->combine, parent_id,
						   word_id, &node_id))) {
				goto out;
			}
		}

		// add the space
		if (has_space) {
			utf8lite_render_char(&f->render, ' ');
			parent_id = node_id;
			if ((err = corpus_tree_add(&f->combine, parent_id,
						   CORPUS_TYPE_NONE,
						   &node_id))) {
				goto out;
			}
			has_space = 0;
		}

		// add the next word
		utf8lite_render_text(&f->render,
				     &f->symtab.types[next_id].text);
		parent_id = node_id;
		if ((err = corpus_tree_add(&f->combine, parent_id, next_id,
					   &node_id))) {
			goto out;
		}
	}

	// check for errors
	if ((err = f->error) || (err = f->render.error)) {
		goto out;
	}

	// expand the rules array if necessary
	size = f->combine.nnode_max;
	if (size0 < size) {
		rules = f->combine_rules;
		rules = corpus_realloc(rules, (size_t)size * sizeof(*rules));
		if (!rules) {
			err = CORPUS_ERROR_NOMEM;
			goto out;
		}
		f->combine_rules = rules;
	}

	// add the new nodes
	nnode = f->combine.nnode;
	while (nnode0 < nnode) {
		f->combine_rules[nnode0] = CORPUS_TYPE_NONE;
		nnode0++;
	}

	if (node_id != CORPUS_TREE_NONE) {
		// add a new type for the combined type
		utf8lite_text_assign(&rule, (const uint8_t *)f->render.string,
				     (size_t)f->render.length,
				     UTF8LITE_TEXT_VALID, NULL);
		if ((err = corpus_filter_add_type(f, &rule, &type_id))) {
			goto out;
		}
		utf8lite_render_clear(&f->render);
		f->combine_rules[node_id] = type_id;
	}

out:
	// restore the old scan if one existed
	corpus_filter_state_pop(f, &state);

	if (err) {
		corpus_log(err, "failed adding combination rule to filter");
		f->error = err;
	}

	return err;
}


int corpus_filter_drop(struct corpus_filter *f,
		       const struct utf8lite_text *type)
{
	int err, type_id;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	if ((err = corpus_filter_add_type(f, type, &type_id))) {
		goto out;
	}

	if (type_id >= 0) {
		f->props[type_id].drop = 1;
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
			      const struct utf8lite_text *type)
{
	int err, type_id;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	if ((err = corpus_filter_add_type(f, type, &type_id))) {
		goto out;
	}

	if (type_id >= 0) {
		f->props[type_id].drop = 0;
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
			const struct utf8lite_text *text)
{
	CHECK_ERROR(CORPUS_ERROR_INVAL);

	corpus_wordscan_make(&f->scan, text);
	f->has_scan = 1;
	f->current.ptr = text->ptr;
	f->current.attr = 0;
	f->type_id = CORPUS_TYPE_NONE;
	return 0;
}


int corpus_filter_advance(struct corpus_filter *f)
{
	int type_id = CORPUS_TYPE_NONE;
	int err, ret;

	ret = corpus_filter_advance_word(f, &type_id);
	f->current = f->scan.current;
	err = f->error;

	if (!ret || err || type_id < 0) {
		goto out;
	}

	if ((err = corpus_filter_try_combine(f, &type_id))) {
		goto out;
	}

	assert(type_id >= 0);

	if ((err = corpus_filter_stem(f, &type_id))) {
		goto out;
	}

	if (type_id < 0 || f->props[type_id].drop) {
		type_id = CORPUS_TYPE_NONE;
		goto out;
	}

	if ((err = corpus_filter_unspace(f, &type_id))) {
		goto out;
	}

out:
	if (err) {
		f->error = err;
		type_id = CORPUS_TYPE_NONE;
	}

	f->type_id = type_id;

	return ret;
}


int corpus_filter_try_combine(struct corpus_filter *f, int *idptr)
{
	struct corpus_wordscan scan;
	struct utf8lite_text current;
	size_t attr, size;
	int err, has_scan, id, type_id, node_id, parent_id, in_space;

	if (!f->combine.nnode) {
		return 0;
	}

	id = *idptr;
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

	size = UTF8LITE_TEXT_SIZE(&current);
	attr = UTF8LITE_TEXT_BITS(&current);
	type_id = CORPUS_TYPE_NONE;
	in_space = 0;

	while (corpus_filter_advance_word(f, &type_id)) {
		size += UTF8LITE_TEXT_SIZE(&f->scan.current);
		attr |= UTF8LITE_TEXT_BITS(&f->scan.current);

		if (type_id == CORPUS_TYPE_NONE) {
			if (in_space) {
				continue;
			} else {
				in_space = 1;
			}
		} else {
			in_space = 0;
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
		id = CORPUS_TYPE_NONE;
	}

	if (idptr) {
		*idptr = id;
	}

	return err;
}


int corpus_filter_stem(struct corpus_filter *f, int *idptr)
{
	const struct utf8lite_text *tok;
	int err, id, has_stem, stem_id;

	if (!f->has_stemmer) {
		return 0;
	}

	id = *idptr;

	// used cached result
	if (f->props[id].has_stem) {
		*idptr = f->props[id].stem;
		return 0;
	}

	has_stem = 1;
	stem_id = CORPUS_TYPE_NONE;

	tok = &f->symtab.types[id].text;
	if ((err = corpus_stem_set(&f->stemmer, tok))) {
		goto out;
	}

	if (f->stemmer.has_type && UTF8LITE_TEXT_SIZE(&f->stemmer.type) > 0) {
		// add the type
		if ((err = corpus_filter_add_type(f, &f->stemmer.type,
						  &stem_id))) {
			goto out;
		}
	}

out:
	if (err) {
		stem_id = CORPUS_TYPE_NONE;
		has_stem = 0;
		corpus_log(err, "failed stemming token");
	}

	f->props[id].stem = stem_id;
	f->props[id].has_stem = has_stem;

	*idptr = stem_id;
	return err;
}


int corpus_filter_unspace(struct corpus_filter *f, int *idptr)
{
	const struct utf8lite_text *type;
	struct utf8lite_text_iter it;
	struct utf8lite_text unspace;
	int32_t ch;
	int err, id, in_space, needs_unspace, has_unspace, unspace_id;

	id = *idptr;

	// used cached result
	if (f->props[id].has_unspace) {
		*idptr = f->props[id].unspace;
		return 0;
	}

	has_unspace = 1;
	unspace_id = CORPUS_TYPE_NONE;
	type = &f->symtab.types[id].text;

	// see whether the type contains a space
	needs_unspace = 0;
	utf8lite_text_iter_make(&it, type);
	while (utf8lite_text_iter_advance(&it)) {
		ch = it.current;
		if (utf8lite_isspace(ch)) {
			needs_unspace = 1;
			break;
		}
	}

	if (!needs_unspace) {
		unspace_id = id;
		err = 0;
		goto out;
	}

	// replace sequences of spaces with connectors
	in_space = 0;
	utf8lite_text_iter_make(&it, type);

	while (utf8lite_text_iter_advance(&it)) {
		ch = it.current;
		if (utf8lite_isspace(ch)) {
			// only render one connector for a string of spaces
			if (!in_space) {
				utf8lite_render_char(&f->render, f->connector);
				in_space = 1;
			}
		} else {
			utf8lite_render_char(&f->render, ch);
			in_space = 0;
		}
	}

	if ((err = f->render.error)) {
		goto out;
	}

	unspace.ptr = (uint8_t *)f->render.string;
	unspace.attr = (size_t)f->render.length;

	// add the unspaced type
	if ((err = corpus_filter_add_type(f, &unspace, &unspace_id))) {
		goto out;
	}

	utf8lite_render_clear(&f->render);

out:
	if (err) {
		unspace_id = CORPUS_TYPE_NONE;
		has_unspace = 0;
		corpus_log(err, "failed removing spaces from type");
	}

	f->props[id].unspace = unspace_id;
	f->props[id].has_unspace = has_unspace;

	*idptr = unspace_id;
	return err;
}


int corpus_filter_advance_word(struct corpus_filter *f, int *idptr)
{
	const struct utf8lite_text *token, *type;
	int err, kind, token_id, n0, n, size0, size, type_id, drop, ret;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	type_id = CORPUS_TYPE_NONE;
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
		type_id = CORPUS_TYPE_NONE;
		ret = 1;
		goto out;
	}

	token = &f->scan.current;
	n0 = f->symtab.ntype;
	size0 = f->symtab.ntype_max;

	// add the token
	if ((err = corpus_symtab_add_token(&f->symtab, token, &token_id))) {
		goto out;
	}
	type_id = f->symtab.tokens[token_id].type_id;

	n = f->symtab.ntype;
	size = f->symtab.ntype_max;

	// grow the type id array if necessary
	if (size0 < size) {
		if ((err = corpus_filter_grow_types(f, size))) {
			goto out;
		}
	}

	// new types got added
	while (n0 < n) {
		type = &f->symtab.types[n0].text;
		kind = corpus_type_kind(type);
		drop = corpus_filter_get_drop(f, kind);
		f->props[n0].drop = drop;
		f->props[n0].has_stem = 0;
		f->props[n0].has_unspace = 0;
		n0++;
	}

	ret = 1;
	err = 0;
out:
	if (err) {
		corpus_log(err, "failed advancing text filter");
		f->error = err;
		type_id = CORPUS_TYPE_NONE;
		ret = 0;
	}

	if (idptr) {
		*idptr = type_id;
	}

	return ret;
}


int corpus_filter_add_type(struct corpus_filter *f,
			   const struct utf8lite_text *type, int *idptr)
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
			if ((err = corpus_filter_grow_types(f, size))) {
				goto out;
			}
		}

		kind = corpus_type_kind(type);
		f->props[id].drop = corpus_filter_get_drop(f, kind);
		f->props[id].has_stem = 0;
		f->props[id].has_unspace = 0;
	}

	err = 0;
out:
	if (err) {
		corpus_log(err, "failed adding type to filter");
		f->error = err;
		id = CORPUS_TYPE_NONE;
	}

	if (idptr) {
		*idptr = id;
	}

	return err;
}


int corpus_filter_grow_types(struct corpus_filter *f, int size)
{
	struct corpus_filter_prop *props;
	int err;

	if (!(props = corpus_realloc(f->props,
					(size_t)size * sizeof(*props)))) {
		err = CORPUS_ERROR_NOMEM;
		goto out;
	}
	f->props = props;
	err = 0;

out:
	if (err) {
		corpus_log(err, "failed growing filter type property array");
		f->error = err;
	}

	return err;
}


int corpus_filter_get_drop(const struct corpus_filter *f, int kind)
{
	int drop;

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


int corpus_type_kind(const struct utf8lite_text *type)
{
	struct corpus_wordscan scan;
	int kind;

	corpus_wordscan_make(&scan, type);

	kind = CORPUS_WORD_NONE;
	while (corpus_wordscan_advance(&scan)) {
		kind = scan.type;
		if (kind != CORPUS_WORD_NONE) {
			break;
		}
	}

	return kind;
}


void corpus_filter_state_push(struct corpus_filter *f,
			      struct corpus_filter_state *state)
{
	if (f->has_scan) {
		state->has_scan = f->has_scan;
		state->scan = f->scan;
		state->current = f->current;
		state->type_id = f->type_id;
	} else {
		state->has_scan = 0;
	}
}


void corpus_filter_state_pop(struct corpus_filter *f,
			     struct corpus_filter_state *state)
{
	if (state->has_scan) {
		f->has_scan = state->has_scan;
		f->scan = state->scan;
		f->current = state->current;
		f->type_id = state->type_id;
	} else {
		f->has_scan = 0;
	}
}
