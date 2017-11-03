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
#include "stopword.h"
#include "table.h"
#include "textset.h"
#include "tree.h"
#include "stem.h"
#include "symtab.h"
#include "wordscan.h"
#include "datatype.h"
#include "data.h"
#include "filter.h"

#define PROGRAM_NAME	"corpus"

#define COMBINE_MAX 256

static const char *combine_rules[COMBINE_MAX];


int main_tokens(int argc, char * const argv[]);
void usage_tokens(void);


struct string_arg {
	const char *name;
	int value;
	const char *desc;
};


static struct string_arg char_maps[] = {
	{ "case", UTF8LITE_TEXTMAP_CASE,
		"Performs Unicode case-folding." },
	{ "compat", UTF8LITE_TEXTMAP_COMPAT,
		"Applies Unicode compatibility mappings."},
	{ "ignorable", UTF8LITE_TEXTMAP_RMDI,
		"Removes Unicode default ignorables." },
	{ "quote", UTF8LITE_TEXTMAP_QUOTE,
		"Replaces Unicode quotes with ASCII single quote (')." },
	{ NULL, 0, NULL }
};


static struct string_arg word_classes[] = {
	{ "letter", CORPUS_FILTER_DROP_LETTER, "Composed of letters." },
	{ "number", CORPUS_FILTER_DROP_NUMBER, "Appears to be a number." },
	{ "punct",  CORPUS_FILTER_DROP_PUNCT,  "Punctuation." },
	{ "symbol", CORPUS_FILTER_DROP_SYMBOL, "Symbols." },
	{ NULL, 0, NULL }
};


static int get_arg(const struct string_arg options[], const char *name)
{
	int i;

	for (i = 0; options[i].name != NULL; i++) {
		if (strcmp(options[i].name, name) == 0) {
			return i;
		}
	}
	return -1;
}


void usage_tokens(void)
{
	const char **stems = corpus_stem_snowball_names();
	const char **stops = corpus_stopword_names();
	int i;

	printf("\
Usage:\t%s tokens [options] <path>\n\
\n\
Description:\n\
\tSegment text into tokens.\n\
\n\
Options:\n\
\t-c <combine>\tAdds a combination rule.\n\
\t-d <class>\tReplace words from the given class with 'null'.\n\
\t-f <field>\tGets text from the given field (defaults to \"text\").\n\
\t-k <map>\tDoes not perform the given character map.\n\
\t-o <path>\tSaves output at the given path.\n\
\t-s <stemmer>\tStems tokens with the given algorithm.\n\
\t-t <stopwords>\tDrops words from the given stop word list.\n\
", PROGRAM_NAME);
	printf("\nCharacter Maps:\n");
	for (i = 0; char_maps[i].name != NULL; i++) {
		printf("\t%s%s\t%s\n", char_maps[i].name,
			strlen(char_maps[i].name) < 8 ? "\t" : "",
			char_maps[i].desc);
	}

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

	printf("\nStop Word Lists:");
	if (*stops) {
		for (i = 0; stops[i] != NULL; i++) {
			if (i != 0) {
				printf(",");
			}
			if (i % 6 == 0) {
				printf("\n\t%s", stops[i]);
			} else {
				printf(" %s", stops[i]);
			}
		}
		printf("\n");
	} else {
		printf("\n\t(none available)\n");
	}

	printf("\nWord Classes:\n");
	for (i = 0; word_classes[i].name != NULL; i++) {
		printf("\t%s%s\t%s\n", word_classes[i].name,
			strlen(word_classes[i].name) < 8 ? "\t" : "",
			word_classes[i].desc);
	}
}

