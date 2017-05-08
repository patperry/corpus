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

#include "error.h"
#include "filebuf.h"
#include "table.h"
#include "text.h"
#include "token.h"
#include "symtab.h"
#include "datatype.h"
#include "data.h"
#include "sentscan.h"

#define PROGRAM_NAME	"corpus"


void usage_sentences(int status)
{
	printf("\
Usage:\t%s sentences [options] <path>\n\
\n\
Description:\n\
\tSegment text into sentences.\n\
\n\
Options:\n\
\t-f <field>\tGets text from the given field (defaults to \"text\").\n\
\t-o <path>\tSaves output at the given path.\n\
", PROGRAM_NAME);

	exit(status);
}


int main_sentences(int argc, char * const argv[])
{
	struct sentscan scan;
	struct data data, val;
	struct text name, text;
	struct schema schema;
	struct filebuf buf;
	struct filebuf_iter it;
	const char *output = NULL;
	const char *field, *input;
	FILE *stream;
	size_t field_len;
	int ch, err, name_id, start;

	field = "text";

	while ((ch = getopt(argc, argv, "f:o:")) != -1) {
		switch (ch) {
		case 'f':
			field = optarg;
			break;
		case 'o':
			output = optarg;
			break;
		default:
			usage_sentences(EXIT_FAILURE);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		fprintf(stderr, "No input file specified.\n\n");
		usage_sentences(EXIT_FAILURE);
	} else if (argc > 1) {
		fprintf(stderr, "Too many input files specified.\n\n");
		usage_sentences(EXIT_FAILURE);
	}

	field_len = strlen(field);
	if (field_len > 0 && field[0] == '"') {
		field++;
		field_len -= 2;
	}

	input = argv[0];

	if (text_assign(&name, (const uint8_t *)field, field_len, 0)) {
		fprintf(stderr, "Invalid field name (%s)\n", field);
		exit(EXIT_FAILURE);
	}

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

	if ((err = schema_name(&schema, &name, &name_id))) {
		goto error;
	}

	filebuf_iter_make(&it, &buf);
	while (filebuf_iter_advance(&it)) {
		if ((err = data_assign(&data, &schema, it.current.ptr,
					it.current.size))) {
				goto error;
		}

		if ((err = data_field(&data, &schema, name_id, &val))) {
			err = data_text(&data, &text);
		} else {
			err = data_text(&val, &text);
		}

		if (err) {
			fprintf(stream, "null\n");
			continue;
		}

		fprintf(stream, "[");
		sentscan_make(&scan, &text);
		start = 1;
		while (sentscan_advance(&scan)) {
			if (!start) {
				fprintf(stream, ", ");
			} else {
				start = 0;
			}

			fprintf(stream, "\"%.*s\"",
				(int)TEXT_SIZE(&scan.current),
				(char *)scan.current.ptr);
		}
		fprintf(stream, "]\n");
	}

	err = 0;

error:
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
