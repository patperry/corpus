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
#define PROGRAM_VERSION	"0.1"


void usage(int status)
{
	printf("\
Usage:\t%s [options] <command> [<args>]\n\
Options:\n\
\t-h\tPrints the help synopsis.\n\
\t-v\tPrints the version number.\n\
\n\
Commands:\n\
\tget\tExtract a field from a data file.\n\
\ttokens\tTokenize text from from a data file.\n\
\tscan\tDetermine the schema of a data file.\n\
", PROGRAM_NAME);

	exit(status);
}


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
", PROGRAM_NAME);

	exit(status);
}


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


void version(void)
{
	printf("%s version %s\n", PROGRAM_NAME, PROGRAM_VERSION);
	exit(EXIT_SUCCESS);
}


void check_ext(const char *file_name)
{
	const char *ext;

	if ((ext = strrchr(file_name, '.')) == NULL) {
		// no file extension; assume json lines
	} else if (!strcmp(ext, ".json") || !strcmp(ext, ".jsonl")) {
		// pass
	} else {
		fprintf(stderr, "Unrecognized file extension '%s'.\n", ext);
		exit(EXIT_FAILURE);
	}
}


int main_get(int argc, char * const argv[], int help)
{
	struct data data, val;
	struct text name;
	struct schema schema;
	struct filebuf buf;
	const char *output = NULL;
	const char *field, *input;
	FILE *stream;
	int ch, err, i, name_id;

	if (help) {
		usage_get(EXIT_SUCCESS);
	}

	while ((ch = getopt(argc, argv, "o:")) != -1) {
		switch (ch) {
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

	if (text_assign(&name, (uint8_t *)field, strlen(field), 0)) {
		fprintf(stderr, "Invalid field name (%s)\n", field);
		exit(EXIT_FAILURE);
	}

	check_ext(input);

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
	}
	return err;
}


int main_scan(int argc, char * const argv[], int help)
{
	struct schema schema;
	struct filebuf buf;
	const char *output = NULL;
	const char *input = NULL;
	FILE *stream;
	int ch, err, i, id, type_id;
	int lines = 0;

	if (help) {
		usage_scan(EXIT_SUCCESS);
	}

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
	check_ext(input);

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
	}
	return err;
}


int main(int argc, char * const argv[])
{
	int help = 0, debug = 0, err = 0;
	int ch;

	openlog(PROGRAM_NAME, LOG_CONS | LOG_PERROR | LOG_PID, LOG_USER);
        setlogmask(LOG_UPTO(LOG_INFO));

	while ((ch = getopt(argc, argv, "dhv")) != -1) {
		switch (ch) {
		case 'd':
			debug = 1;
			break;
		case 'h':
			help = 1;
			break;
		case 'v':
			version();
			break;
		default:
			usage(EXIT_FAILURE);
		}
	}

	argc -= optind;
	argv += optind;
	optreset = 1;
	optind = 1;

	if (debug) {
        	setlogmask(LOG_UPTO(LOG_DEBUG));
	}

	if (argc == 0) {
		usage(EXIT_FAILURE);
	} else if (!strcmp(argv[0], "get")) {
		err = main_get(argc, argv, help);
	} else if (!strcmp(argv[0], "tokens")) {
		usage_tokens(EXIT_FAILURE);
	} else if (!strcmp(argv[0], "scan")) {
		err = main_scan(argc, argv, help);
	} else {
		fprintf(stderr, "Unrecognized command '%s'.\n\n", argv[0]);
		usage(EXIT_FAILURE);
	}

	return (err == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
