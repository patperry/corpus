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
#include "error.h"
#include "text.h"
#include "unicode/wordbreakprop.h"
#include "wordscan.h"


void corpus_wordscan_make(struct corpus_wordscan *scan,
			  const struct corpus_text *text)
{
	scan->text = *text;
	scan->text_attr = text->attr & ~CORPUS_TEXT_SIZE_MASK;

	corpus_text_iter_make(&scan->iter, text);
	corpus_wordscan_reset(scan);
}

#define SCAN() \
	do { \
		scan->current.attr |= scan->attr; \
		scan->ptr = scan->iter_ptr; \
		scan->code = scan->iter.current; \
		scan->attr = scan->iter.attr; \
		scan->prop = scan->iter_prop; \
		scan->iter_ptr = scan->iter.ptr; \
		if (corpus_text_iter_advance(&scan->iter)) { \
			scan->iter_prop = word_break(scan->iter.current); \
		} else { \
			scan->iter_prop = WORD_BREAK_NONE; \
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
			scan->attr |= scan->iter.attr; \
			scan->iter_ptr = scan->iter.ptr; \
			if (corpus_text_iter_advance(&scan->iter)) { \
				scan->iter_prop = \
					word_break(scan->iter.current); \
			} else { \
				scan->iter_prop = WORD_BREAK_NONE; \
			} \
		} \
	} while (0)

