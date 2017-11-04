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
#include "../lib/utf8lite/src/utf8lite.h"
#include "private/sentsuppress.h"
#include "unicode/sentbreakprop.h"
#include "error.h"
#include "memory.h"
#include "table.h"
#include "tree.h"
#include "sentscan.h"
#include "sentfilter.h"

#define BACKSUPP_NONE		0
#define BACKSUPP_PARTIAL	1
#define BACKSUPP_FULL		2

#define FWDSUPP_NONE		0
#define FWDSUPP_FULL		1

#define CHECK_ERROR(value) \
	do { \
		if (f->error) { \
			corpus_log(CORPUS_ERROR_INVAL, "an error occurred" \
				   " during a prior sentence filter" \
				   " operation"); \
			return (value); \
		} \
	} while (0)


static int add_backsupp(struct corpus_sentfilter *f,
			const struct utf8lite_text *prefix, int rule);
static int add_fwdsupp(struct corpus_sentfilter *f,
		        const struct utf8lite_text *pattern);
static int has_suppress(const struct corpus_sentfilter *f,
			struct utf8lite_text_iter *it);
static int has_fwdsupp(const struct corpus_sentfilter *f,
			struct utf8lite_text_iter *it);


const char **corpus_sentsuppress_names(void)
{
	return sentsuppress_names();
}


const uint8_t **corpus_sentsuppress_list(const char *name, int *lenptr)
{
	return sentsuppress_list(name, lenptr);
}


int corpus_sentfilter_init(struct corpus_sentfilter *f, int flags)
{
	int err;

	if ((err = corpus_tree_init(&f->backsupp))) {
		goto error_backsupp;
	}
	if ((err = corpus_tree_init(&f->fwdsupp))) {
		goto error_fwdsupp;
	}
	f->backsupp_rules = NULL;
	f->fwdsupp_rules = NULL;
	f->current.ptr = NULL;
	f->current.attr = 0;
	f->has_scan = 0;
	f->flags = flags;
	f->error = 0;
	return 0;

error_fwdsupp:
	corpus_tree_destroy(&f->backsupp);
error_backsupp:
	corpus_log(err, "failed initializing sentence filter");
	return err;
}


void corpus_sentfilter_destroy(struct corpus_sentfilter *f)
{
	corpus_free(f->fwdsupp_rules);
	corpus_free(f->backsupp_rules);
	corpus_tree_destroy(&f->fwdsupp);
	corpus_tree_destroy(&f->backsupp);
}


void corpus_sentfilter_clear(struct corpus_sentfilter *f)
{
	corpus_tree_clear(&f->backsupp);
	corpus_tree_clear(&f->fwdsupp);
	f->has_scan = 0;
}


int corpus_sentfilter_suppress(struct corpus_sentfilter *f,
			       const struct utf8lite_text *pattern)
{
	struct utf8lite_text prefix;
	struct utf8lite_text_iter it;
	size_t attr, size;
	int err, has_partial;

	CHECK_ERROR(CORPUS_ERROR_INVAL);
	
	// add a full suppression rule for the pattern
	if ((err = add_backsupp(f, pattern, BACKSUPP_FULL))) {
		goto out;
	}

	// add partial suppression rules for the internal ATerms
	has_partial = 0;
	attr = UTF8LITE_TEXT_BITS(pattern);
	utf8lite_text_iter_make(&it, pattern);
	while (utf8lite_text_iter_advance(&it)) {
		// find the next ATerm ('.')
		if (sent_break(it.current) != SENT_BREAK_ATERM) {
			continue;
		}

		// set the prefix
		size = (size_t)(it.ptr - pattern->ptr);
		prefix.ptr = pattern->ptr;
		prefix.attr = size | attr;

		// peek at the next character
		if (!utf8lite_text_iter_advance(&it)) {
			break;
		}

		// skip the prefix if not followed by a space
		if (sent_break(it.current) != SENT_BREAK_SP) {
			continue;
		}

		if ((err = add_backsupp(f, &prefix, BACKSUPP_PARTIAL))) {
			goto out;
		}
		has_partial = 1;
	}

	if (has_partial > 0) {
		err = add_fwdsupp(f, pattern);
	}

out:
	if (err) {
		f->error = err;
		corpus_log(err, "failed adding suppression to sentence filter");
	}

	return err;
}


