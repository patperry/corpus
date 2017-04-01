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
	union {
		int boolean;		/**< value for #DATATYPE_BOOLEAN */
		double number;		/**< value for #DATATYPE_INTEGER or 
					  DATATYPE_REAL */
		struct text text; 	/**< value for #DATATYPE_TEXT */
		uint8_t *blob;		/**< value for #DATATYPE_ARRAY or
						#DATATYPE_RECORD */
	} value;			/**< the data value */
	int type_id;			/**< the data type ID */
};

int bool_assign(int *valptr, const uint8_t *ptr, size_t size);
int int_assign(int *valptr, const uint8_t *ptr, size_t size);
int double_assign(double *valptr, const uint8_t *ptr, size_t size);

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
