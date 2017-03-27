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

#ifndef TABLE_H
#define TABLE_H

/**
 * \file table.h
 *
 * Hash table.
 */

#include <stddef.h>
#include <stdint.h>

/* Code for empty buckets. */
#define TABLE_ITEM_NONE (-1)

/* Linear probing:
 *
 *     h(k,i) = h(k) + i
 */
#define PROBE_JUMP_LIN(nprobe)	(1)

/* Quadratic probing:
 *
 *     h(k,i) = h(k) + 0.5 i + 0.5 i^2
 *
 * When the table size "m" is a power of 2, the values h(k,i) % m
 * are all distinct for i = 0, 1, ..., m - 1
 *
 *     https://en.wikipedia.org/wiki/Quadratic_probing
 */
#define PROBE_JUMP_QUAD(nprobe)	(nprobe)

/* Hash table probing method */
#define PROBE_JUMP(nprobe)	PROBE_JUMP_QUAD(nprobe)

struct table {
	int *items;
	int max;
	unsigned mask;
};

int table_init(struct table *tab);
void table_destroy(struct table *tab);
void table_clear(struct table *tab);
int table_grow(struct table *tab, int count, int nadd);
int table_next_empty(const struct table *tab, uint64_t hash);

#endif /* TABLE_H */
