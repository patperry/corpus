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

#ifndef CORPUS_NGRAM_H
#define CORPUS_NGRAM_H

/**
 * \file ngram.h
 *
 * N-gram frequency counter.
 */

/**
 * N-gram frequency counter.
 */
struct corpus_ngram {
	struct corpus_tree terms; /**< the seen n-gram terms */
	double *weights;	/**< term weights */
	int *buffer;		/**< input buffer */
	int nbuffer;		/**< number of occupied spots in the buffer */
	int nbuffer_max;	/**< buffer capacity */
	int length;		/**< maximum term length */
};

/**
 * An iterator over n-gram frequencies.
 */
struct corpus_ngram_iter {
	const struct corpus_ngram *ngram;	/**< parent collection */
	int *buffer;		/**< client-supplied buffer for storing IDs */
	const int *type_ids;	/**< current n-gram type IDS */
	int length;		/**< current n-gram length */
	double weight;		/**< current n-gram weight */
	int index;		/**< index in the iteration */
};

/**
 * Initialize an n-gram frequency counter.
 *
 * \param ng the counter
 * \param length the maximum n-gram length to count
 *
 * \returns 0 on success
 */
int corpus_ngram_init(struct corpus_ngram *ng, int length);

/**
 * Release an n-gram counter's resources.
 *
 * \param ng the counter
 */
void corpus_ngram_destroy(struct corpus_ngram *ng);

/**
 * Remove all n-grams from a counter, and clear the input buffer.
 */
void corpus_ngram_clear(struct corpus_ngram *ng);

/**
 * Add a type to the input buffer and update the counts for the new
 * n-grams.
 *
 * \param ng the counter
 * \param type_id the type
 * \param weight the weight to add to the new n-grams
 *
 * \returns 0 on success
 */
int corpus_ngram_add(struct corpus_ngram *ng, int type_id, double weight);

/**
 * Clear the n-gram input buffer.
 *
 * \param ng the counter
 *
 * \returns 0 on success
 */
int corpus_ngram_break(struct corpus_ngram *ng);

/**
 * Check whether an n-gram exists in the counter, and get its weight.
 *
 * \param ng the counter
 * \param type_ids the array of type IDs for the n-gram
 * \param length the length of the n-gram
 * \param weightptr if non-NULL, a location to store the n-gram's weight if
 * 	it exists
 *
 * \returns non-zero if the n-gram exists, zero otherwise
 */
int corpus_ngram_has(const struct corpus_ngram *ng, const int *type_ids,
		     int length, double *weightptr);

/**
 * Sort the n-gram terms into breadth-first order.
 *
 * \param ng the counter
 *
 * \returns 0 on success
 */
int corpus_ngram_sort(struct corpus_ngram *ng);

/**
 * Construct an iterator over the set of seen n-grams.
 *
 * \param it the iterator
 * \param ng the n-gram counter
 * \param buffer a buffer to store the type IDs, an array with
 * 	length at least equal to ngram length
 */
void corpus_ngram_iter_make(struct corpus_ngram_iter *it,
			    const struct corpus_ngram *ng,
			    int *buffer);

/**
 * Advance an n-gram iterator to the next term.
 *
 * \param it the iterator
 *
 * \returns non-zero if a next term exists, zero otherwise
 */
int corpus_ngram_iter_advance(struct corpus_ngram_iter *it);

#endif /* CORPUS_NGRAM_H */
