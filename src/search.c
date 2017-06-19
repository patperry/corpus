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

#include <stddef.h>
#include "error.h"
#include "memory.h"
#include "table.h"
#include "tree.h"
#include "text.h"
#include "textset.h"
#include "termset.h"
#include "typemap.h"
#include "symtab.h"
#include "wordscan.h"
#include "filter.h"
#include "search.h"

#define CHECK_ERROR(value) \
	do { \
		if (search->error) { \
			corpus_log(CORPUS_ERROR_INVAL, "an error occurred" \
				   " during a prior search operation"); \
			return (value); \
		} \
	} while (0)


int corpus_search_init(struct corpus_search *search)
{
	int err;

	if ((err = corpus_termset_init(&search->terms))) {
		corpus_log(err, "failed initializing search");
		search->error = err;
		return err;
	}
	search->filter = NULL;
	search->length_max = 0;
	search->current.ptr = NULL;
	search->current.attr = 0;
	search->term_id = -1;
	search->error = 0;

	return 0;
}


void corpus_search_destroy(struct corpus_search *search)
{
	corpus_termset_destroy(&search->terms);
}


int corpus_search_add(struct corpus_search *search,
		      int *type_ids, int length, int *idptr)
{
	int err, id;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	if (search->filter) {
		err = CORPUS_ERROR_INVAL;
		corpus_log(err,
			   "attempted to add search term while in progress");
		goto out;
	}

	if ((err = corpus_termset_add(&search->terms, type_ids, length,
				      &id))) {
		goto out;
	}

	if (length > search->length_max) {
		search->length_max = length;
	}

out:
	if (err) {
		corpus_log(err, "failed adding term to search");
		search->error = err;
		id = -1;
	}
	if (idptr) {
		*idptr = id;
	}
	return err;
}


int corpus_search_has(const struct corpus_search *search,
		      int *type_ids, int length, int *idptr)
{
	return corpus_termset_has(&search->terms, type_ids, length, idptr);
}


int corpus_search_start(struct corpus_search *search,
			const struct corpus_text *text,
			struct corpus_filter *filter)
{
	int err;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	if ((err = corpus_filter_start(filter, text,
				       CORPUS_FILTER_SCAN_TOKENS))) {
		goto out;
	}

	search->filter = filter;
	search->current.ptr = NULL;
	search->current.attr = 0;
	search->term_id = -1;

out:
	if (err) {
		corpus_log(err, "failed starting search");
		search->error = err;
	}

	return err;
}


int corpus_search_advance(struct corpus_search *search)
{
	int err, length, term_id, type_id;

	CHECK_ERROR(0);

	while (corpus_filter_advance(search->filter)) {
		type_id = search->filter->type_id;
		if (type_id < 0) {
			continue;
		}

		length = 1;
		if (!corpus_termset_has(&search->terms, &type_id, length,
				       &term_id)) {
			continue;
		}

		search->current = search->filter->current;
		search->term_id = term_id;
		return 1;
	}
	err = search->filter->error;
	
	if (err) {
		corpus_log(err, "failed advancing search");
		search->error = err;
	}
	return 0;
}
