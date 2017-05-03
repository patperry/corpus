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

#ifndef CENSUS_H
#define CENSUS_H

struct census {
	struct table table;
	int *items;
	double *weights;
	int nitem;
	int nitem_max;
};

int census_init(struct census *c);
void census_destroy(struct census *c);
void census_clear(struct census *c);

int census_add(struct census *c, int item, double weight);
int census_has(const struct census *c, int item, double *weightptr);
int census_sort(struct census *c);

#endif /* CENSUS_H */