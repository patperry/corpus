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

#define _POSIX_C_SOURCE 2 // for getopt

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../lib/utf8lite/src/utf8lite.h"

#include "error.h"
#include "filebuf.h"
#include "table.h"
#include "textset.h"
#include "stem.h"
#include "symtab.h"
#include "datatype.h"
#include "data.h"

#define PROGRAM_NAME	"corpus"

int main_scan(int argc, char * const argv[]);
void usage_scan(void);


void usage_scan(void)
{
	printf("\
Usage:\t%s scan [options] <path>\n\
\n\
Description:\n\
\tDetermine the types of the data values in a newline-delimited JSON file.\n\
\n\
Options:\n\
\t-l\t\tPrints type information for each line.\n\
\t-o <path>\tSaves output at the given path.\n\
", PROGRAM_NAME);
}


int main_scan(int argc, char * const argv[])
{
	struct corpus_schema schema;
	struct corpus_filebuf buf;
	struct corpus_filebuf_iter it;
	const char *output = NULL;
	const char *input = NULL;
	FILE *stream;
	int ch, err, id, type_id;
	int lines = 0;
	uint64_t lineno;

	while ((ch = getopt(argc, argv, "lo:")) != -1) {
		switch (ch) {
		case 'l':
			lines = 1;
			break;
		case 'o':
			output = optarg;
			break;
		default:
			usage_scan();
			return EXIT_FAILURE;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		fprintf(stderr, "No input file specified.\n\n");
		usage_scan();
		return EXIT_FAILURE;
	} else if (argc > 1) {
		fprintf(stderr, "Too many input files specified.\n\n");
		usage_scan();
		return EXIT_FAILURE;
	}

	input = argv[0];

	if ((err = corpus_schema_init(&schema))) {
		goto error_schema;
	}

	if ((err = corpus_filebuf_init(&buf, input))) {
		goto error_filebuf;
	}

	if (output) {
		if (!(stream = fopen(output, "w"))) {
			perror("Failed opening output file");
			err = CORPUS_ERROR_OS;
			goto error_output;
		}
	} else {
		stream = stdout;
	}

	fprintf(stream, "file:   %s\n", input);
	fprintf(stream, "format: %s\n", "newline-delimited JSON");
	fprintf(stream, "--\n");

	type_id = CORPUS_DATATYPE_NULL;

	corpus_filebuf_iter_make(&it, &buf);
	lineno = 0;
	while (corpus_filebuf_iter_advance(&it)) {
		lineno++;

		if ((err = corpus_schema_scan(&schema, it.current.ptr,
					it.current.size, &id))) {
			goto error_scan;
		}

		if (lines) {
			fprintf(stream, "%"PRId64"\t", lineno);
			corpus_write_datatype(stream, &schema, id);
			fprintf(stream, "\n");
		}

		if ((err = corpus_schema_union(&schema, type_id, id,
					       &type_id))) {
			goto error_scan;
		}
	}

	if (lines) {
		fprintf(stream, "--\n");
	}
	corpus_write_datatype(stream, &schema, type_id);
	fprintf(stream, "\n");
	fprintf(stream, "%"PRId64" rows\n", lineno);
	err = 0;

error_scan:
	if (output && fclose(stream) == EOF) {
		perror("Failed closing output file");
		err = CORPUS_ERROR_OS;
	}
error_output:
	corpus_filebuf_destroy(&buf);
error_filebuf:
	corpus_schema_destroy(&schema);
error_schema:
	if (err) {
		fprintf(stderr, "An error occurred.\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
