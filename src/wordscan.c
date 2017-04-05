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
#include "wordscan.h"


void wordscan_make(struct wordscan *scan, const struct text *text)
{
	scan->text = *text;
	scan->end = text->ptr + TEXT_SIZE(text);
	wordscan_reset(scan);
}


void wordscan_reset(struct wordscan *scan)
{
	scan->ptr = scan->text.ptr;
	scan->current.ptr = NULL;
	scan->current.attr = 0;
	scan->type = WORD_NONE;
}


int wordscan_advance(struct wordscan *scan)
{
	(void)scan;
	return 0;
}
