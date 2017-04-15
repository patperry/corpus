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

#define PROGRAM_NAME	"corpus"
#define PROGRAM_VERSION	"0.2.0"

void usage_get(int status);
void usage_scan(int status);
void usage_tokens(int status);

int main_get(int argc, char * const argv[]);
int main_scan(int argc, char * const argv[]);
int main_tokens(int argc, char * const argv[]);


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


void version(void)
{
	printf("%s version %s\n", PROGRAM_NAME, PROGRAM_VERSION);
	exit(EXIT_SUCCESS);
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
			usage(EXIT_FAILURE);
		}

		ch = argv[ind][1];

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

	argc -= ind;
	argv += ind;

	if (argc == 0) {
		usage(EXIT_FAILURE);
	} else if (!strcmp(argv[0], "get")) {
		if (help) {
			usage_get(EXIT_SUCCESS);
		}
		err = main_get(argc, argv);
	} else if (!strcmp(argv[0], "tokens")) {
		if (help) {
			usage_tokens(EXIT_SUCCESS);
		}
		err = main_tokens(argc, argv);
	} else if (!strcmp(argv[0], "scan")) {
		if (help) {
			usage_scan(EXIT_SUCCESS);
		}
		err = main_scan(argc, argv);
	} else {
		fprintf(stderr, "Unrecognized command '%s'.\n\n", argv[0]);
		usage(EXIT_FAILURE);
	}

	return err;
}
