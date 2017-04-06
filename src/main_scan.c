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

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <syslog.h>

#include "errcode.h"
#include "filebuf.h"
#include "table.h"
#include "text.h"
#include "token.h"
#include "symtab.h"
#include "datatype.h"
#include "data.h"

#define PROGRAM_NAME	"corpus"


void usage_scan(int status)
{
	printf("\
Usage:\t%s scan [options] <path>\n\
\n\
Description:\n\
\tDetermine the types of the data values in a JSON Lines input file.\n\
\n\
Options:\n\
\t-l\t\tPrints type information for each line.\n\
\t-o <path>\tSaves output at the given path.\n\
", PROGRAM_NAME);

	exit(status);
}


int main_scan(int argc, char * const argv[])
{
	struct schema schema;
	struct filebuf buf;
	const char *output = NULL;
	const char *input = NULL;
	FILE *stream;
	int ch, err, i, id, type_id;
	int lines = 0;

	while ((ch = getopt(argc, argv, "lo:")) != -1) {
		switch (ch) {
		case 'l':
			lines = 1;
			break;
		case 'o':
			output = optarg;
			break;
		default:
			usage_scan(EXIT_FAILURE);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		fprintf(stderr, "No input file specified.\n\n");
		usage_scan(EXIT_FAILURE);
	} else if (argc > 1) {
		fprintf(stderr, "Too many input files specified.\n\n");
		usage_scan(EXIT_FAILURE);
	}

	input = argv[0];

	if ((err = schema_init(&schema))) {
		goto error_schema;
	}

	if ((err = filebuf_init(&buf, input))) {
		goto error_filebuf;
	}

	if (output) {
		if (!(stream = fopen(output, "w"))) {
			perror("Failed opening output file");
			err = ERROR_OS;
			goto error_output;
		}
	} else {
		stream = stdout;
	}

	fprintf(stream, "file:   %s\n", input);
	fprintf(stream, "format: %s\n", "JSON Lines");
	fprintf(stream, "--\n");

	type_id = DATATYPE_NULL;

	while (filebuf_advance(&buf)) {
		for (i = 0; i < buf.nline; i++) {
			if ((err = schema_scan(&schema, buf.lines[i].ptr,
						buf.lines[i].size, &id))) {
				goto error_scan;
			}

			if (lines) {
				fprintf(stream, "%"PRId64"\t",
					1 + buf.offset + i);
				write_datatype(stream, &schema, id);
				fprintf(stream, "\n");
			}

			if ((err = schema_union(&schema, type_id, id,
							&type_id))) {
				goto error_scan;
			}
		}
	}

	if ((err = buf.error)) {
		perror("Failed scanning input file");
		goto error_scan;
	}

	if (lines) {
		fprintf(stream, "--\n");
	}
	write_datatype(stream, &schema, type_id);
	fprintf(stream, "\n");
	fprintf(stream, "%"PRId64" records\n", buf.offset);
	err = 0;

error_scan:
	if (output && fclose(stream) == EOF) {
		perror("Failed closing output file");
		err = ERROR_OS;
	}
error_output:
	filebuf_destroy(&buf);
error_filebuf:
	schema_destroy(&schema);
error_schema:
	if (err) {
		fprintf(stderr, "An error occurred.\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}