#define MAYBE_EXTEND() \
	do { \
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

#define NEXT() \
	do { \
		SCAN(); \
		MAYBE_EXTEND(); \
	} while (0)


void corpus_wordscan_reset(struct corpus_wordscan *scan)
{
	scan->current.ptr = 0;
	scan->current.attr = 0;
	scan->type = CORPUS_WORD_NONE;

	corpus_text_iter_reset(&scan->iter);
	scan->ptr = scan->iter.ptr;

	if (corpus_text_iter_advance(&scan->iter)) {
		scan->code = scan->iter.current;
		scan->attr = scan->iter.attr;
		scan->prop = word_break(scan->code);

		scan->iter_ptr = scan->iter.ptr;
		if (corpus_text_iter_advance(&scan->iter)) {
			scan->iter_prop = word_break(scan->iter.current);
		} else {
			scan->iter_prop = WORD_BREAK_NONE;
		}
		MAYBE_EXTEND();
	} else {
		scan->code = 0;
		scan->attr = 0;
		scan->prop = WORD_BREAK_NONE;
		scan->iter_ptr = NULL;
		scan->iter_prop = WORD_BREAK_NONE;
	}
}


int corpus_wordscan_advance(struct corpus_wordscan *scan)
{
	scan->current.ptr = (uint8_t *)scan->ptr;
	scan->current.attr = 0;
	scan->type = CORPUS_WORD_NONE;

	switch ((enum word_break_prop)scan->prop) {
	case WORD_BREAK_NONE:
		// Break at the start and end of text unless the text is empty
		// WB2: Any + eot
		goto Break;

	case WORD_BREAK_CR:
		scan->type = CORPUS_WORD_SPACE;
		
		if (scan->iter_prop == WORD_BREAK_LF) {
			// Do not break within CRLF
			// WB3: CR * LF
			NEXT();
		}

		// Otherwise break after Newlines
		// WB3a: (Newline | CR | LF) +
		NEXT();
		goto Break;

	case WORD_BREAK_NEWLINE:
	case WORD_BREAK_LF:
		scan->type = CORPUS_WORD_SPACE;

		// Break after Newlines
		// WB3a: (Newline | LF) +
		NEXT();
		goto Break;

	case WORD_BREAK_ZWJ:
		scan->type = CORPUS_WORD_OTHER;

		if (scan->iter_prop == WORD_BREAK_GLUE_AFTER_ZWJ) {
			scan->type = CORPUS_WORD_SYMBOL;

			// Do not break within emoji zwj sequences
			// WB3c: ZWJ * (Glue_After_Zwj | EBG)
			NEXT();
			NEXT();
			goto Break;
		} else if (scan->iter_prop == WORD_BREAK_E_BASE_GAZ) {
			scan->type = CORPUS_WORD_SYMBOL;

			// WB3c: ZWJ * (Glue_After_Zwj | EBG)
			NEXT();
			NEXT();
			goto E_Base;
		}

		EXTEND();
		NEXT();
		goto Break;

	case WORD_BREAK_ALETTER:
		scan->type = CORPUS_WORD_LETTER;
		NEXT();
		goto ALetter;

	case WORD_BREAK_NUMERIC:
		scan->type = CORPUS_WORD_NUMBER;
		NEXT();
		goto Numeric;

	case WORD_BREAK_EXTENDNUMLET:
		scan->type = CORPUS_WORD_LETTER;
		NEXT();

		switch (scan->prop) {
			case WORD_BREAK_EXTENDNUMLET:
			case WORD_BREAK_ALETTER:
			case WORD_BREAK_HEBREW_LETTER:
				scan->type = CORPUS_WORD_LETTER;
				break;

			case WORD_BREAK_NUMERIC:
				scan->type = CORPUS_WORD_NUMBER;
				break;

			case WORD_BREAK_KATAKANA:
				scan->type = CORPUS_WORD_LETTER;
				break;

			default:
				break;
		}
		goto ExtendNumLet;

	case WORD_BREAK_HEBREW_LETTER:
		scan->type = CORPUS_WORD_LETTER;
		NEXT();
		goto Hebrew_Letter;

	case WORD_BREAK_KATAKANA:
		scan->type = CORPUS_WORD_LETTER;
		NEXT();
		goto Katakana;

	case WORD_BREAK_E_BASE:
	case WORD_BREAK_E_BASE_GAZ:
		scan->type = CORPUS_WORD_SYMBOL;
		NEXT();
		goto E_Base;

	case WORD_BREAK_REGIONAL_INDICATOR:
		scan->type = CORPUS_WORD_SYMBOL;
		NEXT();
		goto Regional_Indicator;

	case WORD_BREAK_LETTER:
		scan->type = CORPUS_WORD_LETTER;
		NEXT();
		goto Break;

	case WORD_BREAK_NUMBER:
		scan->type = CORPUS_WORD_NUMBER;
		NEXT();
		goto Break;

	case WORD_BREAK_DOUBLE_QUOTE:
	case WORD_BREAK_MIDLETTER:
	case WORD_BREAK_MIDNUM:
	case WORD_BREAK_MIDNUMLET:
	case WORD_BREAK_PUNCTUATION:
	case WORD_BREAK_SINGLE_QUOTE:
		scan->type = CORPUS_WORD_PUNCT;
		NEXT();
		goto Break;

	case WORD_BREAK_E_MODIFIER:
	case WORD_BREAK_GLUE_AFTER_ZWJ:
	case WORD_BREAK_SYMBOL:
		scan->type = CORPUS_WORD_SYMBOL;
		NEXT();
		goto Break;

	case WORD_BREAK_EXTEND:
	case WORD_BREAK_MARK:
		scan->type = CORPUS_WORD_MARK;
		NEXT();
		goto Break;

	case WORD_BREAK_FORMAT: // Cf format controls
	case WORD_BREAK_OTHER:
		scan->type = CORPUS_WORD_OTHER;
		NEXT();
		goto Break;

	case WORD_BREAK_WHITE_SPACE:
		scan->type = CORPUS_WORD_SPACE;
		NEXT();
		goto Break;
	}

	corpus_log(CORPUS_ERROR_INTERNAL,
		   "Unhandled word break property (%d)", scan->prop);
	assert(0);
	return 0;

ALetter:
	//fprintf(stderr, "ALetter: code = U+%04X\n", code);

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
	//fprintf(stderr, "Hebrew_Letter: code = U+%04X\n", code);

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
	//fprintf(stderr, "Numeric: code = U+%04X\n", code);

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
	//fprintf(stderr, "Katakana: code = U+%04X\n", code);

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

	//fprintf(stderr, "ExtendNumLet: code = U+%04X\n", code);

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
	//fprintf(stderr, "E_Base: code = U+%04X\n", code);

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

	//fprintf(stderr, "Regional_Indicator: code = U+%04X\n", code);

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
	scan->current.attr |= (size_t)(scan->ptr - scan->current.ptr);
	return (scan->ptr == scan->current.ptr) ? 0 : 1;
}
