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

struct text;

enum escape_type {
	ESCAPE_NONE = 0,
	ESCAPE_CONTROL = (1 << 0),
	ESCAPE_UTF8 = (1 << 1)
};

struct render {
	char *string;
	size_t length;
	int escape_flags;
	const char *tab;
	const char *newline;
	int indent;
};

/**
 * Initialize a new render object.
 *
 * \param r the render object
 *
 * \returns 0 on success
 */
int render_init(struct render *r);

/**
 * Release a render object's resources.
 *
 * \param r the render object
 */
void render_destroy(struct render *r);

/**
 * Reset the render object to the empty string, set the
 * escape_flags, tab, and newline strings to their defaults,
 * and set the indent level to 0.
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
 * Set the tab string.
 *
 * \param r the render object
 * \param tab the tab string
 *
 * \returns the old tab string
 */
const char *render_set_tab(struct render *r, const char *tab);

/**
 * Set the new line string.
 *
 * \param r the render object
 * \param tab the tab string
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
 *
 * \returns 0 on success
 */
int render_newlines(struct render *r, int nline);

/**
 * Render formatted text. If any render escape flags are set, filter
 * all text characters through the appropriate escaping.
 *
 * \param r the render object
 * \param format the format string
 *
 * \returns 0 on success
 */
int render_printf(struct render *r, const char *format, ...);

/**
 * Render a text object. If any render escape flags are set, filter
 * all text characters through the appropriate escaping.
 *
 * \param r the render object
 * \param text the text object
 *
 * \returns 0 on success
 */
int render_text(struct render *r, const char *text);

#endif /* RENDER_H */
