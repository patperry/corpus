/*
 * Copyright 2016 Patrick O. Perry.
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

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

#include "array.h"
#include "errcode.h"
#include "xalloc.h"
#include "filebuf.h"

static int filebuf_init_lines(struct filebuf *file);
static void filebuf_destroy_lines(struct filebuf *file);
static void filebuf_view_clear(struct filebuf_view *view);


int filebuf_init(struct filebuf *file, const char *filename)
{
	struct stat stat;
	long page_size;
	int access, err;

	assert(filename);

	if (!(file->filename = xstrdup(filename))) {
		err = ERROR_NOMEM;
		syslog(LOG_ERR, "failed copying file name `%s'", filename);
		goto strdup_fail;
	}

	access = O_RDONLY;

	if ((file->fd = open(file->filename, access)) < 0) {
		err = ERROR_OS;
		syslog(LOG_ERR, "failed opening file `%s': %s",
		       file->filename, strerror(errno));
		goto open_fail;
	}

	if (fstat(file->fd, &stat) < 0) {
		err = ERROR_OS;
		syslog(LOG_ERR,
		       "failed determining size of file `%s': %s",
		       file->filename, strerror(errno));
		goto fstat_fail;
	}
	file->size = stat.st_size;
	syslog(LOG_DEBUG, "file has size %"PRIu64" bytes",
		(uint64_t)file->size);

	if ((page_size = sysconf(_SC_PAGESIZE)) < 0) {
		err = ERROR_OS;
		syslog(LOG_ERR, "failed determining memory page size: %s",
		       strerror(errno));
		goto sysconf_fail;
	}
	assert(page_size > 0);
	file->page_size = (size_t)page_size;

	if ((err = filebuf_init_lines(file))) {
		goto lines_fail;
	}
	return 0;

sysconf_fail:
lines_fail:
fstat_fail:
	close(file->fd);
open_fail:
	free(file->filename);
strdup_fail:
	syslog(LOG_ERR, "failed initializing file buffer");
	return err;
}


void filebuf_destroy(struct filebuf *file)
{
	filebuf_destroy_lines(file);
	close(file->fd);
	free(file->filename);
}


int filebuf_init_lines(struct filebuf *file)
{
	off_t *line_ends = NULL;
	int nline = 0;
	int nline_max = 0;

	const char *begin, *ptr, *end;
	void *addr, *addr_new, *base;
	size_t chunk, size;
	off_t off;
	char ch;
	int err, flags;

	// determine chunk size
	if ((uint64_t)file->size <= SIZE_MAX) {
		chunk = (size_t)file->size;
	} else {
		chunk = 4096 * file->page_size;
	}

	ch = '\n';
	off = 0;
	addr = NULL;

	while (off < file->size) {
		// map file segment to memory
		flags = MAP_SHARED;
		if (off != 0) {
			flags |= MAP_FIXED;
		}

		syslog(LOG_DEBUG, "mapping file segment at offset %"PRId64
		       ", length %zu", (int64_t)off, chunk);
		addr_new = mmap(addr, chunk, PROT_READ, flags, file->fd, off);
		if (addr_new == MAP_FAILED) {
			err = ERROR_OS;
			syslog(LOG_ERR, "failed %smapping file `%s': %s",
			       off == 0 ? "" : "re-", file->filename,
			       strerror(errno));

			if (off == 0) {
				goto error_mmap;
			} else {
				goto error_remmap;
			}
		} else {
			syslog(LOG_DEBUG, "mapped %zu bytes to address %p",
			       (size_t)chunk, addr_new);
			addr = addr_new;
		}

		// determine chunk size
		if (file->size - off < (off_t)chunk) {
			size = (size_t)(file->size - off);
		} else {
			size = chunk;
		}
		madvise(addr, size, MADV_SEQUENTIAL | MADV_WILLNEED);

		// scan for newlines
		begin = addr;
		ptr = begin;
		end = ptr + size;
		while (ptr != end) {
			ch = *ptr++;
			if (ch != '\n') {
				continue;
			}

			if (nline == nline_max) {
				//syslog(LOG_DEBUG, "growing lines array");
				base = line_ends;
				err = array_grow(&base, &nline_max,
						 sizeof(*line_ends), nline, 1);
				if (err) {
					goto error_array_grow;
				}
				line_ends = base;
			}
			line_ends[nline] = off + (ptr - begin);
			nline++;

			//syslog(LOG_DEBUG,
			//       "found line %zu ending at offset %"PRIu64,
			//       nline, (uint64_t)line_ends[nline - 1]);
		}

		// advance to the next chunk
		off += chunk;
	}
	if (ch != '\n') {
		syslog(LOG_WARNING,
		       "file does not end in newline; discarding last line");
	}

	// store the line_ends array, shrinking if possible
	file->line_ends = line_ends;
	file->nline = nline;
	if (nline < nline_max) {
		line_ends = realloc(line_ends, nline * sizeof(*line_ends));
		// if the realloc fails, keep the original array
		if (line_ends) {
			file->line_ends = line_ends;
		}
	}

	if (addr != NULL) {
		syslog(LOG_DEBUG, "unmapping %zu bytes from address %p",
		       chunk, addr);
		munmap(addr, chunk);
	}
	return 0;

error_array_grow:
error_remmap:
	syslog(LOG_DEBUG, "unmaping %zu bytes from address %p", chunk, addr);
	munmap(addr, chunk);
error_mmap:
	free(line_ends);
	syslog(LOG_ERR, "failed initializing line array");
	return err;
}



void filebuf_destroy_lines(struct filebuf *file)
{
	free(file->line_ends);
}


int filebuf_view_init(struct filebuf_view *view, const struct filebuf *file)
{
	view->file = file;
	view->addr = NULL;
	view->length = 0;
	filebuf_view_clear(view);
	return 0;
}


void filebuf_view_clear(struct filebuf_view *view)
{
	if (view->addr != NULL) {
		syslog(LOG_DEBUG, "unmapping %zu bytes from address %p",
		       view->length, view->addr);
		munmap(view->addr, view->length);
	}
	view->addr = NULL;
	view->length = 0;
	view->begin = NULL;
	view->end = NULL;
	view->lines_begin = 0;
	view->lines_end = 0;
}


void filebuf_view_destroy(struct filebuf_view *view)
{
	filebuf_view_clear(view);
}


int filebuf_view_set(struct filebuf_view *view, int lines_begin, int lines_end)
{
	const struct filebuf *file = view->file;
	void *addr;
	size_t pad, length;
	off_t offset, view_begin, view_end;
	int err, flags;

	assert(0 <= lines_begin);
	assert(lines_begin <= lines_end);
	assert(lines_end <= view->file->nline);

	// determine view begin
	if (lines_begin == 0) {
		view_begin = 0;
	} else {
		view_begin = file->line_ends[lines_begin - 1];
	}

	// determine view end
	if (lines_end == 0) {
		view_end = 0;
	} else {
		view_end = file->line_ends[lines_end - 1];
	}

	// align to page size
	pad = (size_t)(view_begin % (file->page_size));
	offset = view_begin - pad;

	// determine length
	if ((uint64_t)(view_end - offset) > (uint64_t)SIZE_MAX) {
		syslog(LOG_ERR, "file view size (%"PRIu64" bytes)"
		       " exceeds maximum (%zu bytes)",
		       (uint64_t)(view_end - offset), (size_t)SIZE_MAX);
		err = ERROR_OVERFLOW;
		goto error;
	}
	length = (size_t)(view_end - offset);

	// determine flags
	flags = MAP_SHARED;
	if (view->addr) {
	// if a mapping already exists
		if (view->length < length) {
		// remove the old mapping if it is too small
			filebuf_view_clear(view);
		} else {
		// otherwise, re-use the address space
			flags |= MAP_FIXED;
		}
	}

	// set up the mapping
	syslog(LOG_DEBUG, "mapping file segment at offset %"PRId64
		", size %zu", (int64_t)offset, length);
	addr = mmap(view->addr, length, PROT_READ, flags,
		    file->fd, offset);

	// check for failure
	if (addr == MAP_FAILED) {
		syslog(LOG_ERR, "failed %smapping view of file `%s': %s",
		       view->addr ? "re-" : "", file->filename,
		       strerror(errno));
		err = ERROR_OS;
		goto error;
	} else {
		syslog(LOG_DEBUG, "mapped %zu bytes to address %p", length,
		       addr);
	}

	view->addr = addr;
	view->length = length;
	view->begin = (const uint8_t *)addr + pad;
	view->end = (const uint8_t *)addr + length;
	view->lines_begin = lines_begin;
	view->lines_end = lines_end;
	return 0;

error:
	syslog(LOG_ERR, "failed setting file view of lines [%d, %d)",
	       lines_begin, lines_end);
	return err;
}
