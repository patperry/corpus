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
	const uint8_t *ptr;
	size_t size;
	int type_id;
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

int data_bool(const struct data *d, int *valptr);
int data_int(const struct data *d, int *valptr);
int data_double(const struct data *d, double *valptr);
int data_text(const struct data *d, struct text *valptr);

int data_field(const struct data *d, const struct schema *s, int name_id,
	       struct data *valptr);
int data_item(const struct data *d, const struct schema *s, int index,
	      struct data *valptr);

#endif /* DATA_H */
