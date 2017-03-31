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

	if (argc == 0) {
		usage(EXIT_FAILURE);
	} else if (strcmp(argv[0], "scan") != 0) {
		fprintf(stderr, "Unrecognized command '%s'\n",
			argv[0]);
		usage(EXIT_FAILURE);
	}

	return 0;
}
