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


void usage_get(int status)
{
	printf("\
Usage:\t%s get [options] <field> <path>\n\
\n\
Description:\n\
\tExtract a field from a data file.\n\
\n\
Options:\n\
\t-o <path>\tSaves output at the given path.\n\
", PROGRAM_NAME);

	exit(status);
}


int main_get(int argc, char * const argv[])
{
	struct data data, val;
	struct text name;
	struct schema schema;
	struct filebuf buf;
	const char *output = NULL;
	const char *field, *input;
	FILE *stream;
	int ch, err, i, name_id;
	size_t field_len;

	while ((ch = getopt(argc, argv, "o:")) != -1) {
		switch (ch) {
		case 'o':
			output = optarg;
			break;
		default:
			usage_get(EXIT_FAILURE);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		fprintf(stderr, "No field specified.\n\n");
		usage_get(EXIT_FAILURE);
	} else if (argc == 1) {
		fprintf(stderr, "No input file specified.\n\n");
		usage_get(EXIT_FAILURE);
	} else if (argc > 2) {
		fprintf(stderr, "Too many input files specified.\n\n");
		usage_get(EXIT_FAILURE);
	}

	field = argv[0];
	input = argv[1];

	field_len = strlen(field);
	if (field_len > 0 && field[0] == '"') {
		field++;
		field_len -= 2;
	}

	if (text_assign(&name, (uint8_t *)field, field_len, 0)) {
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
		goto error_get;
	}

	while (filebuf_advance(&buf)) {
		for (i = 0; i < buf.nline; i++) {
			if ((err = data_assign(&data, &schema,
						buf.lines[i].ptr,
						buf.lines[i].size))) {
				goto error_get;
			}

			if (data_field(&data, &schema, name_id, &val) == 0) {
				// field exists
				fprintf(stream, "%.*s\n", (int)val.size,
					(char *)val.ptr);
			} else {
				// field is null
				fprintf(stream, "null\n");
			}
		}
	}

	if ((err = buf.error)) {
		perror("Failed parsing input file");
		goto error_get;
	}

	err = 0;

error_get:
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
