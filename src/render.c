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
#include "text.h"
#include "render.h"

int render_init(struct render *r)
{
	r->string = NULL;
	r->length = 0;
	render_clear(r);
	return 0;
}

void render_destroy(struct render *r)
{
	(void)r;
}

void render_clear(struct render *r)
{
	if (r->string) {
		r->string[0] = '\0';
	}
	r->length = 0;
	r->escape_flags = 0;
	r->tab = "\t";
	r->newline = "\n";
	r->indent = 0;
}


int render_set_escape(struct render *r, int flags)
{
	int oldflags = r->escape_flags;
	r->escape_flags = flags;
	return oldflags;
}


const char *render_set_tab(struct render *r, const char *tab)
{
	const char *oldtab = r->tab;
	assert(tab);
	r->tab = tab;
	return oldtab;
}


const char *render_set_newline(struct render *r, const char *newline)
{
	const char *oldnewline = r->newline;
	assert(newline);
	r->newline = newline;
	return oldnewline;
}


void render_indent(struct render *r, int nlevel)
{
	r->indent += nlevel;
	assert(r->indent >= 0);
}


int render_newlines(struct render *r, int nline)
{
	(void)r;
	(void)nline;
	return 0;
}


int render_printf(struct render *r, const char *format, ...)
{
	(void)r;
	(void)format;
	return 0;
}


int render_text(struct render *r, const char *text)
{
	(void)r;
	(void)text;
	return 0;
}
