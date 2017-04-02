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

#include <stdint.h>

struct filebuf_line {
	const uint8_t *ptr;
	size_t size;
};

/**
 * \file filebuf.h
 *
 * File buffer, for holding portions of a file in memory.
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

	int64_t offset;

	struct filebuf_line *lines;
	int nline, nline_max;
	int error;
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

int filebuf_advance(struct filebuf *buf);
int filebuf_reset(struct filebuf *buf);

#endif /* FILEBUF_H */
