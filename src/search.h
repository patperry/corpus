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

#ifndef CORPUS_SEARCH_H
#define CORPUS_SEARCH_H

struct corpus_search_buffer {
	struct corpus_text *tokens;
	int *type_ids;
	int size;
	int size_max;
};

struct corpus_search {
	struct corpus_termset terms;
	struct corpus_filter *filter;
	struct corpus_search_buffer buffer;
	int length_max;

	struct corpus_text current;
	int term_id;
	int length;
	int error;
};

int corpus_search_init(struct corpus_search *search);
void corpus_search_destroy(struct corpus_search *search);

int corpus_search_add(struct corpus_search *search,
		      int *type_ids, int length, int *idptr);
int corpus_search_has(const struct corpus_search *search,
		      int *type_ids, int length, int *idptr);

int corpus_search_start(struct corpus_search *search,
			const struct corpus_text *text,
			struct corpus_filter *filter);
int corpus_search_advance(struct corpus_search *search);

#endif /* CORPUS_SEARCH_H */
