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
#include "wordscan.h"

#define PROGRAM_NAME	"corpus"


void usage_tokens(int status)
{
	printf("\
Usage:\t%s tokens [options] <path>\n\
\n\
Description:\n\
\tTokenize text from from a data file.\n\
\n\
Options:\n\
\t-c\t\tKeeps original case instead of case folding.\n\
\t-d\t\tKeeps dashes instead of replacing them with \"-\".\n\
\t-f <field>\tGets text from the given field (defaults to \"text\").\n\
\t-i\t\tKeeps default ignorable characters like soft hyphens.\n\
\t-k\t\tDoes not perform compatibility decompositions (NFKD).\n\
\t-o <path>\tSaves output at the given path.\n\
\t-q\t\tKeeps quotes instead of replacing them with \"'\".\n\
\t-w\t\tKeeps white space.\n\
\t-x\t\tKeeps non-white-space control characters.\n\
\t-z\t\tKeeps zero-character (empty) tokens.\n\
", PROGRAM_NAME);

	exit(status);
}


int main_tokens(int argc, char * const argv[])
{
	struct wordscan scan;
	struct symtab symtab;
	struct data data, val;
	struct text name, text, word;
	struct schema schema;
	struct filebuf buf;
	const char *output = NULL;
	const char *field, *input;
	FILE *stream;
	size_t field_len;
	int flags;
	int ch, err, i, name_id, start, tokid, typid, zero;

	flags = (TYPE_COMPAT | TYPE_CASEFOLD | TYPE_DASHFOLD
			| TYPE_QUOTFOLD | TYPE_RMCC | TYPE_RMDI
			| TYPE_RMWS);

	field = "text";

	zero = 0;

	while ((ch = getopt(argc, argv, "cdf:iko:qwxz")) != -1) {
		switch (ch) {
		case 'c':
			flags &= ~TYPE_CASEFOLD;
			break;
		case 'd':
			flags &= ~TYPE_DASHFOLD;
			break;
		case 'f':
			field = optarg;
			break;
		case 'i':
			flags &= ~TYPE_RMDI;
			break;
		case 'k':
			flags &= ~TYPE_COMPAT;
			break;
		case 'o':
			output = optarg;
			break;
		case 'q':
			flags &= ~TYPE_QUOTFOLD;
			break;
		case 'w':
			flags &= ~TYPE_RMWS;
			break;
		case 'x':
			flags &= ~TYPE_RMCC;
			break;
		case 'z':
			zero = 1;
			break;
		default:
			usage_tokens(EXIT_FAILURE);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		fprintf(stderr, "No input file specified.\n\n");
		usage_tokens(EXIT_FAILURE);
	} else if (argc > 1) {
		fprintf(stderr, "Too many input files specified.\n\n");
		usage_tokens(EXIT_FAILURE);
	}

	field_len = strlen(field);
	if (field_len > 0 && field[0] == '"') {
		field++;
		field_len -= 2;
	}

	input = argv[0];

	if (text_assign(&name, (uint8_t *)field, field_len, 0)) {
		fprintf(stderr, "Invalid field name (%s)\n", field);
		exit(EXIT_FAILURE);
	}

	if ((err = schema_init(&schema))) {
		goto error_schema;
	}

	if ((err = symtab_init(&symtab, flags))) {
		goto error_symtab;
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

	while (filebuf_advance(&buf)) {
		for (i = 0; i < buf.nline; i++) {
			if ((err = data_assign(&data, &schema,
						buf.lines[i].ptr,
						buf.lines[i].size))) {
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
			wordscan_make(&scan, &text);
			start = 1;
			while (wordscan_advance(&scan)) {

				if ((err = symtab_add_token(&symtab,
							&scan.current,
							&tokid))) {
					goto error;
				}

				typid = symtab.tokens[tokid].type_id;
				word = symtab.types[typid].text;

				if (TEXT_SIZE(&word) == 0 && !zero) {
					continue;
				}

				if (!start) {
					fprintf(stream, ", ");
				} else {
					start = 0;
				}

				fprintf(stream, "\"%.*s\"",
					(int)TEXT_SIZE(&word),
					(char *)word.ptr);
			}
			fprintf(stream, "]\n");
		}
	}

	if ((err = buf.error)) {
		perror("Failed parsing input file");
		goto error;
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
	symtab_destroy(&symtab);
error_symtab:
	schema_destroy(&schema);
error_schema:
	if (err) {
		fprintf(stderr, "An error occurred.\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