int add_fwdsupp(struct corpus_sentfilter *f, const struct utf8lite_text *pattern)
{
	struct utf8lite_text_iter it;
	int *rules;
	int size, size0, nnode, nnode0;
	int code, id, prop, parent_id, err;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	// iterate over the characters in the pattern
	utf8lite_text_iter_make(&it, pattern);
	id = CORPUS_TREE_NONE;

	while (utf8lite_text_iter_advance(&it)) {
		code = (int)it.current;
		prop = sent_break(code);

		switch (prop) {
		case SENT_BREAK_EXTEND:
		case SENT_BREAK_FORMAT:
			// skip over extenders and format characters
			continue;

		case SENT_BREAK_SP:
			// replace spaces with ' '
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
		nnode0 = f->fwdsupp.nnode;
		size0 = f->fwdsupp.nnode_max;
		if ((err = corpus_tree_add(&f->fwdsupp, parent_id, code,
					   &id))) {
			goto out;
		}
		nnode = f->fwdsupp.nnode;

		// check whether a new node got added
		if (nnode0 < nnode) {
			// expand the rules array if necessary
			size = f->fwdsupp.nnode_max;
			if (size0 < size) {
				rules = f->fwdsupp_rules;
				rules = corpus_realloc(rules,
						       (size_t)size
						       * sizeof(*rules));
				if (!rules) {
					err = CORPUS_ERROR_NOMEM;
					goto out;
				}
				f->fwdsupp_rules = rules;
			}

			// mark the new intermediate node as none
			f->fwdsupp_rules[id] = FWDSUPP_NONE;
		}
	}

	// mark the terminal node with the rule
	if (id >= 0) {
		f->fwdsupp_rules[id] = FWDSUPP_FULL;
	}
	err = 0;

out:
	if (err) {
		f->error = err;
		corpus_log(err, "failed adding suppression to sentence filter");
	}
	return err;
}


int add_backsupp(struct corpus_sentfilter *f, const struct utf8lite_text *prefix,
		 int rule)
{
	struct utf8lite_text_iter it;
	int *rules;
	int size, size0, nnode, nnode0;
	int code, id, prop, parent_id, err;

	CHECK_ERROR(CORPUS_ERROR_INVAL);

	// iterate over the characters in the prefix, in reverse
	utf8lite_text_iter_make(&it, prefix);
	utf8lite_text_iter_skip(&it);
	id = CORPUS_TREE_NONE;

	while (utf8lite_text_iter_retreat(&it)) {
		code = (int)it.current;
		prop = sent_break(code);

		switch (prop) {
		case SENT_BREAK_EXTEND:
		case SENT_BREAK_FORMAT:
			// skip over extenders and format characters
			continue;

		case SENT_BREAK_SP:
			// replace spaces with ' '
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
						       (size_t)size
						       * sizeof(*rules));
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
	if (id >= 0 && f->backsupp_rules[id] != BACKSUPP_FULL) {
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
			    const struct utf8lite_text *text)
{
	CHECK_ERROR(CORPUS_ERROR_INVAL);

	corpus_sentscan_make(&f->scan, text, f->flags);
	f->current.ptr = NULL;
	f->current.attr = 0;
	f->has_scan = 1;

	return 0;
}


int corpus_sentfilter_advance(struct corpus_sentfilter *f)
{
	const struct utf8lite_text *text;
	const struct utf8lite_text *current;
	struct utf8lite_text_iter it;
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
	text = &f->scan.text;

	// set an iterator to the end of the sentence
	utf8lite_text_iter_make(&it, current);
	utf8lite_text_iter_skip(&it);

	// the following is a bit of a hack. we need an iterator that
	// can move across sentence boundaries. the call to 'iter_advance'
	// is to ensure that the first call to 'iter_retreat' returns the
	// last character in the sentence
	it.end = text->ptr + UTF8LITE_TEXT_SIZE(text);
	it.text_attr = text->attr;
	utf8lite_text_iter_advance(&it);

	if (!has_suppress(f, &it)) {
		f->current = *current;
		return 1;
	}

	ptr = current->ptr;
	size = UTF8LITE_TEXT_SIZE(current);
	attr = UTF8LITE_TEXT_BITS(current);

	while (corpus_sentscan_advance(&f->scan)) {
		current = &f->scan.current;
		size += UTF8LITE_TEXT_SIZE(current);
		attr |= UTF8LITE_TEXT_BITS(current);

		utf8lite_text_iter_make(&it, current);
		utf8lite_text_iter_skip(&it);

		// hack; see above
		it.end = text->ptr + UTF8LITE_TEXT_SIZE(text);
		it.text_attr = text->attr;
		utf8lite_text_iter_advance(&it);

		if (!has_suppress(f, &it)) {
			break;
		}
	}

	f->current.ptr = (uint8_t *)ptr;
	f->current.attr = size | attr;

	return 1;
}


int has_suppress(const struct corpus_sentfilter *f,
		 struct utf8lite_text_iter *it)
{
	struct utf8lite_text_iter it2;
	int32_t code;
	int id, parent_id, prop, skip_space, rule;
	
	if (f->scan.type != CORPUS_SENT_ATERM) {
		return 0;
	}

	if (f->backsupp.nnode == 0) {
		return 0;
	}

	skip_space = 1;
	rule = BACKSUPP_NONE;
	id = CORPUS_TREE_NONE;

	while (utf8lite_text_iter_retreat(it)) {
		code = it->current;
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
			if (!(f->flags & CORPUS_SENTSCAN_SPCRLF)) {
				goto boundary;
			}
			// fall through

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

		if (code == ' ' || code == '.') { // possible boundary
			if (rule == BACKSUPP_FULL) {
				return 1;
			} else if (rule == BACKSUPP_PARTIAL) {
				it2 = *it;
				if (has_fwdsupp(f, &it2)) {
					return 1;
				}
			}
		}

		parent_id = id;
		if (!corpus_tree_has(&f->backsupp, parent_id, code, &id)) {
			return 0;
		}

		rule = f->backsupp_rules[id];
	}

boundary:
	if (rule == BACKSUPP_FULL) {
		return 1;
	} else if (rule == BACKSUPP_PARTIAL) {
		return has_fwdsupp(f, it);
	} else {
		return 0;
	}
}


int has_fwdsupp(const struct corpus_sentfilter *f, struct utf8lite_text_iter *it)
{
	int code, id, parent_id, prop, rule;

	if (f->fwdsupp.nnode == 0) {
		return 0;
	}

	id = CORPUS_TREE_NONE;
	rule = FWDSUPP_NONE;

	while (utf8lite_text_iter_advance(it)) {
		code = (int)it->current;
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
			if (!(f->flags & CORPUS_SENTSCAN_SPCRLF)) {
				goto boundary;
			}
			// fall through
			
		case SENT_BREAK_SP:
			code = ' ';
			break;

		case SENT_BREAK_ATERM:
			code = '.';
			break;
		}

		if (code == ' ' || code == '.') { // possible boundary
			if (rule == FWDSUPP_FULL) {
				return 1;
			}
		}

		parent_id = id;
		if (!corpus_tree_has(&f->fwdsupp, parent_id, code, &id)) {
			return 0;
		}
		rule = f->fwdsupp_rules[id];
	}

boundary:
	return (rule == FWDSUPP_FULL) ? 1 : 0;
}
