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
#include "sentscan.h"

#define PROGRAM_NAME	"corpus"

int main_sentences(int argc, char * const argv[]);
void usage_sentences(void);


void usage_sentences(void)
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
\t-z\t\tTreats CR and LF like separators, not spaces.\n\
", PROGRAM_NAME);
}


int main_sentences(int argc, char * const argv[])
{
	struct corpus_sentscan scan;
	struct corpus_data data, val;
	struct utf8lite_text name, text;
	struct corpus_schema schema;
	struct corpus_filebuf buf;
	struct corpus_filebuf_iter it;
	const char *output = NULL;
	const char *field, *input;
	FILE *stream;
	size_t field_len;
	int ch, err, name_id, start;
	int flags = CORPUS_SENTSCAN_SPCRLF;

	field = "text";

	while ((ch = getopt(argc, argv, "f:o:z")) != -1) {
		switch (ch) {
		case 'f':
			field = optarg;
			break;
		case 'o':
			output = optarg;
			break;
		case 'z':
			flags &= ~CORPUS_SENTSCAN_SPCRLF;
			break;
		default:
			usage_sentences();
			return EXIT_FAILURE;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		fprintf(stderr, "No input file specified.\n\n");
		usage_sentences();
		return EXIT_FAILURE;
	} else if (argc > 1) {
		fprintf(stderr, "Too many input files specified.\n\n");
		usage_sentences();
		return EXIT_FAILURE;
	}

	field_len = strlen(field);
	if (field_len > 0 && field[0] == '"') {
		field++;
		field_len -= 2;
	}

	input = argv[0];

	if (utf8lite_text_assign(&name, (const uint8_t *)field, field_len, 0,
				 NULL)) {
		fprintf(stderr, "Invalid field name (%s)\n", field);
		return EXIT_FAILURE;
	}

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

	if ((err = corpus_schema_name(&schema, &name, &name_id))) {
		goto error;
	}

	corpus_filebuf_iter_make(&it, &buf);
	while (corpus_filebuf_iter_advance(&it)) {
		if ((err = corpus_data_assign(&data, &schema, it.current.ptr,
					      it.current.size))) {
				goto error;
		}

		if ((err = corpus_data_field(&data, &schema, name_id, &val))) {
			err = corpus_data_text(&data, &text);
		} else {
			err = corpus_data_text(&val, &text);
		}

		if (err) {
			fprintf(stream, "null\n");
			continue;
		}

		fprintf(stream, "[");
		corpus_sentscan_make(&scan, &text, flags);
		start = 1;
		while (corpus_sentscan_advance(&scan)) {
			if (!start) {
				fprintf(stream, ", ");
			} else {
				start = 0;
			}

			fprintf(stream, "\"%.*s\"",
				(int)UTF8LITE_TEXT_SIZE(&scan.current),
				(char *)scan.current.ptr);
		}
		fprintf(stream, "]\n");
	}

	err = 0;

error:
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
