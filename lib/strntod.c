/*
 * Copyright (c) 1988-1993 The Regents of the University of California.
 * Copyright (c) 2017 Patrick O. Perry
 * All rights reserved.
 *
 * Derived from the source code for the "strtod" library procedure,
 * obtained from "tcl7.0/compat/strtod.c":
 *
 *     http://sourceforge.net/projects/tcl/files/Tcl/7.0/tcl7.0.tar.Z
 *
 * This version uses C99 headers, fixes some overflow bugs, and uses
 * the "long double" type for increased precision. It also allows
 * the string to be specified with a length, rather than relying on
 * the string to be null-terminated.
 *
 * The original license follows below:
 *
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF
 * CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 */

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>


static int MAX_EXPONENT = 441;	// Largest possible base 10 exponent.  Any
				// exponent "exp" larger than this will
				// overflow 5^("exp").

static double POWERS_OF_2[] = {		// Binary powers of 2. Entry is
	2.000000000000000000000e+00,	// is 2^(2^i). Used to convert
	4.000000000000000000000e+00,    // decimal exponents into floating-
	1.600000000000000000000e+01,	// point numbers.
	2.560000000000000000000e+02,
	6.553600000000000000000e+04,
	4.294967296000000000000e+09,
	1.844674407370955161600e+19,
	3.402823669209384634634e+38,
	1.157920892373161954236e+77
};

static long double POWERS_OF_5[] = {	// Binary powers of 5. Entry is
	5.0000000000000000000000e+00L,	// 5^(2^i).
	2.5000000000000000000000e+01L,
	6.2500000000000000000000e+02L,
	3.9062500000000000000000e+05L,
	1.5258789062500000000000e+11L,
	2.3283064365386962890625e+22L,
	5.4210108624275221700372e+44L,
	2.9387358770557187699218e+89L,
	8.6361685550944446253864e+178L
};


/**
 * Format:
 *
 * number = ws [ sign ] [ mant ] [ frac ] [ exp ]
 *
 * decimal-point = %x2E       ; .
 *
 * e = %x65 / %x45            ; e E
 *
 * exp = e [ sign ] 1*DIGIT
 *
 * frac = decimal-point *DIGIT
 *
 * mant = *DIGIT
 *
 * minus = %x2D               ; -
 *
 * plus = %x2B                ; +
 *
 * sign = minus / plus
 *
 * ws = *(
 * 	   %x20 /             ; Space
 * 	   %x09 /             ; Horizontal tab
 * 	   %x0A /             ; Line feed or New line
 * 	   %x0B /             ; Vertical tab
 * 	   %x0C /             ; Form feed or New page
 * 	   %x0D )             ; Carriage return
 *
 */
