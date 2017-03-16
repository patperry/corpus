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

#ifndef ARRAY_H
#define ARRAY_H

/**
 * \file array.h
 *
 * Utility functions for arrays.
 */

#include <stddef.h>

/**
 * Grow an array to accommodate more elements, possibly re-allocating.
 *
 * \param baseptr pointer to pointer to first element
 * \param sizeptr pointer to the capacity (in elements) of the array
 * \param width size of each element
 * \param count number of occupied elements
 * \param nadd number of elements to append after the `count` occupied
 *        elements
 *
 * \returns 0 on success
 */
int array_grow(void **baseptr, int *sizeptr, size_t width, int count, int nadd);

/**
 * Determine the capacity for an array that needs to grow.
 *
 * \param count the minimum capacity
 * \param size the current capacity
 *
 * \returns the new capacity
 */
int array_grow_size(int count, int size);

#endif /* ARRAY_H */
