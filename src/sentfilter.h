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

#ifndef CORPUS_SENTFILTER_H
#define CORPUS_SENTFILTER_H

/**
 * \file sentfilter.h
 *
 * Sentence segmentation with break suppression rules.
 */

#include <stdint.h>

/**
 * Sentence filter, for segmenting a text into sentences.
 */
struct corpus_sentfilter {
	struct corpus_tree backsupp; /**< backward suppression prefixes
				       (`.srM`, `.va`, '.C-.J .va') */
	struct corpus_tree fwdsupp; /**< forward suppressions, those with
				      internal spaces (`av. J.-C.`) */
	int *backsupp_rules;	/**< rules for the backward suppressions
				  prefixes (None, Partial, or Full) */
	int *fwdsupp_rules;	/**< rules for the forward suppressions
				  (None or Full */
	struct corpus_sentscan scan; /**< current sentence scan */
	int flags;	/**< #corpus_sentscan_type flags */
	int has_scan;	/**< whether a scan is in progress */

	struct utf8lite_text current; /**< current sentence */
	int error;	/**< error code for the last failing operation */
};

/**
 * Get a list of sentence break suppressions (abbreviations ending in
 * full stops) encoded as NULL-terminated UTF-8 strings.
 *
 * \param name the suppression list name
 * \param lenptr if non-NULL, a location to store the suppression list length
 *
 * \returns a NULL-terminated list of suppressions for the given language,
 * 	or NULL if no suppression list exists for the given language
 */
const uint8_t **corpus_sentsuppress_list(const char *name, int *lenptr);

/**
 * Get a list of the suppression list names.
 *
 * \returns a NULL-terminated array of list names
 */
const char **corpus_sentsuppress_names(void);

/**
 * Initialize a new sentence filter.
 *
 * \param f the filter
 * \param flags a bit mask of #corpus_sentscan_type values specifying
 * 	the scanning behavior
 */
int corpus_sentfilter_init(struct corpus_sentfilter *f, int flags);

/**
 * Release a sentence filter's resources.
 *
 * \param f the filter
 */
void corpus_sentfilter_destroy(struct corpus_sentfilter *f);

/**
 * Remove all suppressions from a sentence filter and clear the current
 * scan, if one exists.
 *
 * \param f the filter
 */
void corpus_sentfilter_clear(struct corpus_sentfilter *f);

/**
 * Add a sentence break suppression to a sentence filter.
 *
 * \param f the filter
 * \param pattern the sentence suppression, typically an abbreviation
 * 	ending in a full stop like "Ms."
 *
 * \returns 0 on success
 */
int corpus_sentfilter_suppress(struct corpus_sentfilter *f,
			       const struct utf8lite_text *pattern);

/**
 * Start segmenting a text into sentences.
 *
 * \param f the filter
 * \param text the text to segment
 *
 * \returns 0 on success
 */
int corpus_sentfilter_start(struct corpus_sentfilter *f,
			    const struct utf8lite_text *text);

/**
 * Advance a sentence filter to the next sentence in the current text.
 *
 * \param f the filter
 *
 * \returns 0 if no next sentence exits or an error occurs, nonzero otherwise;
 * 	if an error occurs, sets `f->error` to the error code
 */
int corpus_sentfilter_advance(struct corpus_sentfilter *f);

#endif /* CORPUS_SENTFILTER_H */
