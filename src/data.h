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

enum datatype_kind {
	DATATYPE_ANY = -1,
	DATATYPE_NULL = 0,
	DATATYPE_BOOL,
	DATATYPE_NUMBER,
	DATATYPE_TEXT,
	DATATYPE_ARRAY,
	DATATYPE_RECORD
};

struct datatype_array {
	int type_id;
	int length;
};

struct datatype_record {
	int *type_ids;
	int *name_ids;
	int nfield;
};

struct datatype {
	int kind;
	union {
		struct datatype_array array;
		struct datatype_record record;
	} meta;
};

struct data {
	union {
		int bool_;
		double number;
		struct text text;
		uint8_t *blob;
	} value;
	int type_id;
};

int data_assign(struct data *d, struct schema *s, const uint8_t *ptr,
		size_t size);

#endif /* DATA_H */
