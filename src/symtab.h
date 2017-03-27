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

#ifndef SYMTAB_H
#define SYMTAB_H

/**
 * \file symtab.h
 *
 * Symbol table.
 */

#include <stdint.h>

struct text;

struct symtab_tok {
	struct text text;
	int typ_id;
};

struct symtab_typ {
	struct text text;
	int *tok_ids;
	int ntok;
};


struct symtab {
	struct typebuf typebuf;
	struct table typ_table;
	struct table tok_table;
	struct symtab_typ *typs;
	struct symtab_tok *toks;
	int ntyp, ntyp_max;
	int ntok, ntok_max;
};

int symtab_init(struct symtab *tab, int typ_flags);
void symtab_destroy(struct symtab *tab);
void symtab_clear(struct symtab *tab);

int symtab_add_tok(struct symtab *tab, const struct text *tok, int *tok_idp);
int symtab_add_typ(struct symtab *tab, const struct text *tok, int *typ_idp);
int symtab_has_tok(const struct symtab *tab, const struct text *tok, int *tok_idp);
int symtab_has_typ(const struct symtab *tab, const struct text *typ, int *typ_idp);

#endif /* SYMTAB_H */
