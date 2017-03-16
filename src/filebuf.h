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

#define _FILE_OFFSET_BITS 64	/**< enable large file support */

#include <stddef.h>
#include <sys/types.h>

/**
 * File buffer, with information about the file size and line offsets.
 */
struct filebuf {
	char *filename;		/**< file name */
	off_t size;		/**< size of the file, in bytes */
	int fd;			/**< file descriptor */
	size_t page_size;	/**< memory page size */
	off_t *line_ends;	/**< line ending offsets */
	int nline;		/**< number of lines in the file */
};

/**
 * Memory view into a range of lines in a file.
 */
struct filebuf_view {
	const struct filebuf *file;	/**< underlying file */
	void *addr;			/**< memory buffer address */
	size_t length;			/**< memory buffer size */

	const uint8_t *begin;		/**< pointer to start of first line */
	const uint8_t *end;		/**< pointer to end of last line */
	int lines_begin;		/**< index of first line */
	int lines_end;			/**< index of last line */
};

/**
 * Initialize a buffer for the specified file.
 *
 * \param file the buffer
 * \param filename the name of the file
 *
 * \returns 0 on success
 */
int filebuf_init(struct filebuf *file, const char *filename);

/**
 * Release a buffer's resources. This should not get called until after
 * all views into the buffer have been destroyed via filebuf_view_destroy().
 *
 * \param file the buffer
 */
void filebuf_destroy(struct filebuf *file);

/**
 * Initialize a view into a file.
 *
 * \param view the view
 * \param file the file
 *
 * \returns 0 on success
 */
int filebuf_view_init(struct filebuf_view *view, const struct filebuf *file);

/**
 * Release a view's resources. This should get called before
 * filebuf_destroy().
 *
 * \param view the view
 */
void filebuf_view_destroy(struct filebuf_view *view);

/**
 * Reset the view to the given line range.
 *
 * \param view the view
 * \param lines_begin the first line in the range
 * \param lines_end the ending of the range
 *
 * \returns 0 on success
 */
int filebuf_view_set(struct filebuf_view *view, int lines_begin, int lines_end);

#endif /* FILEBUF_H */
