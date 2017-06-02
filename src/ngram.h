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
 * Maximum n-gram width.
 */
#define CORPUS_NGRAM_WIDTH_MAX	127

/**
 * N-gram term set, all with the same width, and their weights.
 */
struct corpus_ngram_terms {
	struct corpus_table table;	/**< terms hash table */
	double *weights;		/**< term weights */
	int *type_ids;			/**< terms type ID data */
	int width;			/**< term width */
	int nitem;			/**< number of terms */
	int nitem_max;			/**< term array capacity */
};

/**
 * Iterator over a set of n-gram terms.
 */
struct corpus_ngram_iter {
	const struct corpus_ngram_terms *terms;	/**< the terms */
	const int *type_ids;	/**< the current term */
	double weight;		/**< the current term's weight */
	int index;		/**< the index in the iteration */
};

/**
 * N-gram frequency counter.
 */
struct corpus_ngram {
	struct corpus_ngram_terms *terms;	/**< the term sets */
	int *buffer;		/**< input buffer */
	int nbuffer;		/**< number of occupied spots in the buffer */
	int nbuffer_max;	/**< buffer capacity */
	int width;		/**< maximum term width */
};

/**
 * Initialize an n-gram frequency counter.
 *
 * \param ng the counter
 * \param width the maximum n-gram width to count
 *
 * \returns 0 on success
 */
int corpus_ngram_init(struct corpus_ngram *ng, int width);

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
 * Get the number of n-grams seen of a given width.
 *
 * \param ng the counter
 * \param width the width
 *
 * \returns the count
 */
int corpus_ngram_count(const struct corpus_ngram *ng, int width);


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
 * \params ng the counter
 *
 * \returns 0 on success
 */
int corpus_ngram_break(struct corpus_ngram *ng);


/**
 * Check whether an n-gram exists in the counter, and get its weight.
 *
 * \param ng the counter
 * \param type_ids the array of type IDs for the n-gram
 * \param width the width of the n-gram
 * \param weightptr if non-NULL, a location to store the n-gram's weight if
 * 	it exists
 *
 * \returns non-zero if the n-gram exists, zero otherwise
 */
int corpus_ngram_has(const struct corpus_ngram *ng, const int *type_ids,
		     int width, double *weightptr);

/**
 * Construct an iterator over the set of seen n-grams of a given width.
 *
 * \param it the iterator
 * \param ng the n-gram counter
 * \param width the n-gram width to iterate over.
 */
void corpus_ngram_iter_make(struct corpus_ngram_iter *it,
			    const struct corpus_ngram *ng, int width);

/**
 * Advance an n-gram iterator to the next term.
 *
 * \param it the iterator
 *
 * \returns non-zero if a next term exists, zero otherwise
 */
int corpus_ngram_iter_advance(struct corpus_ngram_iter *it);

#endif /* CORPUS_NGRAM_H */