double corpus_strntod(const char *string, size_t maxlen, char **endPtr)
{
	bool hasDec, sign, expSign;
	long double ldblFraction, ldblExp5;
	double dblExp2, result;
	const char *p;
	const char *end = string + maxlen;
	char c;
	int exp;		/* Exponent read from "EX" field. */
	int fracExp;		/* Exponent that derives from the fractional
				 * part.  Under normal circumstances, it is
				 * the negative of the number of digits in F.
				 * However, if I is very long, the last digits
				 * of I get dropped (otherwise a long I with a
				 * large negative exponent could cause an
				 * unnecessary overflow on I alone).  In this
				 * case, fracExp is incremented one for each
				 * dropped digit. */
	int mantSize;		/* Number of digits in mantissa. */
	int decPt;		/* Number of mantissa digits BEFORE decimal
				 * point. */
	int i;
	const char *pExp;	/* Temporarily holds location of exponent
				 * in string. */

	/*
	 * Strip off leading blanks and check for a sign.
	 */

	p = string;
	while (p < end && isspace(*p)) {
		p += 1;
	}
	if (p < end && *p == '-') {
		sign = true;
		p += 1;
	} else {
		if (p < end && *p == '+') {
			p += 1;
		}
		sign = false;
	}

	/*
	 * Count the number of digits in the mantissa (including the decimal
	 * point), and also locate the decimal point.
	 */

	decPt = -1;
	hasDec = false;
	for (mantSize = 0;; mantSize += 1) {
		c = (p < end) ? *p : 0;
		if (!isdigit(c)) {
			if ((c != '.') || (decPt >= 0)) {
				break;
			}
			decPt = mantSize;
			hasDec = true;
		}
		p += 1;
	}

	/*
	 * Now suck up the digits in the mantissa.  If the mantissa has more
	 * than 18 digits, ignore the extras, since they can't affect the
	 * value anyway.
	 */

	pExp = p;
	p -= mantSize;
	if (!hasDec) {
		decPt = mantSize;
	} else {
		mantSize -= 1;	/* One of the digits was the point. */
	}
	if (mantSize > 18) {
		fracExp = decPt - 18;
		mantSize = 18;
	} else {
		fracExp = decPt - mantSize;
	}
	if (mantSize == 0) {
		ldblFraction = 0;
		p = string;
		goto done;
	} else {
		uint64_t frac;
		bool sawDec = false;

		/* skip leading zeros */
		if (mantSize == 18 && (*p == '0' || *p == '.')) {
			while (p + mantSize < pExp
					&& (*p == '0' || *p == '.')) {
				if (*p == '.') {
					sawDec = true;
				} else {
					fracExp -= 1;
				}
				p += 1;
			}
			if (p + mantSize == pExp && !sawDec && hasDec) {
				p -= 1;
				fracExp += 1;
			}
		}

		frac = 0;
		for (; mantSize > 0; mantSize -= 1) {
			c = *p;
			p += 1;
			if (c == '.') {
				c = *p;
				p += 1;
			}
			frac = 10 * frac + (unsigned)(c - '0');
		}
		ldblFraction = (long double)frac;
	}

	/*
	 * Skim off the exponent.
	 */

	p = pExp;
	exp = 0;
	expSign = false;
	if (p < end && ((*p == 'E') || (*p == 'e'))) {
		p += 1;
		if (p < end && *p == '-') {
			expSign = true;
			fracExp = -fracExp;
			p += 1;
		} else {
			if (p < end && *p == '+') {
				p += 1;
			}
		}
		if (p == end || !isdigit(*p)) {
			p = pExp;
			goto done;
		}
		while (p < end && isdigit(*p)) {
			/* prevent wrap-around; handle overflow later */
			if (exp <= ((INT_MAX - 9) / 10) - (fracExp / 10)) {
				exp = exp * 10 + (*p - '0');
			}
			p += 1;
		}
	}
	exp = exp + fracExp;
	if (expSign) {
		exp = -exp;
	}

	/*
	 * Generate a floating-point number that represents the exponent.
	 * Do this by processing the exponent one bit at a time to combine
	 * many powers of 2 of 10. Then combine the exponent with the
	 * fraction.
	 *
	 * The original code computed 10^exp directly, but this can overflow
	 * if sizeof(long double) == 8. Instead, we compute 2^exp and 5^exp
	 * separately. This does not lose any precision because (a) 2^exp
	 * can be represented exactly and (b) fraction / (2^exp) can be
	 * computed without error.
	 */

	if (ldblFraction != 0) {
		if (exp < 0) {
			expSign = true;
			exp = -exp;
		} else {
			expSign = false;
		}
		if (exp > MAX_EXPONENT) {
			exp = MAX_EXPONENT;
			errno = ERANGE;
		}
		dblExp2 = 1;
		ldblExp5 = 1;
		for (i = 0; exp != 0; exp >>= 1, i += 1) {
			if (exp & 0x01) {
				dblExp2 *= POWERS_OF_2[i];
				ldblExp5 *= POWERS_OF_5[i];
			}
		}
		if (expSign) {
			ldblFraction /= ldblExp5;
			ldblFraction /= (long double)dblExp2;
		} else {
			ldblFraction *= ldblExp5;
			ldblFraction *= (long double)dblExp2;
		}
	}

done:
	if (endPtr != NULL) {
		*endPtr = (char *)p;
	}

	if (ldblFraction > (long double)DBL_MAX) {
		result = HUGE_VAL;
		errno = ERANGE;
	} else {
		result = (double)ldblFraction;
		if (result == 0 && ldblFraction != 0) {
			errno = ERANGE;
		}
	}

	if (sign) {
		return -result;
	}
	return result;
}
