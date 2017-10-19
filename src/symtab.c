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
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "../lib/utf8lite/src/utf8lite.h"
#include "array.h"
#include "error.h"
#include "memory.h"
#include "table.h"
#include "textset.h"
#include "symtab.h"

static int corpus_symtab_grow_tokens(struct corpus_symtab *tab, int nadd);
static int corpus_symtab_grow_types(struct corpus_symtab *tab, int nadd);
static void corpus_symtab_rehash_tokens(struct corpus_symtab *tab);
static void corpus_symtab_rehash_types(struct corpus_symtab *tab);

static int type_add_token(struct corpus_symtab_type *type, int token_id);


int corpus_symtab_init(struct corpus_symtab *tab, int type_kind)
{
	int err;

	if ((err = utf8lite_textmap_init(&tab->typemap, type_kind))) {
		corpus_log(err, "failed initializing type buffer");
		goto error_typemap;
	}

	if ((err = corpus_table_init(&tab->type_table))) {
		corpus_log(err, "failed allocating type table");
		goto error_type_table;
	}

	if ((err = corpus_table_init(&tab->token_table))) {
		corpus_log(err, "failed allocating token table");
		goto error_token_table;
	}

	tab->types = NULL;
	tab->ntype_max = 0;
	tab->ntype = 0;

	tab->tokens = NULL;
	tab->ntoken_max = 0;
	tab->ntoken = 0;

	return 0;

error_token_table:
	corpus_table_destroy(&tab->type_table);
error_type_table:
	utf8lite_textmap_destroy(&tab->typemap);
error_typemap:
	corpus_log(err, "failed initializing symbol table");
	return err;
}


void corpus_symtab_destroy(struct corpus_symtab *tab)
{
	corpus_symtab_clear(tab);
	corpus_free(tab->tokens);
	corpus_free(tab->types);
	corpus_table_destroy(&tab->token_table);
	corpus_table_destroy(&tab->type_table);
	utf8lite_textmap_destroy(&tab->typemap);
}


void corpus_symtab_clear(struct corpus_symtab *tab)
{
	int ntoken = tab->ntoken;
	int ntype = tab->ntype;

	while (ntoken-- > 0) {
		utf8lite_text_destroy(&tab->tokens[ntoken].text);
	}
	tab->ntoken = 0;

	while (ntype-- > 0) {
		utf8lite_text_destroy(&tab->types[ntype].text);
		corpus_free(tab->types[ntype].token_ids);
	}
	tab->ntype = 0;

	corpus_table_clear(&tab->token_table);
	corpus_table_clear(&tab->type_table);
}


int corpus_symtab_has_token(const struct corpus_symtab *tab,
			    const struct utf8lite_text *tok, int *idptr)
{
	struct corpus_table_probe probe;
	unsigned hash = (unsigned)utf8lite_text_hash(tok);
	int token_id = CORPUS_TOKEN_NONE;
	bool found = false;

	corpus_table_probe_make(&probe, &tab->token_table, hash);
	while (corpus_table_probe_advance(&probe)) {
		token_id = probe.current;
		if (utf8lite_text_equals(tok, &tab->tokens[token_id].text)) {
			found = true;
			goto out;
		}
	}

out:
	if (idptr) {
		*idptr = found ? token_id : probe.index;
	}

	return found;
}


int corpus_symtab_has_type(const struct corpus_symtab *tab,
			   const struct utf8lite_text *typ, int *idptr)
{
	struct corpus_table_probe probe;
	unsigned hash = (unsigned)utf8lite_text_hash(typ);
	int type_id = CORPUS_TYPE_NONE;
	bool found = false;

	corpus_table_probe_make(&probe, &tab->type_table, hash);
	while (corpus_table_probe_advance(&probe)) {
		type_id = probe.current;
		if (utf8lite_text_equals(typ, &tab->types[type_id].text)) {
			found = true;
			goto out;
		}
	}

out:
	if (idptr) {
		*idptr = found ? type_id : probe.index;
	}

	return found;
}


