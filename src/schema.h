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

struct schema_buffer {
	int *type_ids;
	int *name_ids;
	int nfield;
	int nfield_max;
};

struct schema_sorter {
	const int **idptrs;
	int size;
};

struct schema {
	struct schema_buffer buffer;
	struct schema_sorter sorter;
	struct symtab names;
	struct datatype *types;
	int ntype;
	int ntype_max;
};

int schema_init(struct schema *s);
void schema_destroy(struct schema *s);
void schema_clear(struct schema *s);

int schema_name(struct schema *s, const struct text *name, int *idptr);

int schema_array(struct schema *s, int type_id, int length, int *idptr);

int schema_record(struct schema *s, const int *type_ids, const int *name_ids,
		  int nfield, int *idptr);

int schema_union(struct schema *s, int id1, int id2, int *idptr);

int write_datatype(FILE *stream, const struct schema *s, int id);

#ifndef DOXYGEN_SHOULD_SKIP_THIS
int schema_buffer_grow(struct schema_buffer *buf, int nadd);
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

#endif /* SCHEMA_H */
