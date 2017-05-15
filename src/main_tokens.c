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

int main_tokens(int argc, char * const argv[]);
void usage_tokens(void);


void usage_tokens(void)
{
	const char **stems = corpus_stemmer_names();
	int i;

	printf("\
Usage:\t%s tokens [options] <path>\n\
\n\
Description:\n\
\tSegment text into tokens.\n\
\n\
Options:\n\
\t-c\t\tKeeps original case instead of case folding.\n\
\t-d\t\tKeeps dashes instead of replacing them with \"-\".\n\
\t-f <field>\tGets text from the given field (defaults to \"text\").\n\
\t-i\t\tKeeps default ignorable characters like soft hyphens.\n\
\t-k\t\tDoes not perform compatibility decompositions (NFKC).\n\
\t-o <path>\tSaves output at the given path.\n\
\t-q\t\tKeeps quotes instead of replacing them with \"'\".\n\
\t-s <stemmer>\tStems tokens with the given algorithm.\n\
\t-w\t\tKeeps white space.\n\
\t-x\t\tKeeps non-white-space control characters.\n\
\t-z\t\tKeeps zero-character (empty) tokens.\n\
", PROGRAM_NAME);

	printf("\nStemming Algorithms:");
	if (*stems) {

		for (i = 0; stems[i] != NULL; i++) {
			if (i != 0) {
				printf(",");
			}
			if (i % 6 == 0) {
				printf("\n\t%s", stems[i]);
			} else {
				printf(" %s", stems[i]);
			}
		}
		printf("\n");
	} else {
		printf("\n\t(none available)\n");
	}
}


int main_tokens(int argc, char * const argv[])
{
	struct corpus_wordscan scan;
	struct corpus_symtab symtab;
	struct corpus_data data, val;
	struct corpus_text name, text, word;
	struct corpus_schema schema;
	struct corpus_filebuf buf;
	struct corpus_filebuf_iter it;
	const char *output = NULL;
	const char *stemmer = NULL;
	const char *field, *input;
	FILE *stream;
	size_t field_len;
	int flags;
	int ch, err, name_id, start, tokid, typid, zero;

	flags = (CORPUS_TYPE_COMPAT | CORPUS_TYPE_CASEFOLD
			| CORPUS_TYPE_DASHFOLD | CORPUS_TYPE_QUOTFOLD
			| CORPUS_TYPE_RMCC | CORPUS_TYPE_RMDI
			| CORPUS_TYPE_RMWS);

	field = "text";

	zero = 0;

	while ((ch = getopt(argc, argv, "cdf:iko:qwxz")) != -1) {
		switch (ch) {
		case 'c':
			flags &= ~CORPUS_TYPE_CASEFOLD;
			break;
		case 'd':
			flags &= ~CORPUS_TYPE_DASHFOLD;
			break;
		case 'f':
			field = optarg;
			break;
		case 'i':
			flags &= ~CORPUS_TYPE_RMDI;
			break;
		case 'k':
			flags &= ~CORPUS_TYPE_COMPAT;
			break;
		case 'o':
			output = optarg;
			break;
		case 'q':
			flags &= ~CORPUS_TYPE_QUOTFOLD;
			break;
		case 's':
			stemmer = optarg;
			break;
		case 'w':
			flags &= ~CORPUS_TYPE_RMWS;
			break;
		case 'x':
			flags &= ~CORPUS_TYPE_RMCC;
			break;
		case 'z':
			zero = 1;
			break;
		default:
			usage_tokens();
			return EXIT_FAILURE;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		fprintf(stderr, "No input file specified.\n\n");
		usage_tokens();
		return EXIT_FAILURE;
	} else if (argc > 1) {
		fprintf(stderr, "Too many input files specified.\n\n");
		usage_tokens();
		return EXIT_FAILURE;
	}

	field_len = strlen(field);
	if (field_len > 0 && field[0] == '"') {
		field++;
		field_len -= 2;
	}

	input = argv[0];

	if (corpus_text_assign(&name, (const uint8_t *)field, field_len, 0)) {
		fprintf(stderr, "Invalid field name (%s)\n", field);
		return EXIT_FAILURE;
	}

	if ((err = corpus_schema_init(&schema))) {
		goto error_schema;
	}

	if ((err = corpus_symtab_init(&symtab, flags, stemmer))) {
		goto error_symtab;
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
		corpus_wordscan_make(&scan, &text);
		start = 1;
		while (corpus_wordscan_advance(&scan)) {

			if ((err = corpus_symtab_add_token(&symtab,
							   &scan.current,
							   &tokid))) {
				goto error;
			}

			typid = symtab.tokens[tokid].type_id;
			word = symtab.types[typid].text;

			if (CORPUS_TEXT_SIZE(&word) == 0 && !zero) {
				continue;
			}

			if (!start) {
				fprintf(stream, ", ");
			} else {
				start = 0;
			}

			fprintf(stream, "\"%.*s\"",
				(int)CORPUS_TEXT_SIZE(&word),
				(char *)word.ptr);
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
	corpus_symtab_destroy(&symtab);
error_symtab:
	corpus_schema_destroy(&schema);
error_schema:
	if (err) {
		fprintf(stderr, "An error occurred.\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
