/*
 * Copyright 2016 Patrick O. Perry.
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

/*
 * The hash table implementation is based on
 *
 *       src/sparsehash/internal/densehashtable.h
 *
 * from the Google SparseHash library (BSD 3-Clause License):
 *
 *       https://github.com/sparsehash/sparsehash
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "array.h"
#include "errcode.h"
#include "table.h"
#include "text.h"
#include "token.h"
#include "xalloc.h"
#include "symtab.h"


static int symtab_grow_toks(struct symtab *tab, int nadd);
static int symtab_grow_typs(struct symtab *tab, int nadd);
static void symtab_rehash_toks(struct symtab *tab);
static void symtab_rehash_typs(struct symtab *tab);

static int typ_add_tok(struct symtab_typ *typ, int tok_id);


int symtab_init(struct symtab *tab, int typ_flags)
{
	int err;

	if ((err = typebuf_init(&tab->typebuf, typ_flags))) {
		syslog(LOG_ERR, "failed allocating type buffer");
		goto error_typebuf;
	}

	if ((err = table_init(&tab->typ_table))) {
		syslog(LOG_ERR, "failed allocating type table");
		goto error_typ_table;
	}

	if ((err = table_init(&tab->tok_table))) {
		syslog(LOG_ERR, "failed allocating token table");
		goto error_tok_table;
	}

	tab->typs = NULL;
	tab->ntyp_max = 0;
	tab->ntyp = 0;

	tab->toks = NULL;
	tab->ntok_max = 0;
	tab->ntok = 0;

	return 0;

error_tok_table:
	table_destroy(&tab->typ_table);
error_typ_table:
	typebuf_destroy(&tab->typebuf);
error_typebuf:
	syslog(LOG_ERR, "failed initializing symbol table");
	return ERROR_NOMEM;
}


void symtab_destroy(struct symtab *tab)
{
	symtab_clear(tab);
	free(tab->toks);
	free(tab->typs);
	table_destroy(&tab->tok_table);
	table_destroy(&tab->typ_table);
	typebuf_destroy(&tab->typebuf);
}


void symtab_clear(struct symtab *tab)
{
	int ntok = tab->ntok;
	int ntyp = tab->ntyp;

	while (ntok-- > 0) {
		text_destroy(&tab->toks[ntok].text);
	}
	tab->ntok = 0;

	while (ntyp-- > 0) {
		text_destroy(&tab->typs[ntyp].text);
		free(tab->typs[ntyp].tok_ids);
	}
	tab->ntyp = 0;

	table_clear(&tab->tok_table);
	table_clear(&tab->typ_table);
}


int symtab_has_tok(const struct symtab *tab, const struct text *tok, int *tok_idp)
{
	const int *table = tab->tok_table.items;
	unsigned mask = tab->tok_table.mask;
	uint64_t hash = tok_hash(tok);
	unsigned pos, nprobe;
	int tok_id;
	bool found;

	nprobe = 1;
	pos = ((unsigned)hash) & mask;

	while (1) {
		tok_id = table[pos];
		if (tok_id == TABLE_ITEM_NONE) {
			found = false;
			break;
		}

		if (tok_equals(tok, &tab->toks[tok_id].text)) {
			found = true;
			break;
		}

		nprobe++;
		pos = (pos + PROBE_JUMP(nprobe)) & mask;
	}

	if (tok_idp) {
		*tok_idp = found ? tok_id : (int)pos;
	}

	return found;
}


int symtab_has_typ(const struct symtab *tab, const struct text *typ, int *typ_idp)
{
	const int *items = tab->typ_table.items;
	unsigned mask = tab->typ_table.mask;
	uint64_t hash = tok_hash(typ);
	unsigned pos, nprobe;
	int typ_id;
	bool found;

	nprobe = 1;
	pos = ((unsigned)hash) & mask;

	while (1) {
		typ_id = items[pos];
		if (typ_id == TABLE_ITEM_NONE) {
			found = false;
			break;
		}

		if (tok_equals(typ, &tab->typs[typ_id].text)) {
			found = true;
			break;
		}

		nprobe++;
		pos = (pos + PROBE_JUMP(nprobe)) & mask;
	}

	if (typ_idp) {
		*typ_idp = found ? typ_id : (int)pos;
	}

	return found;
}


int symtab_add_tok(struct symtab *tab, const struct text *tok, int *tok_idp)
{
	int pos, tok_id, typ_id;
	bool rehash = false;
	int err;

	if (symtab_has_tok(tab, tok, &pos)) {
		tok_id = pos;
	} else {
		tok_id = tab->ntok;

		// compute the type
		if ((err = typebuf_set(&tab->typebuf, tok))) {
			goto error;
		}

		// add the type
		if ((err = symtab_add_typ(tab, &tab->typebuf.text, &typ_id))) {
			goto error;
		}

		// grow the token array if necessary
		if (tok_id == tab->ntok_max) {
			if ((err = symtab_grow_toks(tab, 1))) {
				goto error;
			}
		}

		// grow the token table if necessary
		if (tok_id == tab->tok_table.max) {
			if ((err = table_grow(&tab->tok_table, tok_id, 1))) {
				goto error;
			}
			rehash = true;
		}

		// allocate storage for the token
		if ((err = text_init_copy(&tab->toks[tok_id].text, tok))) {
			goto error;
		}

		// set the type
		tab->toks[tok_id].typ_id = typ_id;

		// add the token to the type
		if ((err = typ_add_tok(&tab->typs[typ_id], tok_id))) {
			text_destroy(&tab->toks[tok_id].text);
			goto error;
		}

		// update the count
		tab->ntok++;

		// set the bucket
		if (rehash) {
			symtab_rehash_toks(tab);
		} else {
			tab->tok_table.items[pos] = tok_id;
		}
	}

	if (tok_idp) {
		*tok_idp = tok_id;
	}

	return 0;

error:
	if (rehash) {
		symtab_rehash_toks(tab);
	}
	syslog(LOG_ERR, "failed adding token to symbol table");
	return err;
}


int symtab_add_typ(struct symtab *tab, const struct text *typ, int *typ_idp)
{
	int pos, typ_id;
	bool rehash = false;
	int err;

	if (symtab_has_typ(tab, typ, &pos)) {
		typ_id = pos;
	} else {
		typ_id = tab->ntyp;

		// grow the type array if necessary
		if (typ_id == tab->ntyp_max) {
			if ((err = symtab_grow_typs(tab, 1))) {
				goto error;
			}
		}

		// grow the type table if necessary
		if (typ_id == tab->typ_table.max) {
			if ((err = table_grow(&tab->typ_table, typ_id, 1))) {
				goto error;
			}
			rehash = true;
		}

		// allocate storage for the type's text
		if ((err = text_init_copy(&tab->typs[typ_id].text, typ))) {
			goto error;
		}

		// initialize type's the token array
		tab->typs[typ_id].tok_ids = NULL;
		tab->typs[typ_id].ntok = 0;

		// update the count
		tab->ntyp++;

		// set the bucket
		if (rehash) {
			symtab_rehash_typs(tab);
		} else {
			tab->typ_table.items[pos] = typ_id;
		}
	}

	if (typ_idp) {
		*typ_idp = typ_id;
	}

	return 0;

error:
	if (rehash) {
		symtab_rehash_typs(tab);
	}
	syslog(LOG_ERR, "failed adding type to symbol table");
	return err;
}


int symtab_grow_toks(struct symtab *tab, int nadd)
{
	void *base = tab->toks;
	int size = tab->ntok_max;
	int err;

	if ((err = array_grow(&base, &size, sizeof(*tab->toks),
			      tab->ntok, nadd))) {
		syslog(LOG_ERR, "failed allocating token array");
		return err;
	}

	if (tab->ntok > INT_MAX - nadd) {
		syslog(LOG_ERR, "token count exceeds maximum (%d)", INT_MAX);
		return ERROR_OVERFLOW;
	}

	if (size > INT_MAX) {
		size = INT_MAX;
	}

	tab->toks = base;
	tab->ntok_max = size;
	return 0;
}


int symtab_grow_typs(struct symtab *tab, int nadd)
{
	void *base = tab->typs;
	int size = tab->ntyp_max;
	int err;

	if ((err = array_grow(&base, &size, sizeof(*tab->typs),
			      tab->ntyp, nadd))) {
		syslog(LOG_ERR, "failed allocating type array");
		return err;
	}

	if (tab->ntyp > INT_MAX - nadd) {
		syslog(LOG_ERR, "type count exceeds maximum (%d)", INT_MAX);
		return ERROR_OVERFLOW;
	}

	if (size > INT_MAX) {
		size = INT_MAX;
	}

	tab->typs = base;
	tab->ntyp_max = size;
	return 0;
}


void symtab_rehash_toks(struct symtab *tab)
{
	const struct symtab_tok *toks = tab->toks;
	struct table *tok_table = &tab->tok_table;
	int pos, i, n = tab->ntok;
	uint64_t hash;

	table_clear(tok_table);

	for (i = 0; i < n; i++) {
		hash = tok_hash(&toks[i].text);
		pos = table_next_empty(tok_table, hash);
		tok_table->items[pos] = i;
	}
}


void symtab_rehash_typs(struct symtab *tab)
{
	const struct symtab_typ *typs = tab->typs;
	struct table *typ_table = &tab->typ_table;
	int pos, i, n = tab->ntyp;
	uint64_t hash;

	table_clear(typ_table);

	for (i = 0; i < n; i++) {
		hash = tok_hash(&typs[i].text);
		pos = table_next_empty(typ_table, hash);
		typ_table->items[pos] = i;
	}
}



int typ_add_tok(struct symtab_typ *typ, int tok_id)
{
	int *tok_ids = typ->tok_ids;
	int ntok = typ->ntok;

	if (!(tok_ids = xrealloc(tok_ids, (ntok + 1) * sizeof(*tok_ids)))) {
		return ERROR_NOMEM;
	}

	tok_ids[ntok] = tok_id;
	typ->tok_ids = tok_ids;
	typ->ntok = ntok + 1;

	return 0;
}
