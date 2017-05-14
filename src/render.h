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

#ifndef CORPUS_RENDER_H
#define CORPUS_RENDER_H

/**
 * \file render.h
 *
 * Text rendering with pretty printing format controls.
 */

#include <stddef.h>
#include <stdint.h>

struct corpus_text;

/**
 * Values specifying render escape behavior. This allows replacing
 * control codes or non-ASCII Unicode characters with JSON-style
 * backslash (\) escapes.
 */
enum corpus_escape_type {
	CORPUS_ESCAPE_NONE = 0,			/**< no escaping */
	CORPUS_ESCAPE_CONTROL = (1 << 0),/**< replace control codes
					  (C0, delete, and C1) with
					  JSON-style backslash escapes */
	CORPUS_ESCAPE_UTF8 = (1 << 1)	/**< replace non-ASCII UTF-8
					  characters with
					  JSON-style backslash escapes */
};

/**
 * Renderer, for printing objects as strings.
 */
struct corpus_render {
	char *string;		/**< the rendered string (null terminated) */
	int length;		/**< the length of the rendered string, not
				  including the null terminator */
	int length_max;		/**< the maximum capacity of the rendered
				  string before requiring reallocation, not
				  including the null terminator */
	int escape_flags;	/**< the flags, a bitmask of
				  #corpus_escape_type values, specifying
				  escaping behavior */

	const char *tab;	/**< the tab string, for indenting */
	int tab_length;		/**< the length in bytes of the tab string,
				  not including the null terminator */

	const char *newline;	/**< the newline string, for advancing
				  to the next line */
	int newline_length;	/**< the length in bytes of the newline string,
				  not including the null terminator */

	int indent;		/**< the current indent level */
	int needs_indent;	/**< whether to indent before the next
				  character */
	int error;		/**< the code for the last error that
				  occurred, or zero if none */
};

/**
 * Initialize a new render object.
 *
 * \param r the render object
 * \param escape_flags a bitmask of #corpus_escape_type flags specifying
 * 	escaping behavior
 *
 * \returns 0 on success
 */
int corpus_render_init(struct corpus_render *r, int escape_flags);

/**
 * Release a render object's resources.
 *
 * \param r the render object
 */
void corpus_render_destroy(struct corpus_render *r);

/**
 * Reset the render object to the empty string and set the indent level to 0.
 * Leave the escape flags, the tab, and the newline string at their current
 * values.
 *
 * \param r the render object
 */
void corpus_render_clear(struct corpus_render *r);

/**
 * Set the escaping behavior.
 *
 * \param r the render object
 * \param flags a bit mask of #corpus_escape_type values
 *
 * \returns the old escape flags
 */
int corpus_render_set_escape(struct corpus_render *r, int flags);

/**
 * Set the tab string. The client must not free the passed-in tab
 * string until either the render object is destroyed or a new tab
 * string gets set.
 *
 * \param r the render object
 * \param tab the tab string (null terminated)
 *
 * \returns the old tab string
 */
const char *corpus_render_set_tab(struct corpus_render *r, const char *tab);

/**
 * Set the new line string.  The client must not free the passed-in newline
 * string until either the render object is destroyed or a new newline
 * string gets set.
 *
 * \param r the render object
 * \param newline the newline string (null terminated)
 *
 * \returns the old newline string
 */
const char *corpus_render_set_newline(struct corpus_render *r,
				      const char *newline);

/**
 * Increase or decrease the indent level.
 *
 * \param r the render object
 * \param nlevel the number of levels add or subtract to the indent
 */
void corpus_render_indent(struct corpus_render *r, int nlevel);

/**
 * Add new lines.
 *
 * \param r the render object
 * \param nline the number of new lines to add
 */
void corpus_render_newlines(struct corpus_render *r, int nline);

/**
 * Render a single character. If any render escape flags are set, filter
 * the character through the appropriate escaping.
 *
 * \param r the render object
 * \param ch the character (UTF-32)
 */
void corpus_render_char(struct corpus_render *r, uint32_t ch);

/**
 * Render a string. If any render escape flags are set, filter
 * all text characters through the appropriate escaping.
 *
 * \param r the render object
 * \param str the string, valid UTF-8
 */
void corpus_render_string(struct corpus_render *r, const char *str);

/**
 * Render formatted text. If any render escape flags are set, filter
 * all text characters through the appropriate escaping.
 *
 * \param r the render object
 * \param format the format string
 */
void corpus_render_printf(struct corpus_render *r, const char *format, ...)
#if (defined(_WIN32) || defined(_WIN64))
	;
#else
	__attribute__ ((format (printf, 2, 3)));
#endif

/**
 * Render a text object. If any render escape flags are set, filter
 * all text characters through the appropriate escaping.
 *
 * \param r the render object
 * \param text the text object
 */
void corpus_render_text(struct corpus_render *r,
			const struct corpus_text *text);

#endif /* CORPUS_RENDER_H */
