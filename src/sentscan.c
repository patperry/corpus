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

#include "text.h"
#include "unicode/sentbreakprop.h"
#include "sentscan.h"


void sentscan_make(struct sentscan *scan, const struct text *text)
{
	scan->text = *text;
	scan->text_attr = text->attr & ~TEXT_SIZE_MASK;

	text_iter_make(&scan->iter, text);
	sentscan_reset(scan);
}


#define SCAN() \
	do { \
		scan->current.attr |= scan->attr; \
		scan->ptr = scan->iter_ptr; \
		scan->code = scan->iter.current; \
		scan->attr = scan->iter.attr; \
		scan->prop = scan->iter_prop; \
		scan->iter_ptr = scan->iter.ptr; \
		if (text_iter_advance(&scan->iter)) { \
			scan->iter_prop = sent_break(scan->iter.current); \
		} else { \
			scan->iter_prop = -1; \
		} \
	} while (0)

// Ignore Format and Extend characters, except when they appear at the
// beginning of a region of text
//
// WB4: X (Extend | Format | ZWJ)* -> X
#define EXTEND() \
	do { \
		while (scan->iter_prop == SENT_BREAK_EXTEND \
				|| scan->iter_prop == SENT_BREAK_FORMAT) { \
			scan->attr |= scan->iter.attr; \
			scan->iter_ptr = scan->iter.ptr; \
			if (text_iter_advance(&scan->iter)) { \
				scan->iter_prop = \
					sent_break(scan->iter.current); \
			} else { \
				scan->iter_prop = -1; \
			} \
		} \
	} while (0)

#define MAYBE_EXTEND() \
	do { \
		switch (scan->prop) { \
		case SENT_BREAK_CR: \
		case SENT_BREAK_LF: \
		case SENT_BREAK_SEP: \
			break; \
		default: \
			EXTEND(); \
			break; \
		} \
	} while (0)

#define NEXT() \
	do { \
		SCAN(); \
		MAYBE_EXTEND(); \
	} while (0)


void sentscan_reset(struct sentscan *scan)
{
	scan->current.ptr = 0;
	scan->current.attr = 0;
	scan->type = SENT_NONE;

	text_iter_reset(&scan->iter);
	scan->ptr = scan->iter.ptr;

	if (text_iter_advance(&scan->iter)) {
		scan->code = scan->iter.current;
		scan->attr = scan->iter.attr;
		scan->prop = sent_break(scan->code);

		scan->iter_ptr = scan->iter.ptr;
		if (text_iter_advance(&scan->iter)) {
			scan->iter_prop = sent_break(scan->iter.current);
		} else {
			scan->iter_prop = -1;
		}
		MAYBE_EXTEND();
	} else {
		scan->code = 0;
		scan->attr = 0;
		scan->prop = -1;
		scan->iter_ptr = NULL;
		scan->iter_prop = -1;
	}
}


int sentscan_advance(struct sentscan *scan)
{
	scan->current.ptr = (uint8_t *)scan->ptr;
	scan->current.attr = 0;

	// Break at the start and end of text, unless the text is empty.
	if (scan->prop < 0) {
		scan->type = SENT_NONE;
		// WB2: Any + eot
		goto Break;
	}

	switch (scan->prop) {
	default:
		scan->type = SENT_OTHER;
		NEXT();
		goto Break;
	}

Break:
	scan->current.attr |= (scan->ptr - scan->current.ptr);
	return (scan->type == SENT_NONE) ? 0 : 1;
}
