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


static int has_future_lower(const struct sentscan *scan)
{
	struct text_iter iter;
	int prop;

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
		case SENT_BREAK_CR:
		case SENT_BREAK_LF:
		case SENT_BREAK_STERM:
		case SENT_BREAK_ATERM:
			return 0;
		case SENT_BREAK_LOWER:
			return 1;
		default:
			break;
		}
		if (text_iter_advance(&iter)) {
			prop = sent_break(iter.current);
		} else {
			return 0;
		}
	}

	return 0;
}


int sentscan_advance(struct sentscan *scan)
{
	scan->current.ptr = (uint8_t *)scan->ptr;
	scan->current.attr = 0;
	scan->type = SENT_NONE;

NoBreak:

	// Break at the start and end of text, unless the text is empty.
	if (scan->prop < 0) {
		// WB2: Any + eot
		goto Break;
	}

	switch (scan->prop) {
	case SENT_BREAK_CR:
		NEXT();
		goto CR;

	case SENT_BREAK_LF:
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
	scan->type = SENT_NEWLINE;
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
		goto CR;

	case SENT_BREAK_LF:
	case SENT_BREAK_SEP:
		// SB9: SATerm Close* * (Close | Sp | ParaSep)
		NEXT();
		goto ParaSep;

	case SENT_BREAK_OLETTER:
	case SENT_BREAK_UPPER:
		scan->type = SENT_ATERM;
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
			scan->type = SENT_ATERM;
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
		goto CR;

	case SENT_BREAK_LF:
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
		scan->type = SENT_STERM;
		goto Break;
	}

Break:
	scan->current.attr |= (scan->ptr - scan->current.ptr);
	return (TEXT_SIZE(&scan->current) > 0) ? 1 : 0;
}
