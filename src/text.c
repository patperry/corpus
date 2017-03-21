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


int text_assign(struct text *text, const uint8_t *ptr, size_t size, int flags)
{
	text->ptr = NULL;
	text->attr = 0;

	(void)ptr;
	(void)size;
	(void)flags;
	return 0;
}


int text_unescape(const struct text *text, uint8_t *buf, size_t *sizeptr)
{
	(void)text;
	(void)buf;
	(void)sizeptr;
	return 0;
}
