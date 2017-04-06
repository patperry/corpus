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
#include "unicode/wordbreakprop.h"
#include "wordscan.h"


void wordscan_make(struct wordscan *scan, const struct text *text)
{
	scan->text = *text;
	scan->text_attr = text->attr & ~TEXT_SIZE_MASK;

	text_iter_make(&scan->iter, text);
	wordscan_reset(scan);
}


#define SCAN() \
	do { \
		scan->current.attr |= scan->attr; \
		scan->ptr = scan->iter_ptr; \
		if (scan->prop < 0) { \
			goto Break; \
		} \
		code = scan->code; \
		prop = scan->prop; \
		scan->code = scan->iter.current; \
		scan->attr = scan->iter.attr; \
		scan->prop = scan->iter_prop; \
		scan->iter_ptr = scan->iter.ptr; \
		if (text_iter_advance(&scan->iter)) { \
			scan->iter_prop = word_break(scan->iter.current); \
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
		while (scan->iter_prop == WORD_BREAK_EXTEND \
				|| scan->iter_prop == WORD_BREAK_FORMAT \
				|| scan->iter_prop == WORD_BREAK_ZWJ) { \
			/* syslog(LOG_DEBUG, \
				  "Extend: code = U+%04X", next); */ \
			scan->attr |= scan->iter.attr; \
			scan->iter_ptr = scan->iter.ptr; \
			if (text_iter_advance(&scan->iter)) { \
				scan->iter_prop = word_break(scan->iter.current); \
			} else { \
				scan->iter_prop = -1; \
			} \
		} \
	} while (0)


#define NEXT() \
	do { \
		SCAN(); \
		switch (scan->prop) { \
		case WORD_BREAK_CR: \
		case WORD_BREAK_LF: \
		case WORD_BREAK_NEWLINE: \
		case WORD_BREAK_ZWJ: \
			break; \
		default: \
			EXTEND(); \
			break; \
		} \
	} while (0)


void wordscan_reset(struct wordscan *scan)
{
	scan->current.ptr = 0;
	scan->current.attr = 0;
	scan->type = WORD_NONE;

	text_iter_reset(&scan->iter);
	scan->ptr = scan->iter.ptr;

	if (text_iter_advance(&scan->iter)) {
		scan->code = scan->iter.current;
		scan->attr = scan->iter.attr;
		scan->prop = word_break(scan->code);

		scan->iter_ptr = scan->iter.ptr;
		if (text_iter_advance(&scan->iter)) {
			scan->iter_prop = word_break(scan->iter.current);
		} else {
			scan->iter_prop = -1;
		}
	} else {
		scan->code = 0;
		scan->attr = 0;
		scan->prop = -1;
		scan->iter_ptr = NULL;
		scan->iter_prop = -1;
	}
}


int wordscan_advance(struct wordscan *scan)
{
	uint32_t code;
	int prop;

	scan->current.ptr = (uint8_t *)scan->ptr;
	scan->current.attr = 0;
	scan->type = WORD_NONE;

	switch (scan->prop) {
	case WORD_BREAK_CR:
		scan->type = WORD_NEWLINE;

		// Do not break within CRLF
		// WB3: CR * LF
		if (scan->iter_prop == WORD_BREAK_LF) {
			SCAN();
		}
		// Otherwise break after Newlines
		// WB3a: (Newline | CR | LF) +
		SCAN();
		goto Break;

	case WORD_BREAK_NEWLINE:
	case WORD_BREAK_LF:
		// Break after Newlines
		// WB3a: (Newline | LF) +
		scan->type = WORD_NEWLINE;
		SCAN();
		goto Break;

	case WORD_BREAK_ZWJ:
		scan->type = WORD_ZWJ;

		switch (scan->iter_prop) {
			case WORD_BREAK_GLUE_AFTER_ZWJ:
			// Do not break within emoji zwj sequences
			// WB3c: ZWJ * (Glue_After_Zwj | EBG)
			SCAN();
			SCAN();
			goto Break;

		case WORD_BREAK_E_BASE_GAZ:
			// WB3c: ZWJ * (Glue_After_Zwj | EBG)
			SCAN();
			SCAN();
			goto E_Base;

		default:
			EXTEND();
			SCAN();
			goto Break;
		}

	default:
		break;
	}

	EXTEND();
	NEXT();

	switch (prop) {
	case WORD_BREAK_ALETTER:
		scan->type = WORD_ALETTER;
		goto ALetter;

	case WORD_BREAK_NUMERIC:
		scan->type = WORD_NUMERIC;
		goto Numeric;

	case WORD_BREAK_EXTENDNUMLET:
		scan->type = WORD_EXTEND;
		goto ExtendNumLet;

	case WORD_BREAK_HEBREW_LETTER:
		scan->type = WORD_HEBREW;
		goto Hebrew_Letter;

	case WORD_BREAK_KATAKANA:
		scan->type = WORD_KATAKANA;
		goto Katakana;

	case WORD_BREAK_E_BASE:
	case WORD_BREAK_E_BASE_GAZ:
		scan->type = WORD_EBASE;
		goto E_Base;

	case WORD_BREAK_REGIONAL_INDICATOR:
		scan->type = WORD_REGIONAL;
		goto Regional_Indicator;

	default:
		scan->type = WORD_OTHER;
		goto Break;
	}


ALetter:
	//syslog(LOG_DEBUG, "ALetter: code = U+%04X", code);

	switch (scan->prop) {
	case WORD_BREAK_ALETTER:
		// Do not break between most letters
		// WB5: AHLetter * AHLetter
		NEXT();
		goto ALetter;

	case WORD_BREAK_HEBREW_LETTER:
		// WB5: AHLetter * AHLetter
		NEXT();
		goto Hebrew_Letter;

	case WORD_BREAK_MIDLETTER:
	case WORD_BREAK_MIDNUMLET:
	case WORD_BREAK_SINGLE_QUOTE:
		// Do not break across certain punctuation

		// WB6: AHLetter * (MidLetter | MidNumLetQ) AHLetter

		if (scan->iter_prop == WORD_BREAK_ALETTER) {
			// WB7: AHLetter (MidLetter | MidNumLetQ) * AHLetter
			NEXT();
			NEXT();
			goto ALetter;
		} else if (scan->iter_prop == WORD_BREAK_HEBREW_LETTER) {
			// WB7:  AHLetter (MidLetter | MidNumLetQ) * AHLetter
			NEXT();
			NEXT();
			goto Hebrew_Letter;
		}

		// Otherwise break everywhere
		// WB14: Any + Any
		goto Break;

	case WORD_BREAK_NUMERIC:
		// Do not break within sequences of digits, or digits
		// adjacent to letters (“3a”, or “A3”).
		// WB9: AHLetter * Numeric
		NEXT();
		goto Numeric;

	case WORD_BREAK_EXTENDNUMLET:
		// Do not break from extenders
		// WB13a: AHLetter * ExtendNumLet
		NEXT();
		goto ExtendNumLet;

	default:
		goto Break;
	}


Hebrew_Letter:
	//syslog(LOG_DEBUG, "Hebrew_Letter: code = U+%04X", code);

	switch (scan->prop) {
	case WORD_BREAK_ALETTER:
		// Do not break between most letters
		// WB5: AHLetter * AHLetter
		NEXT();
		goto ALetter;

	case WORD_BREAK_HEBREW_LETTER:
		// WB5: AHLetter * AHLetter
		NEXT();
		goto Hebrew_Letter;

	case WORD_BREAK_MIDLETTER:
	case WORD_BREAK_MIDNUMLET:
	case WORD_BREAK_SINGLE_QUOTE:
		// Do not break across certain punctuation

		// WB6: AHLetter * (MidLetter | MidNumLetQ) * AHLetter
		if (scan->iter_prop == WORD_BREAK_HEBREW_LETTER) {
			// WB7:
			// AHLetter (MidLetter | MidNumLetQ) * AHLetter
			NEXT();
			NEXT();
			goto Hebrew_Letter;
		}
		if (scan->iter_prop == WORD_BREAK_ALETTER) {
			// WB7:
			// AHLetter (MidLetter | MidNumLetQ) * AHLetter
			NEXT();
			NEXT();
			goto ALetter;
		}
		if (scan->prop == WORD_BREAK_SINGLE_QUOTE) {
			// WB7a: Hebrew_Letter * Single_Quote
			NEXT();
		}
		goto Break;

	case WORD_BREAK_DOUBLE_QUOTE:
		// WB7b: Hebrew_Letter * Double_Quote Hebrew_Letter
		if (scan->iter_prop == WORD_BREAK_HEBREW_LETTER) {
			// Wb7c:
			//   Hebrew_Letter Double_Quote * Hebrew_Letter
			NEXT();
			NEXT();
			goto Hebrew_Letter;
		}
		goto Break;

	case WORD_BREAK_NUMERIC:
		// WB9: AHLetter * Numeric
		NEXT();
		goto Numeric;

	case WORD_BREAK_EXTENDNUMLET:
		// WB13a: AHLetter * ExtendNumLet
		NEXT();
		goto ExtendNumLet;

	default:
		goto Break;
	}

Numeric:
	//syslog(LOG_DEBUG, "Numeric: code = U+%04X", code);

	switch (scan->prop) {
	case WORD_BREAK_NUMERIC:
		// WB8: Numeric * Numeric
		NEXT();
		goto Numeric;

	case WORD_BREAK_MIDNUMLET:
	case WORD_BREAK_SINGLE_QUOTE:
	case WORD_BREAK_MIDNUM:
		// WB12: Numeric * (MidNum | MidNumLetQ) Numeric
		if (scan->iter_prop == WORD_BREAK_NUMERIC) {
			// WB11: Numeric (MidNum|MidNumLeqQ) * Numeric
			NEXT();
			NEXT();
			goto Numeric;
		}
		goto Break;

	case WORD_BREAK_EXTENDNUMLET:
		// WB13a: Numeric * ExtendNumLet
		NEXT();
		goto ExtendNumLet;

	case WORD_BREAK_ALETTER:
		// WB10: Numeric * AHLetter
		NEXT();
		goto ALetter;

	case WORD_BREAK_HEBREW_LETTER:
		// WB10: Numeric * AHLetter
		NEXT();
		goto Hebrew_Letter;

	default:
		goto Break;
	}

Katakana:
	//syslog(LOG_DEBUG, "Katakana: code = U+%04X", code);

	switch (scan->prop) {
	case WORD_BREAK_KATAKANA:
		// WB13: Katakana * Katakana
		NEXT();
		goto Katakana;

	case WORD_BREAK_EXTENDNUMLET:
		// WB13a: Katakana * ExtendNumLet
		NEXT();
		goto ExtendNumLet;

	default:
		goto Break;
	}

ExtendNumLet:

	//syslog(LOG_DEBUG, "ExtendNumLet: code = U+%04X", code);

	switch (scan->prop) {
	case WORD_BREAK_ALETTER:
		// WB13b: ExtendNumLet * AHLetter
		NEXT();
		goto ALetter;

	case WORD_BREAK_NUMERIC:
		// WB13b: ExtendNumLet * Numeric
		NEXT();
		goto Numeric;

	case WORD_BREAK_EXTENDNUMLET:
		// WB13a: ExtendNumLet * ExtendNumLet
		NEXT();
		goto ExtendNumLet;

	case WORD_BREAK_HEBREW_LETTER:
		// WB13b: ExtendNumLet * AHLetter
		NEXT();
		goto Hebrew_Letter;

	case WORD_BREAK_KATAKANA:
		// WB13c: ExtendNumLet * Katakana
		NEXT();
		goto Katakana;

	default:
		goto Break;
	}

E_Base:
	//syslog(LOG_DEBUG, "E_Base: code = U+%04X", code);

	switch (scan->prop) {
	case WORD_BREAK_E_MODIFIER:
		// Do not break within emoji modifier sequences
		// WB14: (E_Base | EBG) * E_Modifier
		NEXT();
		goto Break;

	default:
		goto Break;
	}

Regional_Indicator:

	//syslog(LOG_DEBUG, "Regional_Indicator: code = U+%04X", code);

	// Do not break within emoji flag sequences. That is, do not break
	// between regional indicator (RI) symbols if there is an odd number
	// of RI characters before the break point

	switch (scan->prop) {
	case WORD_BREAK_REGIONAL_INDICATOR:
		// WB15/16: [^RI] RI * RI
		NEXT();
		goto Break;

	default:
		// WB15/16: [^RI] RI * RI
		goto Break;
	}

Break:
	scan->current.attr |= (scan->ptr - scan->current.ptr);
	return (scan->type == WORD_NONE) ? 0 : 1;
}
