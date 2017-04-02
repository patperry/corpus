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
#ifndef FILEBUF_H
#define FILEBUF_H

/**
 * \file filebuf.h
 *
 * File buffer, for holding portions of a file in memory.
 */

#include <stdint.h>

/**
 * A line in the file buffer.
 */
struct filebuf_line {
	const uint8_t *ptr;	/**<  pointer to the first byte in the line */
	size_t size;		/**< number of bytes in the line */
};

/**
 * File buffer, holding contiguous ranges of lines in a file. Successive
 * calls to filebuf_advance() update the #lines array and page through
 * all lines in the file.
 */
struct filebuf {
	char *file_name;	/**< file name */
	int fd;			/**< the file descriptor */
	uint64_t file_size;	/**< file size, in bytes */
	size_t page_size;	/**< memory page size, in bytes */

	void *map_addr;		/**< the memory-mapped address */
	size_t map_size;	/**< the memory map size */
	size_t map_pad;		/**< the amount of padding to skip at the
				  start of the map */
	int64_t map_offset;	/**< the file position */

	int64_t offset;		/**< the (0-based) index of the
				 first line in the buffer */

	struct filebuf_line *lines;
				/**< an array of lines in the buffer */
	int nline;		/**< the number of lines in the buffer */
	int nline_max;		/**< the capacity of the #lines array */
	int error;		/**< the error code from the last call
				  to filebuf_advance() */
};

/**
 * Initialize a buffer for the specified file.
 *
 * \param buf the buffer
 * \param file_name the file name
 *
 * \returns 0 on success
 */
int filebuf_init(struct filebuf *buf, const char *file_name);

/**
 * Release a buffer's resources.
 *
 * \param buf the buffer
 */
void filebuf_destroy(struct filebuf *buf);

/**
 * Advance the buffer to the next portion of the file.
 *
 * \param buf the buffer
 *
 * \returns 0 if already at the end of the file, or an error occurs. In
 * 	the event of an error, `buf->error` stores the error code.
 */
int filebuf_advance(struct filebuf *buf);

/**
 * Reset the buffer to the start of the file.
 *
 * \param buf the buffer
 */
void filebuf_reset(struct filebuf *buf);

#endif /* FILEBUF_H */
