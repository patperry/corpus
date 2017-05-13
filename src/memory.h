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

#ifndef CORPUS_MEMORY_H
#define CORPUS_MEMORY_H

/**
 * \file memory.h
 *
 * Memory allocation routines.  * Unlike the stdlib functions, on an
 * allocation failure these will log the error and call
 * #corpus_alloc_fail_func.
 * Memory returned by these functions can be freed with `free`.
 */

#include <stddef.h>

/**
 * If non-`NULL`, this function gets called after an allocation failure.
 * Initialized to `NULL`.
 */
extern void (*corpus_alloc_fail_func) (void);

/**
 * Replacement for `calloc`.
 */
void *corpus_calloc(size_t count, size_t size);

/**
 * Replacement for `free`.
 */
void corpus_free(void *ptr);

/**
 * Replacement for `malloc`.
 */
void *corpus_malloc(size_t size);

/**
 * Replacement for `realloc`.
 */
void *corpus_realloc(void *ptr, size_t size);

/**
 * Replacement for `strdup`.
 */
char *corpus_strdup(const char *s1);

#endif /* CORPUS_MEMORY_H */
