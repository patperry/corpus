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

#ifndef DATA_H
#define DATA_H

/**
 * \file data.h
 *
 * Data types and values.
 */

#include <stddef.h>
#include <stdint.h>

struct schema;

/**
 * A basic data type.
 */
enum datatype_kind {
	DATATYPE_ANY = -1,	/**< universal (top), supertype of all others */
	DATATYPE_NULL = 0,	/**< empty (bottom), subtype of all others */
	DATATYPE_BOOLEAN,	/**< boolean (true/false) value */
	DATATYPE_NUMBER,	/**< floating-point number */
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
 * A typed data value.
 */
struct data {
	union {
		int boolean;		/**< value for #DATATYPE_BOOLEAN */
		double number;		/**< value for #DATATYPE_NUMBER */
		struct text text; 	/**< value for #DATATYPE_TEXT */
		uint8_t *blob;		/**< value for #DATATYPE_ARRAY or
						#DATATYPE_RECORD */
	} value;			/**< the data value */
	int type_id;			/**< the data type ID */
};

/**
 * Assign a data value by parsing input in JavaScript Object Notation (JSON)
 * format.
 *
 * \param d the data value
 * \param s a schema to store the type information
 * \param ptr input, UTF-8 encoded characters
 * \param size size the input length, in bytes
 *
 * \returns 0 on success, nonzero for invalid input (a parse error), memory
 * 	allocation failure, or overflow error
 */
int data_assign(struct data *d, struct schema *s, const uint8_t *ptr,
		size_t size);

#endif /* DATA_H */
