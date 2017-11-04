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

#include "../lib/utf8lite/src/utf8lite.h"
#include "unicode/sentbreakprop.h"
#include "sentscan.h"


void corpus_sentscan_make(struct corpus_sentscan *scan,
			  const struct utf8lite_text *text,
			  int flags)
{
	scan->text = *text;
	scan->flags = flags;

	utf8lite_text_iter_make(&scan->iter, text);
	corpus_sentscan_reset(scan);
}


#define SCAN() \
	do { \
		scan->ptr = scan->iter_ptr; \
		scan->code = scan->iter.current; \
		scan->prop = scan->iter_prop; \
		scan->iter_ptr = scan->iter.ptr; \
		if (utf8lite_text_iter_advance(&scan->iter)) { \
			scan->iter_prop = sent_break(scan->iter.current); \
		} else { \
			scan->iter_prop = -1; \
		} \
	} while (0)

// Ignore Format and Extend characters, except when they appear at the
// beginning of a region of text
//
// SB5: X (Extend | Format)* -> X
#define EXTEND() \
	do { \
		while (scan->iter_prop == SENT_BREAK_EXTEND \
				|| scan->iter_prop == SENT_BREAK_FORMAT) { \
			scan->iter_ptr = scan->iter.ptr; \
			if (utf8lite_text_iter_advance(&scan->iter)) { \
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
			if (scan->flags & CORPUS_SENTSCAN_SPCRLF) { \
				EXTEND(); \
			} \
			break; \
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


void corpus_sentscan_reset(struct corpus_sentscan *scan)
{
	scan->current.ptr = NULL;
	scan->current.attr = scan->iter.text_attr & ~UTF8LITE_TEXT_SIZE_MASK;
	scan->type = CORPUS_SENT_NONE;

	utf8lite_text_iter_reset(&scan->iter);
	scan->ptr = scan->iter.ptr;

	if (utf8lite_text_iter_advance(&scan->iter)) {
		scan->code = scan->iter.current;
		scan->prop = sent_break(scan->code);

		scan->iter_ptr = scan->iter.ptr;
		if (utf8lite_text_iter_advance(&scan->iter)) {
			scan->iter_prop = sent_break(scan->iter.current);
		} else {
			scan->iter_prop = -1;
		}
		MAYBE_EXTEND();
	} else {
		scan->code = 0;
		scan->prop = -1;
		scan->iter_ptr = NULL;
		scan->iter_prop = -1;
	}
	scan->at_end = 0;
}


static int has_future_lower(const struct corpus_sentscan *scan)
{
	struct utf8lite_text_iter iter;
	int prop, ret;

	if (scan->iter_prop < 0) {
		return 0;
	}

	prop = scan->iter_prop;
	iter = scan->iter;

	while (1) {
		switch (prop) {
		case SENT_BREAK_OLETTER:
		case SENT_BREAK_UPPER:
		case SENT_BREAK_SEP:
		case SENT_BREAK_STERM:
		case SENT_BREAK_ATERM:
			ret = 0;
			goto out;

		case SENT_BREAK_CR:
		case SENT_BREAK_LF:
			if (scan->flags & CORPUS_SENTSCAN_SPCRLF) {
				break;
			}
			ret = 0;
			goto out;

		case SENT_BREAK_LOWER:
			ret = 1;
			goto out;

		default:
			break;
		}

		if (utf8lite_text_iter_advance(&iter)) {
			prop = sent_break(iter.current);
		} else {
			ret = 0;
			goto out;
		}
	}

out:
	return ret;
}


int corpus_sentscan_advance(struct corpus_sentscan *scan)
{
	scan->current.ptr = (uint8_t *)scan->ptr;
	scan->current.attr = scan->iter.text_attr & ~UTF8LITE_TEXT_SIZE_MASK;
	scan->type = CORPUS_SENT_NONE;

NoBreak:

	// Break at the start and end of text, unless the text is empty.
	if (scan->prop < 0) {
		// WB2: Any + eot
		goto Break;
	}

	switch (scan->prop) {
	case SENT_BREAK_CR:
		NEXT();
		if (scan->flags & CORPUS_SENTSCAN_SPCRLF) {
			goto NoBreak;
		}
		goto CR;

	case SENT_BREAK_LF:
		NEXT();
		if (scan->flags & CORPUS_SENTSCAN_SPCRLF) {
			goto NoBreak;
		}
		goto ParaSep;

	case SENT_BREAK_SEP:
		NEXT();
		goto ParaSep;

	case SENT_BREAK_UPPER:
	case SENT_BREAK_LOWER:
		NEXT();
		goto UpperLower;

	case SENT_BREAK_ATERM:
		NEXT();
		goto ATerm;

	case SENT_BREAK_STERM:
		NEXT();
		goto STerm;

	default:
		NEXT();
		goto NoBreak;
	}

CR:
	if (scan->prop == SENT_BREAK_LF) {
		// Do not break within CRLF
		// SB3: CR * LF
		NEXT();
	}
	// fall through

ParaSep:
	// SB4: ParaSep +
	scan->type = CORPUS_SENT_PARASEP;
	goto Break;

UpperLower:
	if (scan->prop == SENT_BREAK_ATERM) {
		NEXT();
		goto UpperLower_ATerm;
	} else {
		goto NoBreak;
	}

UpperLower_ATerm:
	switch (scan->prop) {
	case SENT_BREAK_UPPER:
		// SB7: (Upper | Lower) ATerm * Upper
		NEXT();
		goto UpperLower;
	default:
		goto ATerm;
	}

ATerm:
	switch (scan->prop) {
	case SENT_BREAK_NUMERIC:
		// SB5: ATerm * Numeric
		NEXT();
		goto NoBreak;

	default:
		goto ATerm_Close;
	}

ATerm_Close:
	switch(scan->prop) {
	case SENT_BREAK_CLOSE:
		NEXT();
		goto ATerm_Close;
	
	default:
		goto ATerm_Close_Sp;
	}

ATerm_Close_Sp:
	switch (scan->prop) {
	case SENT_BREAK_SP:
		NEXT();
		goto ATerm_Close_Sp;

	case SENT_BREAK_CR:
		// SB9: SATerm Close* * (Close | Sp | ParaSep)
		NEXT();
		if (scan->flags & CORPUS_SENTSCAN_SPCRLF) {
			goto ATerm_Close_Sp;
		}
		goto CR;

	case SENT_BREAK_LF:
		NEXT();
		if (scan->flags & CORPUS_SENTSCAN_SPCRLF) {
			goto ATerm_Close_Sp;
		}
		goto ParaSep;

	case SENT_BREAK_SEP:
		// SB9: SATerm Close* * (Close | Sp | ParaSep)
		NEXT();
		goto ParaSep;

	case SENT_BREAK_OLETTER:
	case SENT_BREAK_UPPER:
		scan->type = CORPUS_SENT_ATERM;
		goto Break;

	case SENT_BREAK_LOWER:
		NEXT();
		goto UpperLower;

	case SENT_BREAK_SCONTINUE:
		NEXT();
		goto NoBreak;

	case SENT_BREAK_STERM:
		NEXT();
		goto STerm;

	case SENT_BREAK_ATERM:
		NEXT();
		goto ATerm;

	default:
		if (has_future_lower(scan)) {
			goto NoBreak;
		} else {
			scan->type = CORPUS_SENT_ATERM;
			goto Break;
		}
	}

STerm:
	goto STerm_Close;

STerm_Close:
	switch(scan->prop) {
	case SENT_BREAK_CLOSE:
		NEXT();
		goto STerm_Close;
	
	default:
		goto STerm_Close_Sp;
	}

STerm_Close_Sp:
	switch (scan->prop) {
	case SENT_BREAK_SP:
		NEXT();
		goto STerm_Close_Sp;

	case SENT_BREAK_CR:
		// SB9: SATerm Close* * (Close | Sp | ParaSep)
		NEXT();
		if (scan->flags & CORPUS_SENTSCAN_SPCRLF) {
			goto STerm_Close_Sp;
		}
		goto CR;

	case SENT_BREAK_LF:
		NEXT();
		if (scan->flags & CORPUS_SENTSCAN_SPCRLF) {
			goto STerm_Close_Sp;
		}
		goto ParaSep;

	case SENT_BREAK_SEP:
		NEXT();
		goto ParaSep;
		// SB9: SATerm Close* * (Close | Sp | ParaSep)

	case SENT_BREAK_SCONTINUE:
		NEXT();
		goto NoBreak;

	case SENT_BREAK_STERM:
		NEXT();
		goto STerm;

	case SENT_BREAK_ATERM:
		NEXT();
		goto ATerm;

	default:
		scan->type = CORPUS_SENT_STERM;
		goto Break;
	}

Break:
	scan->current.attr |= (size_t)(scan->ptr - scan->current.ptr);

	if (UTF8LITE_TEXT_SIZE(&scan->current) == 0) {
		if (UTF8LITE_TEXT_SIZE(&scan->text) == 0 && !scan->at_end) {
			scan->at_end = 1;
			return 1;
		} else {
			scan->at_end = 1;
			return 0;
		}
	} else {
		return 1;
	}
}
