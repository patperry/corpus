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

/**
 * Symbol table token.
 */
struct symtab_token {
	struct corpus_text text;/**< the token text */
	int type_id;		/**< the ID of the token's type */
};

/**
 * Symbol table type.
 */
struct symtab_type {
	struct corpus_text text;/**< the type text */
	int *token_ids;		/**< the IDs of the tokens in the type */
	int ntoken;		/**< the number of tokens in the type */
};

/**
 * Symbol table.
 */
struct symtab {
	struct typemap typemap;		/**< type map, for normalizing
					  tokens to types */
	struct corpus_table type_table;	/**< type hash table */
	struct corpus_table token_table;/**< token hash table */
	struct symtab_type *types;	/**< type array */
	struct symtab_token *tokens;	/**< token array */
	int ntype;			/**< type array length */
	int ntype_max;			/**< type array capacity */
	int ntoken;			/**< token array length */
	int ntoken_max;			/**< token array capacity */
};


/**
 * Initialize an empty symbol table with types of the specified kind.
 *
 * \param tab the symbol table
 * \param type_kind the type kind specifier, a bit mask of #type_kind values
 * \param stemmer the stemming algorithm name, or NULL to disable stemming
 *
 * \returns 0 on success
 */
int symtab_init(struct symtab *tab, int type_kind, const char *stemmer);

/**
 * Release the resources associated with a symbol table.
 *
 * \param tab the symbol table
 */
void symtab_destroy(struct symtab *tab);

/**
 * Remove all tokens and types from a symbol table.
 *
 * \param tab the symbol table
 */
void symtab_clear(struct symtab *tab);

/**
 * Add a token to a symbol table if it does not already exist there, and
 * get the id of the token in the table.
 *
 * \param tab the symbol table
 * \param tok the token
 * \param idptr a pointer to store the token id, or NULL
 *
 * \returns 0 on success
 */
int symtab_add_token(struct symtab *tab, const struct corpus_text *tok,
		     int *idptr);

/**
 * Add a type to a symbol table if it does not already exist there, and
 * get the id of the type in the table.
 *
 * \param tab the symbol table
 * \param typ the type
 * \param idptr a pointer to store the type id, or NULL
 *
 * \returns 0 on success
 */
int symtab_add_type(struct symtab *tab, const struct corpus_text *typ,
		    int *idptr);

/**
 * Query whether a token exists in a symbol table.
 *
 * \param tab the symbol table
 * \param tok the token
 * \param idptr a pointer to store the token id if it exists, or NULL
 *
 * \returns non-zero if the token exists in the table; zero otherwise
 */
int symtab_has_token(const struct symtab *tab, const struct corpus_text *tok,
		     int *idptr);

/**
 * Query whether a type exists in a symbol table.
 *
 * \param tab the symbol table
 * \param typ the type
 * \param idptr a pointer to store the type id if it exists, or NULL
 *
 * \returns non-zero if the type exists in the table; zero otherwise
 */
int symtab_has_type(const struct symtab *tab, const struct corpus_text *typ,
		    int *idptr);

#endif /* SYMTAB_H */
