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

#ifndef DATATYPE_H
#define DATATYPE_H

/**
 * \file datatype.h
 *
 * Data types and data schema.
 */

#include <stdio.h>

struct render;

/**
 * A basic data type.
 */
enum datatype_kind {
	DATATYPE_ANY = -1,	/**< universal (top), supertype of all others */
	DATATYPE_NULL = 0,	/**< empty (bottom), subtype of all others */
	DATATYPE_BOOLEAN,	/**< boolean (true/false) value */
	DATATYPE_INTEGER,	/**< integer-valued number */
	DATATYPE_REAL,		/**< real-valued floating point number */
	DATATYPE_TEXT,		/**< unicode text */
	DATATYPE_ARRAY,		/**< array type */
	DATATYPE_RECORD		/**< record type */
};

/**
 * An array type, of fixed or variable length. Array elements all have the
 * same type.
 */
struct datatype_array {
	int type_id;	/**< the element type */
	int length;	/**< the length (-1 for variable) */
};

/**
 * A record type, with named fields. The fields are not ordered, so that
 * we consider `{"a": 1, "b": true}` and `{"b": false, "a": 0}` to have the
 * same type.
 */
struct datatype_record {
	int *type_ids;	/**< the field types */
	int *name_ids;	/**< the field names */
	int nfield;	/**< the number of fields */
};

/**
 * A data type.
 */
struct datatype {
	int kind;	/**< the kind of data type, a #datatype_kind value */
	union {
		struct datatype_array array;
			/**< metadata for kind #DATATYPE_ARRAY */
		struct datatype_record record;
			/**< metadata for kind #DATATYPE_RECORD */
	} meta;		/**< the data type's metadata */
};

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
 * Data schema, mapping data types to integer IDs.
 */
struct schema {
	struct schema_buffer buffer;	/**< internal field buffer */
	struct schema_sorter sorter;	/**< internal field name sorter */
	struct symtab names;		/**< record field names */
	struct corpus_table arrays;	/**< array type table */
	struct corpus_table records;	/**< record type table */
	struct datatype *types;		/**< data type array */
	int ntype;			/**< number of data types */
	int narray;			/**< number of array types */
	int nrecord;			/**< number of record types */
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

/**
 * Scan an input value and add its data type to the schema.
 *
 * \param s the schema
 * \param ptr the input buffer
 * \param size the size (in bytes) of the input buffer
 * \param idptr on exit, a pointer to the value's type ID
 *
 * \returns 0 on success
 */
int schema_scan(struct schema *s, const uint8_t *ptr, size_t size, int *idptr);

/**
 * Render a textual representation of a data type.
 *
 * \param r the render object
 * \param s the schema
 * \param id the type id
 */
void render_datatype(struct render *r, const struct schema *s, int id);

/**
 * Write a textual representation of a data type to the specified stream.
 * Escape all control characters and non-ASCII Unicode characters using
 * JSON-style backslash (\) escapes.
 *
 * \param stream the stream
 * \param s the schema
 * \param id the type id
 *
 * \returns 0 on success
 */
int write_datatype(FILE *stream, const struct schema *s, int id);

#endif /* DATATYPE_H */
