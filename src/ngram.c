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


#include "error.h"
#include "memory.h"
#include "table.h"
#include "census.h"
#include "ngram.h"


int corpus_ngram_init(struct corpus_ngram *ng, int width)
{
	(void)ng;
	(void)width;
	return 0;
}


void corpus_ngram_destroy(struct corpus_ngram *ng)
{
	(void)ng;
}


void corpus_ngram_clear(struct corpus_ngram *ng)
{
	(void)ng;
}


int corpus_ngram_add(struct corpus_ngram *ng, int type_id, double weight)
{
	(void)ng;
	(void)type_id;
	(void)weight;
	return 0;
}


int corpus_ngram_break(struct corpus_ngram *ng)
{
	(void)ng;
	return 0;
}
