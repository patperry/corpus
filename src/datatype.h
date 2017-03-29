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

enum atom_type {
	ATOM_NULL = 0,
	ATOM_BOOL,
	ATOM_NUMBER,
	ATOM_TEXT
};

struct array_type {
	int element_type;
	int length;
};

struct record_type {
	int *field_types;
	int *field_names;
	int nfield;
};

struct datatyper {
	int dummy;
};

int datatyper_init(struct datatyper *t);
void datatyper_destroy(struct datatyper *t);

int datatyper_add_name(struct datatyper *t, uint8_t *ptr, size_t len);
int datatyper_add_array(struct datatyper *t, int type_id, int length,
			int *idptr);
int datatyper_add_record(struct datatyper *t, int *field_types,
			 int *field_names, int nfield, int *idptr);

int datatyper_scan(struct datatyper *t, const uint8_t *ptr, size_t len);

#endif /* DATATYPE_H */
