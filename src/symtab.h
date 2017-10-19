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

#ifndef CORPUS_SYMTAB_H
#define CORPUS_SYMTAB_H

/**
 * \file symtab.h
 *
 * Symbol table, assigning integer IDs to tokens and types.
 */

/** Code for a missing or non-existent token */
#define CORPUS_TOKEN_NONE (-1)

/** Code for a missing or non-existent type */
#define CORPUS_TYPE_NONE (-1)

/**
 * Symbol table token.
 */
struct corpus_symtab_token {
	struct utf8lite_text text;/**< the token text */
	int type_id;		/**< the ID of the token's type */
};

/**
 * Symbol table type.
 */
struct corpus_symtab_type {
	struct utf8lite_text text;/**< the type text */
	int *token_ids;		/**< the IDs of the tokens in the type */
	int ntoken;		/**< the number of tokens in the type */
};

/**
 * Symbol table.
 */
struct corpus_symtab {
	struct utf8lite_textmap typemap;/**< type map, for normalizing
					  tokens to types */
	struct corpus_table type_table;	/**< type hash table */
	struct corpus_table token_table;/**< token hash table */
	struct corpus_symtab_type *types;	/**< type array */
	struct corpus_symtab_token *tokens;	/**< token array */
	int ntype;			/**< type array length */
	int ntype_max;			/**< type array capacity */
	int ntoken;			/**< token array length */
	int ntoken_max;			/**< token array capacity */
};


/**
 * Initialize an empty symbol table with types of the specified kind.
 *
 * \param tab the symbol table
 * \param type_kind the type kind specifier, a bit mask of #corpus_type_kind
 * 	values
 *
 * \returns 0 on success
 */
int corpus_symtab_init(struct corpus_symtab *tab, int type_kind);

/**
 * Release the resources associated with a symbol table.
 *
 * \param tab the symbol table
 */
void corpus_symtab_destroy(struct corpus_symtab *tab);

/**
 * Remove all tokens and types from a symbol table.
 *
 * \param tab the symbol table
 */
void corpus_symtab_clear(struct corpus_symtab *tab);

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
int corpus_symtab_add_token(struct corpus_symtab *tab,
			    const struct utf8lite_text *tok, int *idptr);

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
int corpus_symtab_add_type(struct corpus_symtab *tab,
			   const struct utf8lite_text *typ, int *idptr);

/**
 * Query whether a token exists in a symbol table.
 *
 * \param tab the symbol table
 * \param tok the token
 * \param idptr a pointer to store the token id if it exists, or NULL
 *
 * \returns non-zero if the token exists in the table; zero otherwise
 */
int corpus_symtab_has_token(const struct corpus_symtab *tab,
			    const struct utf8lite_text *tok, int *idptr);

/**
 * Query whether a type exists in a symbol table.
 *
 * \param tab the symbol table
 * \param typ the type
 * \param idptr a pointer to store the type id if it exists, or NULL
 *
 * \returns non-zero if the type exists in the table; zero otherwise
 */
int corpus_symtab_has_type(const struct corpus_symtab *tab,
			   const struct utf8lite_text *typ, int *idptr);

#endif /* CORPUS_SYMTAB_H */
