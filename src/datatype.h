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
 * Data types and type checking.
 */

#include <stdint.h>

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
	int *name_ids;
	int *type_ids;
	int nfield;
};

struct datatype {
	int kind;
	union {
		struct datatype_array array;
		struct datatype_record record;
	} meta;
};

struct schema {
	struct symtab names;
	struct datatype *types;
	int ntype, ntype_max;
};

int schema_init(struct schema *s);
void schema_destroy(struct schema *s);
void schema_clear(struct schema *s);

int schema_name(struct schema *s, const struct text *name, int *idptr);

int schema_array(struct schema *s, int type_id, int length, int *idptr);

int schema_record(struct schema *s, const int *type_ids, const int *name_ids,
		  int nfield, int *idptr);

int schema_union(struct schema *s, int type_id1, int type_id2, int *idptr);

int schema_scan(struct schema *s, const uint8_t *ptr, size_t len, int *idptr);

#endif /* DATATYPE_H */
