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

#ifndef RENDER_H
#define RENDER_H

/**
 * \file render.h
 *
 * Text rendering with pretty printing format controls.
 */

#include <stddef.h>
#include <stdint.h>

struct text;

/**
 * Values specifying render escape behavior. This allows replacing
 * control codes or non-ASCII Unicode characters with JSON-style
 * backslash (\) escapes.
 */
enum escape_type {
	ESCAPE_NONE = 0,		/**< no escaping */
	ESCAPE_CONTROL = (1 << 0),	/**< replace control codes
					  (C0, delete, and C1) with
					  JSON-style backslash escapes */
	ESCAPE_UTF8 = (1 << 1)		/**< replace non-ASCII UTF-8
					  characters with
					  JSON-style backslash escapes */
};

/**
 * Renderer, for printing objects as strings.
 */
struct render {
	char *string;		/**< the rendered string (null terminated) */
	int length;		/**< the length of the rendered string, not
				  including the null terminator */
	int length_max;		/**< the maximum capacity of the rendered
				  string before requiring reallocation, not
				  including the null terminator */
	int escape_flags;	/**< the flags, a bitmask of #escape_type
				 values, specifying escaping behavior */

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
 * \param escape_flags a bitmask of #escape_type flags specifying
 * 	escaping behavior
 *
 * \returns 0 on success
 */
int render_init(struct render *r, int escape_flags);

/**
 * Release a render object's resources.
 *
 * \param r the render object
 */
void render_destroy(struct render *r);

/**
 * Reset the render object to the empty string and set the indent level to 0.
 * Leave the escape flags, the tab, and the newline string at their current
 * values.
 *
 * \param r the render object
 */
void render_clear(struct render *r);

/**
 * Set the escaping behavior.
 *
 * \param r the render object
 * \param flags a bit mask of #escape_type values
 *
 * \returns the old escape flags
 */
int render_set_escape(struct render *r, int flags);

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
const char *render_set_tab(struct render *r, const char *tab);

/**
 * Set the new line string.  The client must not free the passed-in newline
 * string until either the render object is destroyed or a new newline
 * string gets set.
 *
 * \param r the render object
 * \param tab the tab string (null terminated)
 *
 * \returns the old tab string
 */
const char *render_set_newline(struct render *r, const char *newline);

/**
 * Increase or decrease the indent level.
 *
 * \param r the render object
 * \param nlevel the number of levels add or subtract to the indent
 */
void render_indent(struct render *r, int nlevel);

/**
 * Add new lines.
 *
 * \param r the render object
 * \param nline the number of new lines to add
 */
void render_newlines(struct render *r, int nline);

/**
 * Render a single character. If any render escape flags are set, filter
 * the character through the appropriate escaping.
 *
 * \param r the render object
 * \param ch the character (UTF-32)
 */
void render_char(struct render *r, uint32_t ch);

/**
 * Render a string. If any render escape flags are set, filter
 * all text characters through the appropriate escaping.
 *
 * \param r the render object
 * \param string the string, valid UTF-8
 */
void render_string(struct render *r, const char *str);

/**
 * Render formatted text. If any render escape flags are set, filter
 * all text characters through the appropriate escaping.
 *
 * \param r the render object
 * \param format the format string
 */
void render_printf(struct render *r, const char *format, ...);

/**
 * Render a text object. If any render escape flags are set, filter
 * all text characters through the appropriate escaping.
 *
 * \param r the render object
 * \param text the text object
 */
void render_text(struct render *r, const struct text *text);

#endif /* RENDER_H */