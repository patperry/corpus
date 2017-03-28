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
 * Symbol table, assigning integer IDs to tokens and types.
 */

struct symtab_token {
	struct text text;
	int type_id;
};

struct symtab_type {
	struct text text;
	int *token_ids;
	int ntoken;
};

struct symtab {
	struct typebuf typebuf;
	struct table type_table;
	struct table token_table;
	struct symtab_type *types;
	struct symtab_token *tokens;
	int ntype, ntype_max;
	int ntoken, ntoken_max;
};

int symtab_init(struct symtab *tab, int type_kind);
void symtab_destroy(struct symtab *tab);
void symtab_clear(struct symtab *tab);

int symtab_add_token(struct symtab *tab, const struct text *tok, int *idptr);
int symtab_add_type(struct symtab *tab, const struct text *typ, int *idptr);
int symtab_has_token(const struct symtab *tab, const struct text *tok,
		     int *idptr);
int symtab_has_type(const struct symtab *tab, const struct text *typ,
		    int *idptr);

#endif /* SYMTAB_H */
