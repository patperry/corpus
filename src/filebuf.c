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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <fcntl.h>	// open, O_RDONLY
#include <sys/mman.h>	// mmap, munmap, PROT_READ, MAP_SHARED, MAP_FAILED
#include <sys/stat.h>	// struct stat, fstat
#include <unistd.h>	// close

#include "error.h"
#include "xalloc.h"
#include "filebuf.h"


int filebuf_init(struct filebuf *buf, const char *file_name)
{
	struct stat stat;
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

	if (buf->file_size > SIZE_MAX) {
		err = ERROR_OVERFLOW;
		logmsg(err, "file size (%"PRIu64" bytes)"
			"exceeds maximum (%"PRIu64" bytes)",
			buf->file_size, (uint64_t)SIZE_MAX);
		goto mmap_fail;
	}

	buf->map_size = (size_t)buf->file_size;
	buf->map_addr = mmap(NULL, buf->map_size, PROT_READ, MAP_SHARED,
			     buf->fd, 0);
	if (buf->map_addr == MAP_FAILED) {
		err = ERROR_OS;
		logmsg(err, "failed memory-mapping file (%s): %s", file_name,
		      strerror(errno));
		goto mmap_fail;
	}

	err = 0;
	goto out;

mmap_fail:
fstat_fail:
	close(buf->fd);
open_fail:
	free(buf->file_name);
strdup_fail:
	logmsg(err, "failed initializing file buffer");
out:
	return err;
}


void filebuf_destroy(struct filebuf *buf)
{
	if (buf->map_addr) {
		//fprintf(stderr, "unmaping %"PRIu64" bytes from address %p\n",
		//        (uint64_t)buf->map_size, buf->map_addr);

		munmap(buf->map_addr, buf->map_size);
	}

	close(buf->fd);
	free(buf->file_name);
}


void filebuf_iter_make(struct filebuf_iter *it, const struct filebuf *buf)
{
	it->begin = (uint8_t *)buf->map_addr;
	it->end = it->begin + (size_t)buf->file_size;
	filebuf_iter_reset(it);
}


void filebuf_iter_reset(struct filebuf_iter *it)
{
	it->ptr = it->begin;
	it->current.ptr = NULL;
	it->current.size = 0;
}


int filebuf_iter_advance(struct filebuf_iter *it)
{
	const uint8_t *ptr = it->ptr;
	const uint8_t *end = it->end;
	uint_fast8_t ch;

	if (ptr == end) {
		it->current.ptr = NULL;
		it->current.size = 0;
		return 0;
	}

	it->current.ptr = ptr;

	do {
	       ch = *ptr++;
	} while (ch != '\n' && ptr != end);

	it->current.size = (size_t)(ptr - it->current.ptr);
	it->ptr = ptr;

	return 1;
}