int corpus_symtab_add_token(struct corpus_symtab *tab,
			    const struct utf8lite_text *tok, int *idptr)
{
	int pos, token_id, type_id;
	bool rehash = false;
	int err;

	if (corpus_symtab_has_token(tab, tok, &pos)) {
		token_id = pos;
		goto out;
	}

	token_id = tab->ntoken;

	// compute the type
	if ((err = utf8lite_textmap_set(&tab->typemap, tok))) {
		goto error;
	}

	// add the type
	if ((err = corpus_symtab_add_type(tab, &tab->typemap.text,
					  &type_id))) {
		goto error;
	}

	// grow the token array if necessary
	if (token_id == tab->ntoken_max) {
		if ((err = corpus_symtab_grow_tokens(tab, 1))) {
			goto error;
		}
	}

	// grow the token table if necessary
	if (token_id == tab->token_table.capacity) {
		if ((err = corpus_table_reinit(&tab->token_table,
					       token_id + 1))) {
			goto error;
		}
		rehash = true;
	}

	// allocate storage for the token
	if ((err = utf8lite_text_init_copy(&tab->tokens[token_id].text, tok))) {
		goto error;
	}

	// set the type
	tab->tokens[token_id].type_id = type_id;

	if (type_id >= 0) {
		// add the token to the type
		if ((err = type_add_token(&tab->types[type_id], token_id))) {
			utf8lite_text_destroy(&tab->tokens[token_id].text);
			goto error;
		}
	}

	// update the count
	tab->ntoken++;

	// set the bucket
	if (rehash) {
		corpus_symtab_rehash_tokens(tab);
	} else {
		tab->token_table.items[pos] = token_id;
	}

out:
	if (idptr) {
		*idptr = token_id;
	}

	return 0;

error:
	if (rehash) {
		corpus_symtab_rehash_tokens(tab);
	}
	corpus_log(err, "failed adding token to symbol table");
	return err;
}


int corpus_symtab_add_type(struct corpus_symtab *tab,
			   const struct utf8lite_text *typ, int *idptr)
{
	int pos, type_id;
	bool rehash = false;
	int err;

	if (corpus_symtab_has_type(tab, typ, &pos)) {
		type_id = pos;
	} else {
		type_id = tab->ntype;

		// grow the type array if necessary
		if (type_id == tab->ntype_max) {
			if ((err = corpus_symtab_grow_types(tab, 1))) {
				goto error;
			}
		}

		// grow the type table if necessary
		if (type_id == tab->type_table.capacity) {
			if ((err = corpus_table_reinit(&tab->type_table,
						       type_id + 1))) {
				goto error;
			}
			rehash = true;
		}

		// allocate storage for the type's text
		if ((err = utf8lite_text_init_copy(&tab->types[type_id].text,
						 typ))) {
			goto error;
		}

		// initialize type's the token array
		tab->types[type_id].token_ids = NULL;
		tab->types[type_id].ntoken = 0;

		// update the count
		tab->ntype++;

		// set the bucket
		if (rehash) {
			corpus_symtab_rehash_types(tab);
		} else {
			tab->type_table.items[pos] = type_id;
		}
	}

	if (idptr) {
		*idptr = type_id;
	}

	return 0;

error:
	if (rehash) {
		corpus_symtab_rehash_types(tab);
	}
	corpus_log(err, "failed adding type to symbol table");
	return err;
}


int corpus_symtab_grow_tokens(struct corpus_symtab *tab, int nadd)
{
	void *base = tab->tokens;
	int size = tab->ntoken_max;
	int err;

	if ((err = corpus_array_grow(&base, &size, sizeof(*tab->tokens),
				     tab->ntoken, nadd))) {
		corpus_log(err, "failed allocating token array");
		return err;
	}

	tab->tokens = base;
	tab->ntoken_max = size;
	return 0;
}


int corpus_symtab_grow_types(struct corpus_symtab *tab, int nadd)
{
	void *base = tab->types;
	int size = tab->ntype_max;
	int err;

	if ((err = corpus_array_grow(&base, &size, sizeof(*tab->types),
				     tab->ntype, nadd))) {
		corpus_log(err, "failed allocating type array");
		return err;
	}

	tab->types = base;
	tab->ntype_max = size;
	return 0;
}


void corpus_symtab_rehash_tokens(struct corpus_symtab *tab)
{
	const struct corpus_symtab_token *tokens = tab->tokens;
	struct corpus_table *token_table = &tab->token_table;
	int i, n = tab->ntoken;
	unsigned hash;

	corpus_table_clear(token_table);

	for (i = 0; i < n; i++) {
		hash = (unsigned)utf8lite_text_hash(&tokens[i].text);
		corpus_table_add(token_table, hash, i);
	}
}


void corpus_symtab_rehash_types(struct corpus_symtab *tab)
{
	const struct corpus_symtab_type *types = tab->types;
	struct corpus_table *type_table = &tab->type_table;
	int i, n = tab->ntype;
	unsigned hash;

	corpus_table_clear(type_table);

	for (i = 0; i < n; i++) {
		hash = (unsigned)utf8lite_text_hash(&types[i].text);
		corpus_table_add(type_table, hash, i);
	}
}


int type_add_token(struct corpus_symtab_type *typ, int tok_id)
{
	int *tok_ids = typ->token_ids;
	int ntok = typ->ntoken;

	tok_ids = corpus_realloc(tok_ids,
				 (size_t)(ntok + 1) * sizeof(*tok_ids));
	if (!tok_ids) {
		return CORPUS_ERROR_NOMEM;
	}

	tok_ids[ntok] = tok_id;
	typ->token_ids = tok_ids;
	typ->ntoken = ntok + 1;

	return 0;
}
