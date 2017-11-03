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
#include <string.h>
#include "../lib/utf8lite/src/utf8lite.h"
#include "error.h"
#include "memory.h"
#include "table.h"
#include "tree.h"
#include "textset.h"
#include "termset.h"
#include "stem.h"
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


static void buffer_init(struct corpus_search_buffer *buffer);
static void buffer_destroy(struct corpus_search_buffer *buffer);
static void buffer_clear(struct corpus_search_buffer *buffer);
static int buffer_reserve(struct corpus_search_buffer *buffer, int size);
static void buffer_ignore(struct corpus_search_buffer *buffer,
		          const struct utf8lite_text *text);
static void buffer_push(struct corpus_search_buffer *buffer, int type_id,
		        const struct utf8lite_text *token);
static int buffer_advance(struct corpus_search_buffer *buffer,
			  struct corpus_filter *filter);


int corpus_search_init(struct corpus_search *search)
{
	int err;

	if ((err = corpus_termset_init(&search->terms))) {
		corpus_log(err, "failed initializing search");
		search->error = err;
		return err;
	}
	buffer_init(&search->buffer);
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
	buffer_destroy(&search->buffer);
	corpus_termset_destroy(&search->terms);
}


int corpus_search_add(struct corpus_search *search,
		      const int *type_ids, int length, int *idptr)
{
	int err, id = -1;

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
		      const int *type_ids, int length, int *idptr)
{
	return corpus_termset_has(&search->terms, type_ids, length, idptr);
}


int corpus_search_start(struct corpus_search *search,
			const struct utf8lite_text *text,
			struct corpus_filter *filter)
{
	int err;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	buffer_clear(&search->buffer);
	if ((err = buffer_reserve(&search->buffer, search->length_max))) {
		goto out;
	}

	if ((err = corpus_filter_start(filter, text))) {
		goto out;
	}

	search->filter = filter;
	search->current.ptr = NULL;
	search->current.attr = 0;
	search->term_id = -1;
	search->length = 0;

out:
	if (err) {
		corpus_log(err, "failed starting search");
		search->error = err;
	}

	return err;
}


int corpus_search_advance(struct corpus_search *search)
{
	const struct utf8lite_text *tokens;
	struct utf8lite_text token;
	const int *type_ids;
	int err, length, i, off, nbuf, term_id;

	CHECK_ERROR(0);

	do {
		nbuf = search->buffer.size;
		length = search->length;
		if (length == 0) {
			length = nbuf;
		} else {
			length--;
		}

		while (length > 0) {
			off = nbuf - length;
			type_ids = search->buffer.type_ids + off;

			if (corpus_termset_has(&search->terms, type_ids, length,
					       &term_id)) {
				search->length = length;
				search->term_id = term_id;

				tokens = search->buffer.tokens + off;
				token = tokens[0];
				for (i = 1; i < length; i++) {
					token.attr +=
						UTF8LITE_TEXT_SIZE(&tokens[i]);
					token.attr |=
						UTF8LITE_TEXT_BITS(&tokens[i]);
				}
				search->current = token;

				return 1;
			}
			length--;
		}
		search->length = 0;
	} while (buffer_advance(&search->buffer, search->filter));

	if ((err = search->filter->error)) {
		corpus_log(err, "failed advancing search");
		search->error = err;
	}

	search->current.ptr = NULL;
	search->current.attr = 0;
	search->term_id = -1;
	search->length = 0;

	return 0;
}


void buffer_init(struct corpus_search_buffer *buffer)
{
	buffer->tokens = NULL;
	buffer->type_ids = NULL;
	buffer->size = 0;
	buffer->size_max = 0;
}


void buffer_destroy(struct corpus_search_buffer *buffer)
{
	corpus_free(buffer->type_ids);
	corpus_free(buffer->tokens);
}


void buffer_clear(struct corpus_search_buffer *buffer)
{
	buffer->size = 0;
}


int buffer_reserve(struct corpus_search_buffer *buffer, int size)
{
	struct utf8lite_text *tokens;
	int *type_ids;
	int err;

	if (size <= buffer->size_max) {
		buffer->size_max = size;
		return 0;
	}

	tokens = corpus_realloc(buffer->tokens, (size_t)size * sizeof(*tokens));
	if (!tokens) {
		err = CORPUS_ERROR_NOMEM;
		goto out;
	}
	buffer->tokens = tokens;

	type_ids = corpus_realloc(buffer->type_ids,
				  (size_t)size * sizeof(*type_ids));
	if (!type_ids) {
		err = CORPUS_ERROR_NOMEM;
		goto out;
	}
	buffer->type_ids = type_ids;

	buffer->size_max = size;
	err = 0;
out:
	if (err) {
		corpus_log(err, "failed allocating search buffer");
	}
	return err;
}


int buffer_advance(struct corpus_search_buffer *buffer,
		   struct corpus_filter *filter)
{
	const struct utf8lite_text *current;
	int type_id;

	while (corpus_filter_advance(filter)) {
		type_id = filter->type_id;
		current = &filter->current;
		if (type_id == CORPUS_TYPE_NONE) {
			buffer_ignore(buffer, current);
			continue;
		} else if (type_id < 0) {
			buffer_clear(buffer);
			continue;
		}
		buffer_push(buffer, type_id, current);
		return 1;
	}

	return 0;
}


void buffer_ignore(struct corpus_search_buffer *buffer,
		   const struct utf8lite_text *text)
{
	if (buffer->size == 0) {
		return;
	}

	buffer->tokens[buffer->size - 1].attr |= UTF8LITE_TEXT_BITS(text);
	buffer->tokens[buffer->size - 1].attr += UTF8LITE_TEXT_SIZE(text);
}


void buffer_push(struct corpus_search_buffer *buffer, int type_id,
		 const struct utf8lite_text *token)
{
	int n = buffer->size;

	if (buffer->size_max == 0) {
		return;
	}

	if (n == buffer->size_max) {
		n--;
		if (n > 0) {
			memmove(buffer->type_ids, buffer->type_ids + 1,
				(size_t)n * sizeof(*buffer->type_ids));
			memmove(buffer->tokens, buffer->tokens + 1,
				(size_t)n * sizeof(*buffer->tokens));
		}
	}

	buffer->type_ids[n] = type_id;
	buffer->tokens[n] = *token;
	buffer->size = n + 1;
}
