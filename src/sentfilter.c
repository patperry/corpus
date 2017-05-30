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

#include <assert.h>
#include <stddef.h>
#include "private/sentsuppress.h"
#include "unicode/sentbreakprop.h"
#include "error.h"
#include "memory.h"
#include "text.h"
#include "tree.h"
#include "sentscan.h"
#include "sentfilter.h"

#define SUPPRESS_NONE	0
#define SUPPRESS_FULL	1

#define CHECK_ERROR(value) \
	do { \
		if (f->error) { \
			corpus_log(CORPUS_ERROR_INVAL, "an error occurred" \
				   " during a prior sentence filter" \
				   " operation"); \
			return (value); \
		} \
	} while (0)


static int corpus_sentfilter_has_suppress(const struct corpus_sentfilter *f,
					  const struct corpus_text *text);


const char **corpus_sentsuppress_names(void)
{
	return sentsuppress_names();
}


const uint8_t **corpus_sentsuppress_list(const char *name, int *lenptr)
{
	return sentsuppress_list(name, lenptr);
}


int corpus_sentfilter_init(struct corpus_sentfilter *f)
{
	int err;

	if ((err = corpus_tree_init(&f->suppress))) {
		goto error_suppress;
	}
	f->suppress_rules = NULL;
	f->current.ptr = NULL;
	f->current.attr = 0;
	f->has_scan = 0;
	f->error = 0;
	return 0;

error_suppress:
	corpus_log(err, "failed initializing sentence filter");
	return err;
}


void corpus_sentfilter_destroy(struct corpus_sentfilter *f)
{
	corpus_tree_destroy(&f->suppress);
	corpus_free(f->suppress_rules);
}


int corpus_sentfilter_suppress(struct corpus_sentfilter *f,
			       const struct corpus_text *pattern)
{
	struct corpus_text_iter it;
	int *rules;
	int size, size0, nnode, nnode0;
	int code, id, prop, parent_id, err;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	// root the suppression tree
	if (f->suppress.nnode == 0) {
		if ((err = corpus_tree_root(&f->suppress))) {
			goto out;
		}

		size = f->suppress.nnode_max;
		assert(size > 0);

		rules = corpus_malloc(size * sizeof(*rules));
		if (!rules) {
			err = CORPUS_ERROR_NOMEM;
			goto out;
		}
		rules[0] = SUPPRESS_NONE;
		f->suppress_rules = rules;
	}

	id = 0;

	// iterate over the characters in the pattern, in reverse
	corpus_text_iter_make(&it, pattern);
	corpus_text_iter_skip(&it);

	while (corpus_text_iter_retreat(&it)) {
		code = (int)it.current;
		prop = sent_break(code);

		switch (prop) {
		case SENT_BREAK_EXTEND:
		case SENT_BREAK_FORMAT:
			// skip over extenders and format characters
			continue;

		case SENT_BREAK_SP:
			// replace space with ' '
			code = ' ';
			break;

		case SENT_BREAK_ATERM:
			// replace full stops with '.'
			code = '.';
			break;

		default:
			break;
		}

		parent_id = id;
		nnode0 = f->suppress.nnode;
		size0 = f->suppress.nnode_max;
		if ((err = corpus_tree_add(&f->suppress, parent_id, code,
					   &id))) {
			goto out;
		}
		nnode = f->suppress.nnode;
		
		// check whether a new node got added
		if (nnode0 < nnode) {
			// expand the rules array if necessary
			size = f->suppress.nnode_max;
			if (size0 < size) {
				rules = f->suppress_rules;
				rules = corpus_realloc(rules,
						       size * sizeof(*rules));
				if (!rules) {
					err = CORPUS_ERROR_NOMEM;
					goto out;
				}
				f->suppress_rules = rules;
			}

			// mark the new intermediate rule as partial
			f->suppress_rules[id] = SUPPRESS_NONE;
		}
	}

	// mark the terminal nodes as full
	f->suppress_rules[id] = SUPPRESS_FULL;
	err = 0;

out:
	if (err) {
		f->error = err;
		corpus_log(err, "failed adding suppression to sentence filter");
	}
	return err;
}


int corpus_sentfilter_start(struct corpus_sentfilter *f,
			    const struct corpus_text *text)
{
	CHECK_ERROR(CORPUS_ERROR_INVAL);

	corpus_sentscan_make(&f->scan, text);
	f->current.ptr = NULL;
	f->current.attr = 0;
	f->has_scan = 1;

	return 0;
}


int corpus_sentfilter_advance(struct corpus_sentfilter *f)
{
	const struct corpus_text *current;
	const uint8_t *ptr;
	size_t size, attr;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	if (!f->has_scan || !corpus_sentscan_advance(&f->scan)) {
		f->current.ptr = NULL;
		f->current.attr = 0;
		f->has_scan = 0;
		return 0;
	}

	current = &f->scan.current;
	if (f->scan.type != CORPUS_SENT_ATERM
			|| !corpus_sentfilter_has_suppress(f, current)) {
		f->current = *current;
		return 1;
	}

	ptr = current->ptr;
	size = CORPUS_TEXT_SIZE(current);
	attr = CORPUS_TEXT_BITS(current);

	while (corpus_sentscan_advance(&f->scan)) {
		current = &f->scan.current;
		size += CORPUS_TEXT_SIZE(current);
		attr |= CORPUS_TEXT_BITS(current);

		if (f->scan.type != CORPUS_SENT_ATERM) {
			break;
		} else if (!corpus_sentfilter_has_suppress(f, current)) {
			break;
		}
	}

	f->current.ptr = (uint8_t *)ptr;
	f->current.attr = size | attr;

	return 1;
}


int corpus_sentfilter_has_suppress(const struct corpus_sentfilter *f,
				   const struct corpus_text *text)
{
	struct corpus_text_iter it;
	int code, id, has, parent_id, prop, skip_space;
	
	if (f->suppress.nnode == 0) {
		return 0;
	}

	skip_space = 1;
	has = 0;
	id = 0;
	corpus_text_iter_make(&it, text);
	corpus_text_iter_skip(&it);

	while (corpus_text_iter_retreat(&it)) {
		code = (int)it.current;
		prop = sent_break(code);

		switch (prop) {
		case SENT_BREAK_EXTEND:
		case SENT_BREAK_FORMAT:
			continue;

		case SENT_BREAK_SP:
			if (skip_space) {
				continue;
			}
			code = ' ';
			break;

		case SENT_BREAK_ATERM:
			code = '.';
			break;

		default:
			break;
		}

		parent_id = id;
		if (!corpus_tree_has(&f->suppress, parent_id, code, &id)) {
			switch (prop) {
			case SENT_BREAK_ATERM:
			case SENT_BREAK_CLOSE:
			case SENT_BREAK_CR:
			case SENT_BREAK_LF:
			case SENT_BREAK_SEP:
			case SENT_BREAK_SP:
				return has;
			default:
				return 0;
			}
		}

		if (f->suppress_rules[id] == SUPPRESS_FULL) {
			has = 1;
		} else {
			has = 0;
		}

		// if we just saw a space, skip to the next non-space
		skip_space = (code == ' ') ? 1 : 0;
	}

	return has;
}
