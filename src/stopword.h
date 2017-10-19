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

#ifndef CORPUS_STOPWORD_H
#define CORPUS_STOPWORD_H

/**
 * \file stopword.h
 *
 * Stop word lists.
 */

#include <stdint.h>

/**
 * Get a list of stop words (common function words) encoded as
 * NULL-terminated UTF-8 strings.
 *
 * \param name the stop word list name
 * \param lenptr if non-NULL, a location to store the word list length
 *
 * \returns a NULL-terminated list of stop words for the given language,
 * 	or NULL if no stop word list exists for the given language
 */
const uint8_t **corpus_stopword_list(const char *name, int *lenptr);

/**
 * Get a list of the stop word list names.
 *
 * \returns a NULL-terminated array of list names
 */
const char **corpus_stopword_names(void);

#endif /* CORPUS_STOPWORD_H */
