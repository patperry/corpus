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
#ifndef CORPUS_FILEBUF_H
#define CORPUS_FILEBUF_H

/**
 * \file filebuf.h
 *
 * File buffer, for holding a file in memory.
 */

#include <stdint.h>

/**
 * File buffer, holding a file in memory. Internally, we memory-map the
 * file, letting the operating system move the data from the hard disk
 * to main memory whenever necessary.
 */
struct corpus_filebuf {
	char *file_name;	/**< file name */
	intptr_t handle;	/**< the file descriptor or handle */
	uint64_t file_size;	/**< file size, in bytes */

	void *map_addr;		/**< the memory-mapped address */
	size_t map_size;	/**< the memory map size */
};

/**
 * A line in a file.
 */
struct corpus_filebuf_line {
	const uint8_t *ptr;	/**< the start of the line */
	size_t size;		/**< the size (in bytes) of the line */
};

/**
 * An iterator over the lines in a file. Lines include the trailing
 * newline (`\\n`), if it exists. (The last line in the file may or
 * may not end in a newline; all other lines do.)
 */
struct corpus_filebuf_iter {
	const uint8_t *begin;	/**< the beginning of the file */
	const uint8_t *ptr;	/**< the current position in the file */
	const uint8_t *end;	/**< the end of the file */

	struct corpus_filebuf_line current; /**< the current line */
};

/**
 * Initialize a buffer for the specified file.
 *
 * \param buf the buffer
 * \param file_name the file name
 *
 * \returns 0 on success
 */
int corpus_filebuf_init(struct corpus_filebuf *buf, const char *file_name);

/**
 * Release a buffer's resources.
 *
 * \param buf the buffer
 */
void corpus_filebuf_destroy(struct corpus_filebuf *buf);

/**
 * Get an iterator over the lines in a file.
 *
 * \param it the iterator to initialize
 * \param buf the file buffer
 */
void corpus_filebuf_iter_make(struct corpus_filebuf_iter *it,
			      const struct corpus_filebuf *buf);

/**
 * Advance an iterator to the next line in the file.
 *
 * \param it the iterator
 *
 * \returns nonzero if a next line exists, zero if at the end of the file
 */
int corpus_filebuf_iter_advance(struct corpus_filebuf_iter *it);

/**
 * Reset an iterator to the beginning of the file.
 *
 * \param it the iterator
 */
void corpus_filebuf_iter_reset(struct corpus_filebuf_iter *it);

#endif /* CORPUS_FILEBUF_H */
