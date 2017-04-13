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

#define _FILE_OFFSET_BITS 64	// enable large file support

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "array.h"
#include "error.h"
#include "xalloc.h"
#include "filebuf.h"


static int filebuf_grow_lines(struct filebuf *buf, int nadd)
{
	void *base = buf->lines;
	int size = buf->nline_max;
	int err;

	if ((err = array_grow(&base, &size, sizeof(*buf->lines),
			      buf->nline, nadd))) {
		logmsg(err, "failed allocating lines array");
		return err;
	}

	buf->lines = base;
	buf->nline_max = size;
	return 0;
}


int filebuf_init(struct filebuf *buf, const char *file_name)
{
	struct stat stat;
	long page_size;
	int access, err;

	assert(file_name);

	if (!(buf->file_name = xstrdup(file_name))) {
		err = ERROR_NOMEM;
		logmsg(err, "failed copying file name (%s)", file_name);
		goto strdup_fail;
	}

	access = O_RDONLY;

	if ((buf->fd = open(buf->file_name, access)) < 0) {
		err = ERROR_OS;
		logmsg(err, "failed opening file (%s): %s",
		       buf->file_name, strerror(errno));
		goto open_fail;
	}

	if (fstat(buf->fd, &stat) < 0) {
		err = ERROR_OS;
		logmsg(err, "failed determining size of file (%s): %s",
		       buf->file_name, strerror(errno));
		goto fstat_fail;
	}
	buf->file_size = (uint64_t)stat.st_size;
	//fprintf(stderr, "file has size %"PRIu64" bytes\n", buf->file_size);

	if ((page_size = sysconf(_SC_PAGESIZE)) < 0) {
		err = ERROR_OS;
		logmsg(err, "failed determining memory page size: %s",
		       strerror(errno));
		goto sysconf_fail;
	}
	assert(page_size > 0);
	buf->page_size = (size_t)page_size;

	buf->map_addr = NULL;
	buf->map_size = 0;
	buf->map_pad = 0;
	buf->map_offset = 0;

	buf->lines = NULL;
	buf->nline = 0;
	buf->nline_max = 0;
	buf->offset = -1;

	err = 0;
	goto out;

sysconf_fail:
fstat_fail:
	close(buf->fd);
open_fail:
	free(buf->file_name);
strdup_fail:
	logmsg(err, "failed initializing file buffer");
out:
	buf->error = err;
	return err;
}


void filebuf_destroy(struct filebuf *buf)
{
	if (buf->map_addr) {
		//fprintf(stderr, "unmaping %"PRIu64" bytes from address %p\n",
		//        (uint64_t)buf->map_size, buf->map_addr);

		munmap(buf->map_addr, buf->map_size);
	}

	free(buf->lines);
	close(buf->fd);
	free(buf->file_name);
}


void filebuf_reset(struct filebuf *buf)
{
	buf->nline = 0;
	buf->offset = -1;

	if (buf->map_offset != 0) {
		//fprintf(stderr, "unmapping %"PRIu64" bytes from address %p\n",
		//	  (uint64_t)buf->map_size, buf->map_addr);

		munmap(buf->map_addr, buf->map_size);
		buf->map_addr = NULL;
		buf->map_size = 0;
		buf->map_offset = 0;
		buf->map_pad = 0;
	}

	buf->error = 0;
}


int filebuf_advance(struct filebuf *buf)
{
	void *addr;
	const struct filebuf_line *prev;
	const uint8_t *begin, *ptr, *end;
	size_t pad, size, length;
	off_t offset, start;
	int flags, eof, err;
	uint8_t ch;

	if (buf->nline > 0) {
		buf->offset += buf->nline;
		prev = &buf->lines[buf->nline - 1];
		start = (buf->map_offset
				+ (prev->ptr + prev->size
					- (uint8_t *)buf->map_addr));
		pad = (size_t)(start % buf->page_size);
		offset = start - pad;
		size = buf->map_size;
		flags = MAP_SHARED | MAP_FIXED;
	} else if (buf->offset == -1 && buf->map_size == 0) {
		// start a new map
		buf->offset = 0; // start a new map

		pad = 0;
		offset = 0;
		if (buf->file_size <= SIZE_MAX) {
			size = buf->file_size;
		} else {
			size = 4096 * buf->page_size;
		}
		flags = MAP_SHARED;
	} else if (buf->offset == -1 && buf->map_size > 0) {
		// use an existing map, ignoring the old padding

		assert(buf->map_offset == 0);

		buf->offset = 0;
		pad = 0;
		offset = buf->map_offset;
		size = buf->map_size;
		goto map_ready;
	} else {
		// there is an existing map, but it isn't big enough
		offset = buf->map_offset;
		pad = buf->map_pad;
		if (buf->map_size < SIZE_MAX / 2) {
			size = 2 * buf->map_size;
		} else if (buf->map_size != SIZE_MAX) {
			size = SIZE_MAX;
		} else {
			err = ERROR_OVERFLOW;
			logmsg(err, "file line size exceeds maximum"
			       " (%"PRIu64" bytes)",
			       (uint64_t)(SIZE_MAX - buf->page_size + 1));
			goto error;
		}

		//fprintf(stderr, "unmapping %"PRIu64" bytes from address %p\n",
		//        (uint64_t)buf->map_size, buf->map_addr);

		munmap(buf->map_addr, buf->map_size);
		buf->map_addr = NULL;
		flags = MAP_SHARED;
	}

	//fprintf(stderr, "mapping file segment at offset %"PRIu64
	//	  ", length %"PRIu64"\n", (uint64_t)offset, (uint64_t)size);

	addr = mmap(buf->map_addr, size, PROT_READ, flags, buf->fd, offset);

	if (addr == MAP_FAILED) {
		err = ERROR_OS;
		logmsg(err, "failed %smapping file (%s): %s",
		       buf->map_addr == NULL ? "" : "re-", buf->file_name,
		       strerror(errno));
		goto error;
	} else {
		//fprintf(stderr, "mapped %"PRIu64" bytes to address %p\n",
		//	(uint64_t)size, addr);

		buf->map_addr = addr;
		buf->map_size = size;
		buf->map_offset = offset;
		buf->map_pad = pad;
	}

map_ready:

	if (buf->file_size - offset < size) {
		length = (size_t)(buf->file_size - offset - pad);
		eof = 1;
	} else {
		length = size - pad;
		eof = 0;
	}

	madvise(buf->map_addr, length, MADV_SEQUENTIAL | MADV_WILLNEED);

	begin = (const uint8_t *)buf->map_addr + buf->map_pad;
	ptr = begin;
	end = ptr + length;

	buf->nline = 0;

	while (ptr != end) {
		ch = *ptr++;
		if (ch != '\n') {
			continue;
		}

		if (buf->nline == buf->nline_max) {
			if ((err = filebuf_grow_lines(buf, 1))) {
				goto error;
			}
		}

		buf->lines[buf->nline].ptr = begin;
		buf->lines[buf->nline].size = ptr - begin;
		buf->nline++;
		begin = ptr;

		if (buf->nline == INT_MAX && ptr != end) {
			eof = 0;
			break;
		}
	}

	if (begin < end && eof) {
		if (buf->nline == buf->nline_max) {
			if ((err = filebuf_grow_lines(buf, 1))) {
				goto error;
			}
		}

		buf->lines[buf->nline].ptr = begin;
		buf->lines[buf->nline].size = end - begin;
		buf->nline++;

	}

	madvise(buf->map_addr, length, MADV_SEQUENTIAL | MADV_WILLNEED);

	return (length > 0);

error:
	buf->error = err;
	return 0;
}
