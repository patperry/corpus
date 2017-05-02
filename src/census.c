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

#include "census.h"


int census_init(struct census *c)
{
	(void)c;
	return 0;
}


void census_destroy(struct census *c)
{
	(void)c;
}


void census_clear(struct census *c)
{
	(void)c;
}


int census_add(struct census *c, int item, double weight)
{
	(void)c;
	(void)item;
	(void)weight;
	return 0;
}


int census_has(const struct census *c, int item, double *weightptr)
{
	double weight = 0;

	(void)c;
	(void)item;

	if (weightptr) {
		*weightptr = weight;
	}

	return 0;
}


int census_sort(struct census *c)
{
	(void)c;
	return 0;
}