int main_tokens(int argc, char * const argv[])
{
	struct corpus_filter filter;
	struct corpus_stem_snowball snowball;
	struct corpus_data data, val;
	struct utf8lite_text name, text, word;
	const struct utf8lite_text *type;
	struct corpus_schema schema;
	struct corpus_filebuf buf;
	struct corpus_filebuf_iter it;
	struct utf8lite_render render;
	const char *output = NULL;
	const char *stemmer = NULL;
	const uint8_t **stopwords = NULL;
	const char *field, *input;
	FILE *stream;
	size_t field_len;
	int filter_flags, type_flags;
	int ch, err, i, name_id, start, type_id, ncomb;

	filter_flags = CORPUS_FILTER_KEEP_ALL;
	type_flags = (UTF8LITE_TEXTMAP_CASE | UTF8LITE_TEXTMAP_COMPAT
			| UTF8LITE_TEXTMAP_QUOTE | UTF8LITE_TEXTMAP_RMDI);

	field = "text";
	ncomb = 0;

	while ((ch = getopt(argc, argv, "c:d:f:k:o:s:t:")) != -1) {
		switch (ch) {
		case 'c':
			if (ncomb == COMBINE_MAX) {
				fprintf(stderr, "Too many combination rules"
					" (maximum is %d)\n", COMBINE_MAX);
				return EXIT_FAILURE;
			}

			combine_rules[ncomb] = optarg;
			ncomb++;
			break;

		case 'd':
			i = get_arg(word_classes, optarg);
			if (i < 0) {
				fprintf(stderr,
					"Unrecognized word class: '%s'.\n\n",
					optarg);
				usage_tokens();
				return EXIT_FAILURE;
			}
			filter_flags |= word_classes[i].value;
			break;
		case 'f':
			field = optarg;
			break;
		case 'k':
			i = get_arg(char_maps, optarg);
			if (i < 0) {
				fprintf(stderr,
					"Unrecognized character map: '%s'.\n\n",
					optarg);
				usage_tokens();
				return EXIT_FAILURE;
			}
			type_flags &= ~(char_maps[i].value);
			break;
		case 'o':
			output = optarg;
			break;
		case 's':
			stemmer = optarg;
			break;
		case 't':
			if (!(stopwords = corpus_stopword_list(optarg, NULL))) {
				fprintf(stderr,
					"Unrecognized stop word list: '%s'."
					"\n\n", optarg);
				usage_tokens();
				return EXIT_FAILURE;
			}
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

	if (utf8lite_text_assign(&name, (const uint8_t *)field, field_len, 0,
				 NULL)) {
		fprintf(stderr, "Invalid field name (%s)\n", field);
		return EXIT_FAILURE;
	}

	if ((err = utf8lite_render_init(&render, (UTF8LITE_ESCAPE_CONTROL
						  | UTF8LITE_ESCAPE_UTF8)))) {
		goto error_render;
	}

	if ((err = corpus_schema_init(&schema))) {
		goto error_schema;
	}

	if (stemmer) {
		if ((err = corpus_stem_snowball_init(&snowball, stemmer))) {
			goto error_snowball;
		}
		if ((err = corpus_filter_init(&filter, filter_flags,
					      type_flags, '_',
					      corpus_stem_snowball,
					      &snowball))) {
			goto error_filter;
		}
	} else {
		if ((err = corpus_filter_init(&filter, filter_flags,
					      type_flags, '_', NULL, NULL))) {
			goto error_filter;
		}
	}

	if (stopwords) {
		while (*stopwords) {
			err = utf8lite_text_assign(&word, *stopwords,
					           strlen((const char *)
							   *stopwords),
						   UTF8LITE_TEXT_UNKNOWN,
						   NULL);
			if (err) {
				fprintf(stderr, "Internal error:"
					" stop word list is not valid UTF-8.");
				goto error_stopwords;
			}

			err = corpus_filter_stem_except(&filter, &word);
			if (err) {
				goto error_stopwords;
			}

			err = corpus_filter_drop(&filter, &word);
			if (err) {
				goto error_stopwords;
			}

			stopwords++;
		}
	}

	for (i = 0; i < ncomb; i++) {
		err = utf8lite_text_assign(&word,
				           (const uint8_t *)combine_rules[i],
					   strlen(combine_rules[i]),
					   UTF8LITE_TEXT_UNKNOWN, NULL);
		if (err) {
			fprintf(stderr,
				"Combination rule ('%s') is not valid UTF-8.",
				combine_rules[i]);
			goto error_combine;
		}

		if ((err = corpus_filter_combine(&filter, &word))) {
			goto error_combine;
		}
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
		start = 1;

		if ((err = corpus_filter_start(&filter, &text))) {
			goto error;
		}

		while (corpus_filter_advance(&filter)) {
			type_id = filter.type_id;
			if (type_id == CORPUS_TYPE_NONE) {
				continue;
			}

			if (!start) {
				fprintf(stream, ", ");
			} else {
				start = 0;
			}

			if (type_id < 0) {
				fprintf(stream, "null");
			} else {
				type = &filter.symtab.types[type_id].text;

				utf8lite_render_clear(&render);
				utf8lite_render_text(&render, type);
				if ((err = render.error)) {
					goto error;
				}

				fprintf(stream, "\"%.*s\"",
					render.length, render.string);
			}
		}
		if (filter.error) {
			goto error;
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
error_combine:
error_stopwords:
	corpus_filter_destroy(&filter);
error_filter:
	if (stemmer) {
		corpus_stem_snowball_destroy(&snowball);
	}
error_snowball:
	corpus_schema_destroy(&schema);
error_schema:
	utf8lite_render_destroy(&render);
error_render:
	if (err) {
		fprintf(stderr, "An error occurred.\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
