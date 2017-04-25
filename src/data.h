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
 * Data values.
 */

#include <stddef.h>
#include <stdint.h>

struct schema;

/**
 * A typed data value.
 */
struct data {
	const uint8_t *ptr;	/**< the value memory location */
	size_t size;		/**< the value size, in bytes */
	int type_id;		/**< the type ID */
};

/**
 * An iterator over the items in an array.
 */
struct data_items {
	const struct schema *schema;	/**< the data schema */
	int item_type;			/**< the array item type ID */
	int item_kind;			/**< the array item kind */
	int length;			/**< the array length */
	const uint8_t *ptr;		/**< the array memory location */

	struct data current;		/**< the current item value */
	int index;			/**< the current item index */
};

/**
 * An iterator over the fields in a record.
 */
struct data_fields {
	const struct schema *schema;	/**< the data schema */
	const int *field_types;		/**< the record field types */
	const int *field_names;		/**< the record field names*/
	int nfield;			/**< the number of record fields */
	const uint8_t *ptr;		/**< the record memory location */

	struct data current;		/**< the current field value */
	int name_id;			/**< the current field name */
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

/**
 * Get the boolean value of a data value.
 *
 * \param d the data value
 * \param valptr if non-NULL, a location to store the value
 *
 * \returns 0 on success, #ERROR_INVAL if the data value is null or
 * 	not boolean
 */
int data_bool(const struct data *d, int *valptr);

/**
 * Get the integer value of a data value.
 *
 * \param d the data value
 * \param valptr if non-NULL, a location to store the value
 *
 * \returns 0 on success; #ERROR_INVAL if the data value is null or
 * 	not integer; #ERROR_OVERFLOW if the data value is too big
 * 	to store in an `int`, in which case it gets clipped
 * 	to `INT_MAX` or `INT_MIN`
 */
int data_int(const struct data *d, int *valptr);

/**
 * Get the double value of a data value.
 *
 * \param d the data value
 * \param valptr if non-NULL, a location to store the value
 *
 * \returns 0 on success; #ERROR_INVAL if the data value is null or
 * 	not numeric (integer or real); #ERROR_OVERFLOW if the data
 * 	value is too big to store in a `double`, in which case it
 * 	gets set to `+Infinity` or `-Infinity`
 */
int data_double(const struct data *d, double *valptr);

/**
 * Get the text value of a data value.
 *
 * \param d the data value
 * \param valptr if non-NULL, a location to store the value
 *
 * \returns 0 on success; #ERROR_INVAL if the data value is null or
 * 	not text
 */
int data_text(const struct data *d, struct text *valptr);

/**
 * Get the number of items (the length) of an array data value.
 *
 * \param d the data value
 * \param s the data schema
 * \param nitemptr if non-NULL, a location to store the number of items
 *
 * \returns 0 on success; #ERROR_INVAL if the data value is null or
 * 	is not an array
 */
int data_nitem(const struct data *d, const struct schema *s, int *nitemptr);

/**
 * Get the array items from a data value.
 *
 * \param d the data value
 * \param s the data schema
 * \param valptr if non-NULL, a location to store the array items
 *
 * \returns 0 on success; #ERROR_INVAL if the data value is null or
 * 	is not an array
 */
int data_items(const struct data *d, const struct schema *s,
	       struct data_items *valptr);

/**
 * Advance an array items iterator to the next item.
 *
 * \param it the iterator
 *
 * \returns zero if no next item exists, nonzero otherwise
 */
int data_items_advance(struct data_items *it);

/**
 * Reset an array items iterator to the beginning of the array.
 *
 * \param it the iterator
 */
void data_items_reset(struct data_items *it);

/**
 * Get the number of fields of a record data value.
 *
 * \param d the data value
 * \param s the data schema
 * \param nfieldptr if non-NULL, a location to store the number of fields
 *
 * \returns 0 on success; #ERROR_INVAL if the data value is null or
 * 	is not a record
 */
int data_nfield(const struct data *d, const struct schema *s, int *nfieldptr);

/**
 * Get a record field from a data value.
 *
 * \param d the data value
 * \param s the data schema
 * \param name_id the record field name ID
 * \param valptr if non-NULL, a location to store the field value
 *
 * \returns 0 on success; #ERROR_INVAL if the data value is null,
 * 	is not a record, or is a record but does not have a field for
 * 	the given name
 */
int data_field(const struct data *d, const struct schema *s, int name_id,
	       struct data *valptr);

/**
 * Get the record fields from a data value.
 *
 * \param d the data value
 * \param s the data schema
 * \param valptr if non_NULL, a location to store the record fields 
 *
 * \returns 0 on success; #ERROR_INVAL if the data value is null or
 * 	is not a record
 */
int data_fields(const struct data *d, const struct schema *s,
		struct data_fields *valptr);

/**
 * Advance a record field iterator to the next field.
 *
 * \param it the iterator
 *
 * \returns zero if no next field exists, nonzero otherwise
 */
int data_fields_advance(struct data_fields *it);

/**
 * Reset a record field iterator to the beginning of the record.
 *
 * \param it the iterator
 */
void data_fields_reset(struct data_fields *it);

#endif /* DATA_H */
