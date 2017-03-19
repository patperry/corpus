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

#ifndef TYPEMGR_H
#define TYPEMGR_H

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

struct symtab {
	int dummy;
};

struct typemgr {
	struct symtab fields;
};

int typemgr_init(struct typemgr *t);
void typemgr_destroy(struct typemgr *t);

int typemgr_add_name(struct typemgr *t, uint8_t *ptr, size_t len);
int typemgr_add_array(struct typemgr *t, int type_id, int length, int *idptr);
int typemgr_add_record(struct typemgr *t, int *field_types, int *field_names,
			int nfield, int *idptr);

int typemgr_scan(struct typemgr *t, const uint8_t **bufptr,
		 const uint8_t *end, int *type_id);

#endif /* TYPEMGR_H */
