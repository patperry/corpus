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

#include "error.h"
#include "memory.h"
#include "filebuf.h"


#if (defined(_WIN32) || defined(_WIN64))

#include <windows.h>


int corpus_filebuf_init(struct corpus_filebuf *buf, const char *file_name)
{
	HANDLE handle, mapping;
	DWORD lo, hi;
	void *addr;
	int err;

	assert(file_name);

	if (!(buf->file_name = corpus_strdup(file_name))) {
		err = CORPUS_ERROR_NOMEM;
		corpus_log(err, "failed copying file name (%s)", file_name);
		goto strdup_fail;
	}

	handle = CreateFile(file_name, GENERIC_READ,
			    FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
			    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (handle == INVALID_HANDLE_VALUE) {
		// GetLastError()
		err = CORPUS_ERROR_OS;
		corpus_log(err, "failed opening file (%s)", buf->file_name);
		goto open_fail;
	}
	buf->handle = (intptr_t)handle;

	lo = GetFileSize(handle, &hi);
	buf->file_size = (uint64_t)lo + ((uint64_t)hi << 32);

	if (buf->file_size > SIZE_MAX) {
		err = CORPUS_ERROR_OVERFLOW;
		corpus_log(err, "file size (%"PRIu64" bytes)"
			   "exceeds maximum (%"PRIu64" bytes)",
			   buf->file_size, (uint64_t)SIZE_MAX);
		goto mapping_fail;
	}
	buf->map_size = (size_t)(buf->file_size);

	if (buf->map_size > 0) {
		mapping = CreateFileMapping(handle, NULL, PAGE_READONLY, hi,
					    lo, NULL);
		if (mapping == NULL) {
			// GetLastError()
			err = CORPUS_ERROR_OS;
			corpus_log(err,
				   "failed creating mapping for file (%s)",
				   buf->file_name);
			goto mapping_fail;
		}

		addr = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
		if (addr == NULL) {
			// GetLastError()
			err = CORPUS_ERROR_OS;
			corpus_log(err,
				   "failed creating mapping for file (%s)",
				   buf->file_name);
			goto view_fail;
		}
		buf->map_addr = addr;
		CloseHandle(mapping);
	} else {
		buf->map_addr = NULL;
	}

	err = 0;
	goto out;

view_fail:
	CloseHandle(mapping);
mapping_fail:
	CloseHandle(handle);
open_fail:
	corpus_free(buf->file_name);
strdup_fail:
	corpus_log(err, "failed initializing file buffer");
out:
	return err;
}


void corpus_filebuf_destroy(struct corpus_filebuf *buf)
{
	if (buf->map_addr) {
		UnmapViewOfFile(buf->map_addr);
	}
	CloseHandle((HANDLE)buf->handle);
	corpus_free(buf->file_name);
}


#else /* POSIX */


#include <fcntl.h>	// open, O_RDONLY
#include <sys/mman.h>	// mmap, munmap, PROT_READ, MAP_SHARED, MAP_FAILED
#include <sys/stat.h>	// struct stat, fstat
#include <unistd.h>	// close


int corpus_filebuf_init(struct corpus_filebuf *buf, const char *file_name)
{
	struct stat stat;
	int access, err;

	assert(file_name);

	if (!(buf->file_name = corpus_strdup(file_name))) {
		err = CORPUS_ERROR_NOMEM;
		corpus_log(err, "failed copying file name (%s)", file_name);
		goto strdup_fail;
	}

	access = O_RDONLY;
#ifdef O_LARGEFILE
	access |= O_LARGEFILE;
#endif

	if ((buf->handle = (intptr_t)open(buf->file_name, access)) < 0) {
		err = CORPUS_ERROR_OS;
		corpus_log(err, "failed opening file (%s): %s",
			   buf->file_name, strerror(errno));
		goto open_fail;
	}

	if (fstat((int)buf->handle, &stat) < 0) {
		err = CORPUS_ERROR_OS;
		corpus_log(err, "failed determining size of file (%s): %s",
			   buf->file_name, strerror(errno));
		goto fstat_fail;
	}
	buf->file_size = (uint64_t)stat.st_size;

	if (buf->file_size > SIZE_MAX) {
		err = CORPUS_ERROR_OVERFLOW;
		corpus_log(err, "file size (%"PRIu64" bytes)"
			   "exceeds maximum (%"PRIu64" bytes)",
			   buf->file_size, (uint64_t)SIZE_MAX);
		goto mmap_fail;
	}

	buf->map_size = (size_t)buf->file_size;
	if (buf->map_size > 0) {
		buf->map_addr = mmap(NULL, buf->map_size, PROT_READ,
				     MAP_SHARED, (int)buf->handle, 0);
		if (buf->map_addr == MAP_FAILED) {
			err = CORPUS_ERROR_OS;
			corpus_log(err, "failed memory-mapping file (%s): %s",
				   file_name, strerror(errno));
			goto mmap_fail;
		}
	} else {
		buf->map_addr = NULL;
	}

	err = 0;
	goto out;

mmap_fail:
fstat_fail:
	close((int)buf->handle);
open_fail:
	corpus_free(buf->file_name);
strdup_fail:
	corpus_log(err, "failed initializing file buffer");
out:
	return err;
}


void corpus_filebuf_destroy(struct corpus_filebuf *buf)
{
	if (buf->map_addr) {
		//fprintf(stderr, "unmaping %"PRIu64" bytes from address %p\n",
		//        (uint64_t)buf->map_size, buf->map_addr);

		munmap(buf->map_addr, buf->map_size);
	}

	close((int)buf->handle);
	corpus_free(buf->file_name);
}


#endif /* end of platform-specific code */ 


void corpus_filebuf_iter_make(struct corpus_filebuf_iter *it,
			      const struct corpus_filebuf *buf)
{
	it->begin = (uint8_t *)buf->map_addr;
	it->end = it->begin + (size_t)buf->file_size;
	corpus_filebuf_iter_reset(it);
}


void corpus_filebuf_iter_reset(struct corpus_filebuf_iter *it)
{
	it->ptr = it->begin;
	it->current.ptr = NULL;
	it->current.size = 0;
}


int corpus_filebuf_iter_advance(struct corpus_filebuf_iter *it)
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
