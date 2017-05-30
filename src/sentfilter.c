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
#include <stdio.h>
#include "private/sentsuppress.h"
#include "unicode/sentbreakprop.h"
#include "error.h"
#include "memory.h"
#include "text.h"
#include "tree.h"
#include "sentscan.h"
#include "sentfilter.h"

#define BACKSUPP_NONE		0
#define BACKSUPP_PARTIAL	1
#define BACKSUPP_FULL		2

#define CHECK_ERROR(value) \
	do { \
		if (f->error) { \
			corpus_log(CORPUS_ERROR_INVAL, "an error occurred" \
				   " during a prior sentence filter" \
				   " operation"); \
			return (value); \
		} \
	} while (0)


static int corpus_sentfilter_backsupp(struct corpus_sentfilter *f,
				      const struct corpus_text *prefix,
				      int rule);
static int corpus_sentfilter_has_suppress(const struct corpus_sentfilter *f,
					  const struct corpus_text *text);

static int aterm_precedes(const struct corpus_text_iter *it);



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

	if ((err = corpus_tree_init(&f->backsupp))) {
		goto error_backsupp;
	}
	f->backsupp_rules = NULL;
	f->current.ptr = NULL;
	f->current.attr = 0;
	f->has_scan = 0;
	f->error = 0;
	return 0;

error_backsupp:
	corpus_log(err, "failed initializing sentence filter");
	return err;
}


void corpus_sentfilter_destroy(struct corpus_sentfilter *f)
{
	corpus_tree_destroy(&f->backsupp);
	corpus_free(f->backsupp_rules);
}


void corpus_sentfilter_clear(struct corpus_sentfilter *f)
{
	corpus_tree_clear(&f->backsupp);
	f->has_scan = 0;
}


int corpus_sentfilter_suppress(struct corpus_sentfilter *f,
			       const struct corpus_text *pattern)
{
	struct corpus_text prefix, suffix;
	struct corpus_text_iter it, it2;
	size_t attr, attr2, size, size2;
	int err, rule;

	CHECK_ERROR(CORPUS_ERROR_INVAL);
	
	err = 0;
	attr = 0;
	corpus_text_iter_make(&it, pattern);
	while (corpus_text_iter_advance(&it)) {
		attr |= it.attr;

		// find the next ATerm ('.')
		if (sent_break(it.current) != SENT_BREAK_ATERM) {
			continue;
		}

		// split the pattern around the ATerm
		size = (size_t)(it.ptr - pattern->ptr);
		prefix.ptr = pattern->ptr;
		prefix.attr = size | attr;

		it2 = it;
		attr2 = 0;
		while (corpus_text_iter_advance(&it2)) {
			attr2 |= it2.attr;
		}
		size2 = (size_t)(it2.ptr - it.ptr);
		suffix.ptr = (uint8_t *)it.ptr;
		suffix.attr = size2 | attr2;

		if (size2 == 0) {
			rule = BACKSUPP_FULL;
		} else {
			rule = BACKSUPP_PARTIAL;
		}

		if ((err = corpus_sentfilter_backsupp(f, &prefix, rule))) {
			goto out;
		}
	}
out:
	if (err) {
		f->error = err;
		corpus_log(err, "failed adding suppression to sentence filter");
	}

	return err;
}


int corpus_sentfilter_backsupp(struct corpus_sentfilter *f,
			       const struct corpus_text *prefix, int rule)
{
	struct corpus_text_iter it;
	int *rules;
	int size, size0, nnode, nnode0;
	int code, id, prop, parent_id, err;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	// root the suppression tree
	if (f->backsupp.nnode == 0) {
		if ((err = corpus_tree_root(&f->backsupp))) {
			goto out;
		}

		size = f->backsupp.nnode_max;
		assert(size > 0);

		rules = corpus_malloc(size * sizeof(*rules));
		if (!rules) {
			err = CORPUS_ERROR_NOMEM;
			goto out;
		}
		rules[0] = BACKSUPP_NONE;
		f->backsupp_rules = rules;
	}

	id = 0;

	// iterate over the characters in the prefix, in reverse
	corpus_text_iter_make(&it, prefix);
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
			if (aterm_precedes(&it)) {
				// skip over spaces that follow '.'
				continue;
			}
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
		nnode0 = f->backsupp.nnode;
		size0 = f->backsupp.nnode_max;
		if ((err = corpus_tree_add(&f->backsupp, parent_id, code,
					   &id))) {
			goto out;
		}
		nnode = f->backsupp.nnode;
		
		// check whether a new node got added
		if (nnode0 < nnode) {
			// expand the rules array if necessary
			size = f->backsupp.nnode_max;
			if (size0 < size) {
				rules = f->backsupp_rules;
				rules = corpus_realloc(rules,
						       size * sizeof(*rules));
				if (!rules) {
					err = CORPUS_ERROR_NOMEM;
					goto out;
				}
				f->backsupp_rules = rules;
			}

			// mark the new intermediate node as none
			f->backsupp_rules[id] = BACKSUPP_NONE;
		}
	}

	// if new, mark the terminal node with the rule
	if (f->backsupp_rules[id] != BACKSUPP_FULL) {
		f->backsupp_rules[id] = rule;
	}
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
	int code, id, parent_id, prop, skip_space, rule;
	
	if (f->backsupp.nnode == 0) {
		return 0;
	}

	skip_space = 1;
	rule = BACKSUPP_NONE;
	id = 0;

	// retreat until we reach a boundary or we run out of matches
	corpus_text_iter_make(&it, text);
	corpus_text_iter_skip(&it);

	while (corpus_text_iter_retreat(&it)) {
		code = (int)it.current;
		prop = sent_break(code);

		switch (prop) {
		case SENT_BREAK_EXTEND:
		case SENT_BREAK_FORMAT:
			continue;

		case SENT_BREAK_SEP:
		case SENT_BREAK_CLOSE:
		case SENT_BREAK_STERM:
			goto boundary;

		case SENT_BREAK_CR:
		case SENT_BREAK_LF:
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
		skip_space = 0; // done skipping over initial spaces

		if (code == ' ') {
			// skip over spaces that follow '.'
			if (aterm_precedes(&it)) {
				continue;
			}
		}

		parent_id = id;
		if (!corpus_tree_has(&f->backsupp, parent_id, code, &id)) {
			switch (code) {
			case ' ':
			case '.':
				goto boundary;
			default:
				return 0;
			}
		}

		rule = f->backsupp_rules[id];
	}

boundary:
	if (rule == BACKSUPP_FULL) {
		return 1;
	} else if (rule == BACKSUPP_PARTIAL) {
		printf("Partial!!\n");
		return 0;
	} else {
		return 0;
	}
}


int aterm_precedes(const struct corpus_text_iter *it)
{
	struct corpus_text_iter it2 = *it;
	int prop;

	while (corpus_text_iter_retreat(&it2)) {
		prop = sent_break(it2.current);

		switch (prop) {
		case SENT_BREAK_SEP:
		case SENT_BREAK_EXTEND:
		case SENT_BREAK_FORMAT:
			continue;

		case SENT_BREAK_ATERM:
			return 1;
		default:
			return 0;
		}
	}

	return 0;
}
