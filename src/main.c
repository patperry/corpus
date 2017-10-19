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
#define PROGRAM_VERSION	"0.6.0"

void usage(void);
void usage_get(void);
void usage_ngrams(void);
void usage_scan(void);
void usage_sentences(void);
void usage_tokens(void);

void version(void);

int main_get(int argc, char * const argv[]);
int main_ngrams(int argc, char * const argv[]);
int main_scan(int argc, char * const argv[]);
int main_sentences(int argc, char * const argv[]);
int main_tokens(int argc, char * const argv[]);


void usage(void)
{
	printf("\
Usage:\t%s [options] <command> [<args>]\n\
Options:\n\
\t-h\tPrints the help synopsis.\n\
\t-v\tPrints the version number.\n\
\n\
Commands:\n\
\tget\tExtract a field from a data file.\n\
\tngrams\tCompute token n-gram frequencies.\n\
\tscan\tDetermine the schema of a data file.\n\
\tsentences\tSegment text into sentences.\n\
\ttokens\tSegment text into tokens.\n\
", PROGRAM_NAME);
}


void version(void)
{
	printf("%s version %s\n", PROGRAM_NAME, PROGRAM_VERSION);
}


int main(int argc, char * const argv[])
{
	int help = 0, err = 0;
	int ind, ch;

	// handle initial arguments ourselves rather than calling
	// getopt twice, which requires platform-specific behavior

	for (ind = 1; ind < argc; ind++) {
		if (argv[ind][0] != '-') {
			break;
		}

		if (argv[ind][2] != '\0') {
			printf("illegal option: %s\n", argv[ind]);
			usage();
			return EXIT_FAILURE;
		}

		ch = argv[ind][1];

		switch (ch) {
		case 'h':
			help = 1;
			break;
		case 'v':
			version();
			return EXIT_SUCCESS;
		default:
			usage();
			return EXIT_FAILURE;
		}
	}

	argc -= ind;
	argv += ind;

	if (argc == 0) {
		usage();
		return EXIT_FAILURE;
	} else if (!strcmp(argv[0], "get")) {
		if (help) {
			usage_get();
			return EXIT_SUCCESS;
		}
		err = main_get(argc, argv);
	} else if (!strcmp(argv[0], "ngrams")) {
		if (help) {
			usage_ngrams();
			return EXIT_SUCCESS;
		}
		err = main_ngrams(argc, argv);
	} else if (!strcmp(argv[0], "tokens")) {
		if (help) {
			usage_tokens();
			return EXIT_SUCCESS;
		}
		err = main_tokens(argc, argv);
	} else if (!strcmp(argv[0], "sentences")) {
		if (help) {
			usage_sentences();
			return EXIT_SUCCESS;
		}
		err = main_sentences(argc, argv);
	} else if (!strcmp(argv[0], "scan")) {
		if (help) {
			usage_scan();
			return EXIT_SUCCESS;
		}
		err = main_scan(argc, argv);
	} else {
		fprintf(stderr, "Unrecognized command '%s'.\n\n", argv[0]);
		usage();
		return EXIT_FAILURE;
	}

	return err;
}
