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

#ifndef SCHEMA_H
#define SCHEMA_H

/**
 * \file schema.h
 *
 * Data schema, mapping data types to integer IDs.
 */

#include <stdio.h>

int bool_assign(int *valptr, const uint8_t *ptr, size_t size, int nullval);
int int_assign(int *valptr, const uint8_t *ptr, size_t size, int nullval);
int double_assign(double *valptr, const uint8_t *ptr, size_t size,
		  double nullval);

/**
 * Used internally to store record (name,type) field descriptors.
 */
struct schema_buffer {
	int *type_ids;	/**< type ID buffer */
	int *name_ids;	/**< name ID buffer */
	int nfield;	/**< number of occupied buffer slots */
	int nfield_max;	/**< maximum buffer capacity */
};

/**
 * Used internally to sort record names.
 */
struct schema_sorter {
	const int **idptrs;	/**< name ID pointer buffer */
	int size;		/**< buffer size */
};

/**
 * A collection of data types.
 */
struct schema {
	struct schema_buffer buffer;	/**< internal field buffer */
	struct schema_sorter sorter;	/**< internal field name sorter */
	struct symtab names;		/**< record field names */
	struct datatype *types;		/**< data type array */
	int ntype;			/**< number of data types */
	int ntype_max;			/**< data type array capacity */
};

/**
 * Initialize an empty schema with just atomic types.
 *
 * \param s the schema
 *
 * \returns 0 on success
 */
int schema_init(struct schema *s);

/**
 * Release a schema resources.
 *
 * \param s the schema
 */
void schema_destroy(struct schema *s);

/**
 * Remove all names and non-atomic data types from the schema.
 */
void schema_clear(struct schema *s);

/**
 * Create a new field name, or get the name's ID if it already exists.
 *
 * \param s the schema
 * \param name the name token
 * \param idptr on exit, a pointer to the name ID
 *
 * \returns 0 on success
 */
int schema_name(struct schema *s, const struct text *name, int *idptr);

/**
 * Create a new array type, or get the type's ID if it already exists.
 *
 * \param s the schema
 * \param type_id the element type
 * \param length the array length, or -1 for variable
 * \param idptr on exit, a pointer to the type ID
 *
 * \returns 0 on success
 */
int schema_array(struct schema *s, int type_id, int length, int *idptr);

/**
 * Create a new record type, or get the types ID if it already exists.
 *
 * \param s the schema
 * \param type_ids the field type IDs
 * \param name_ids the field name IDs
 * \param nfield the number of record fields
 * \param idptr on exit, a pointer to the type ID
 *
 * \returns 0 on success
 */
int schema_record(struct schema *s, const int *type_ids, const int *name_ids,
		  int nfield, int *idptr);

/**
 * Get or create a new type by taking the union of two other types.
 *
 * \param s the schema
 * \param id1 the first type ID
 * \param id2 the second type ID
 * \param idptr on exit, a pointer to the union's type ID
 *
 * \returns 0 on success
 */
int schema_union(struct schema *s, int id1, int id2, int *idptr);

int schema_scan(struct schema *s, const uint8_t *ptr, size_t size, int *idptr);

/**
 * Write a textual representation of a data type to the specified stream.
 *
 * \param stream the stream
 * \param s the schema
 * \param id the type id
 *
 * \returns 0 on success
 */
int write_datatype(FILE *stream, const struct schema *s, int id);

#ifndef DOXYGEN_SHOULD_SKIP_THIS
/**
 * Grow a schema's field buffer to accommodate more elements.
 *
 * \param buf the field buffer
 * \param nadd the number of new elements to accomodate
 *
 * \returns 0 on success
 */
int schema_buffer_grow(struct schema_buffer *buf, int nadd);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

#endif /* SCHEMA_H */
