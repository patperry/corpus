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
#include "unicode/sentbreakprop.h"
#include "error.h"
#include "text.h"
#include "tree.h"
#include "sentscan.h"
#include "sentfilter.h"


int corpus_sentfilter_init(struct corpus_sentfilter *f)
{
	int err;

	if ((err = corpus_tree_init(&f->suppress))) {
		goto error_suppress;
	}
	f->current.ptr = NULL;
	f->current.attr = 0;
	f->error = 0;
	return 0;

error_suppress:
	corpus_log(err, "failed initializing sentence filter");
	return err;
}


void corpus_sentfilter_destroy(struct corpus_sentfilter *f)
{
	corpus_tree_destroy(&f->suppress);
}


int corpus_sentfilter_suppress(struct corpus_sentfilter *f,
			       const struct corpus_text *pattern)
{
	struct corpus_text_iter it;
	int code, id, prop, parent_id, err;

	if ((err = corpus_tree_root(&f->suppress))) {
		goto error;
	}

	id = 0;

	// iterate over the characters in the pattern, in reverse
	corpus_text_iter_make(&it, pattern);
	corpus_text_iter_skip(&it);
	while (corpus_text_iter_retreat(&it)) {
		code = (int)it.current;
		prop = sent_break(code);

		if (prop == SENT_BREAK_SP) {
			// skip over spaces
			continue;
		}

		if (prop == SENT_BREAK_ATERM) {
			code = '.';
		}

		parent_id = id;
		if ((err = corpus_tree_add(&f->suppress, parent_id, code,
						&id))) {
			goto error;
		}

		// TODO: mark intermediate nodes as partial
	}
	// TODO: mark terminal nodes as full

	return 0;

error:
	corpus_log(err, "failed adding suppression to sentence filter");
	return err;
}


int corpus_sentfilter_start(struct corpus_sentfilter *f,
			    const struct corpus_text *text)
{
	corpus_sentscan_make(&f->scan, text);
	f->current.ptr = NULL;
	f->current.attr = 0;
	return 0;
}


int corpus_sentfilter_advance(struct corpus_sentfilter *f)
{
	int ans;
	ans = corpus_sentscan_advance(&f->scan);
	if (ans) {
		f->current = f->scan.current;
	} else {
		f->current.ptr = NULL;
		f->current.attr = 0;
	}
	return ans;
}
