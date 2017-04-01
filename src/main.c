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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
\tscan\tDetermine the schema of a data file\n\
", PROGRAM_NAME);

	exit(status);
}


void version()
{
	printf("%s version %s\n", PROGRAM_NAME, PROGRAM_VERSION);
	exit(EXIT_SUCCESS);
}


void usage_scan(int status)
{
	printf("\
Usage:\t%s scan [options] <path>\n\
\n\
Description:\n\
\tDetermine the types of the data values in an input file.  If you do\n\
\tnot specify one of the format options (-j or -t), then use the file\n\
\textension to determine the input format.\n\
\n\
Options:\n\
\t-j\t\tParses input in JSON Lines format.\n\
\t-o <path>\tSaves output at the given path.\n\
\t-t\t\tParses input in Tab Separated Value (TSV) format.\n\
", PROGRAM_NAME);

	exit(status);
}

enum file_type {
	FILE_NONE = 0,
	FILE_JSONL,
	FILE_TSV
};


int main_scan(int argc, char * const argv[], int help)
{
	const char *output = NULL;
	const char *input = NULL;
	const char *ext;
	int type = FILE_NONE;
	int ch;

	if (help) {
		usage_scan(EXIT_SUCCESS);
	}

	while ((ch = getopt(argc, argv, "jo:t")) != -1) {
		switch (ch) {
		case 'j':
			type = FILE_JSONL;
			break;
		case 'o':
			output = optarg;
			break;
		case 't':
			type = FILE_TSV;
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

	if (!type) {
		if ((ext = strrchr(input, '.')) == NULL) {
			// no file extension; assume json lines
			type = FILE_JSONL;
		} else if (!strcmp(ext, ".tsv")) {
			type = FILE_TSV;
		} else if (!strcmp(ext, ".json") || !strcmp(ext, ".jsonl")) {
			type = FILE_JSONL;
		} else {
			fprintf(stderr, "Unrecognized file extension '%s'.\n",
				ext);
			usage_scan(EXIT_FAILURE);
		}
	}

	printf("input:  %s\n", input);
	printf("format: %s\n", type == FILE_JSONL ? "jsonl" : "tsv");
	printf("output: %s\n", output ? output : "(stdout)");

	//usage_scan(EXIT_FAILURE);
	return 0;
}


int main(int argc, char * const argv[])
{
	int help = 0;
	int ch;

	while ((ch = getopt(argc, argv, "hv")) != -1) {
		switch (ch) {
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

	if (argc == 0) {
		usage(EXIT_FAILURE);
	} else if (strcmp(argv[0], "scan") != 0) {
		fprintf(stderr, "Unrecognized command '%s'.\n\n",
			argv[0]);
		usage(EXIT_FAILURE);
	}

	return main_scan(argc, argv, help);
}
