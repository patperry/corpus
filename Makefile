CC     += -std=c99

CFLAGS += -Wall -Wextra -pedantic -Werror \
	-Wno-cast-qual \
	-Wno-padded \
	-Wno-unused-macros \
	-g

#LDFLAGS +=
LIBS    += -lm
AR      = ar rcu
RANLIB  = ranlib
MKDIR_P = mkdir -p
CURL    = curl

LIB_CFLAGS = \
	-Wno-cast-align \
	-Wno-cast-qual \
	-Wno-float-equal \
	-Wno-missing-prototypes \
	-Wno-sign-conversion \
	-Wno-unreachable-code-break

TEST_CFLAGS = $(shell pkg-config --cflags check) \
	-Wno-double-promotion \
	-Wno-float-equal \
	-Wno-gnu-zero-variadic-macro-arguments \
	-Wno-missing-prototypes \
	-Wno-missing-variable-declarations \
	-Wno-reserved-id-macro

TEST_LIBS = $(shell pkg-config --libs check)

CLDR = https://raw.githubusercontent.com/unicode-cldr/cldr-segments-modern/master
EMOJI = http://www.unicode.org/Public/emoji/5.0
UNICODE = http://www.unicode.org/Public/10.0.0

CORPUS_A = libcorpus.a
LIB_O	= lib/strntod.o lib/strntoimax.o \
	  lib/utf8lite/src/char.o lib/utf8lite/src/encode.o \
	  lib/utf8lite/src/error.o lib/utf8lite/src/escape.o \
	  lib/utf8lite/src/normalize.o lib/utf8lite/src/text.o \
	  lib/utf8lite/src/textassign.o lib/utf8lite/src/textiter.o \
	  lib/utf8lite/src/textmap.o \
	  src/array.o src/census.o \
	  src/data.o src/datatype.o src/error.o src/filebuf.o src/filter.o \
	  src/intset.o src/memory.o src/ngram.o src/render.o src/search.o \
	  src/sentfilter.o src/sentscan.o src/stem.o src/stopword.o \
	  src/symtab.o src/table.o src/termset.o src/textset.o \
	  src/tree.o src/wordscan.o

STEMMER = lib/libstemmer_c
STEMMER_O = $(STEMMER)/src_c/stem_UTF_8_arabic.o \
	    $(STEMMER)/src_c/stem_UTF_8_danish.o \
	    $(STEMMER)/src_c/stem_UTF_8_dutch.o \
	    $(STEMMER)/src_c/stem_UTF_8_english.o \
	    $(STEMMER)/src_c/stem_UTF_8_finnish.o \
	    $(STEMMER)/src_c/stem_UTF_8_french.o \
	    $(STEMMER)/src_c/stem_UTF_8_german.o \
	    $(STEMMER)/src_c/stem_UTF_8_hungarian.o \
	    $(STEMMER)/src_c/stem_UTF_8_italian.o \
	    $(STEMMER)/src_c/stem_UTF_8_norwegian.o \
	    $(STEMMER)/src_c/stem_UTF_8_porter.o \
	    $(STEMMER)/src_c/stem_UTF_8_portuguese.o \
	    $(STEMMER)/src_c/stem_UTF_8_romanian.o \
	    $(STEMMER)/src_c/stem_UTF_8_russian.o \
	    $(STEMMER)/src_c/stem_UTF_8_spanish.o \
	    $(STEMMER)/src_c/stem_UTF_8_swedish.o \
	    $(STEMMER)/src_c/stem_UTF_8_tamil.o \
	    $(STEMMER)/src_c/stem_UTF_8_turkish.o \
	    $(STEMMER)/runtime/api.o \
	    $(STEMMER)/runtime/utilities.o \
	    $(STEMMER)/libstemmer/libstemmer_utf8.o

CORPUS_T = corpus
CORPUS_O = src/main.o src/main_get.o src/main_ngrams.o src/main_scan.o \
		   src/main_sentences.o src/main_tokens.o

DATA    = data/emoji/emoji-data.txt \
	  data/ucd/CaseFolding.txt \
	  data/ucd/CompositionExclusions.txt \
	  data/ucd/DerivedCoreProperties.txt \
	  data/ucd/EastAsianWidth.txt \
	  data/ucd/PropList.txt \
	  data/ucd/Scripts.txt \
	  data/ucd/UnicodeData.txt \
	  data/ucd/auxiliary/SentenceBreakProperty.txt \
	  data/ucd/auxiliary/WordBreakProperty.txt

TESTS_T = tests/check_census tests/check_data \
	  tests/check_filter tests/check_intset tests/check_ngram \
	  tests/check_search tests/check_sentfilter tests/check_sentscan \
	  tests/check_stem tests/check_stopword tests/check_symtab \
	  tests/check_termset tests/check_tree tests/check_wordscan
TESTS_O = tests/check_census.o tests/check_data.o \
	  tests/check_filter.o tests/check_intset.o tests/check_ngram.o \
	  tests/check_search.o tests/check_sentfilter.o tests/check_sentscan.o \
	  tests/check_stem.o tests/check_stopword.o tests/check_symtab.o \
	  tests/check_termset.o tests/check_tree.o tests/check_wordscan.o \
	  tests/testutil.o

TESTS_DATA = data/ucd/auxiliary/SentenceBreakTest.txt \
	     data/ucd/auxiliary/WordBreakTest.txt

ALL_O = $(LIB_O) $(CORPUS_O) $(STEMMER_O)
ALL_T = $(CORPUS_A) $(CORPUS_T)
ALL_A = $(CORPUS_A)


# Products

all: $(ALL_T)

$(CORPUS_A): $(LIB_O) $(STEMMER_O)
	$(AR) $@ $(LIB_O) $(STEMMER_O)
	$(RANLIB) $@

$(CORPUS_T): $(CORPUS_O) $(CORPUS_A)
	$(CC) -o $@ $(CORPUS_O) $(CORPUS_A) $(LIBS) $(LDFLAGS)


# Data

data/cldr/segments/de/suppressions.json:
	$(MKDIR_P) data/cldr/segments/de
	$(CURL) -o $@ $(CLDR)/segments/de/suppressions.json

data/cldr/segments/en/suppressions.json:
	$(MKDIR_P) data/cldr/segments/en
	$(CURL) -o $@ $(CLDR)/segments/en/suppressions.json

data/cldr/segments/es/suppressions.json:
	$(MKDIR_P) data/cldr/segments/es
	$(CURL) -o $@ $(CLDR)/segments/es/suppressions.json

data/cldr/segments/fr/suppressions.json:
	$(MKDIR_P) data/cldr/segments/fr
	$(CURL) -o $@ $(CLDR)/segments/fr/suppressions.json

data/cldr/segments/it/suppressions.json:
	$(MKDIR_P) data/cldr/segments/it
	$(CURL) -o $@ $(CLDR)/segments/it/suppressions.json

data/cldr/segments/pt/suppressions.json:
	$(MKDIR_P) data/cldr/segments/pt
	$(CURL) -o $@ $(CLDR)/segments/pt/suppressions.json

data/cldr/segments/ru/suppressions.json:
	$(MKDIR_P) data/cldr/segments/ru
	$(CURL) -o $@ $(CLDR)/segments/ru/suppressions.json

data/ucd/DerivedCoreProperties.txt:
	$(MKDIR_P) data/ucd
	$(CURL) -o $@ $(UNICODE)/ucd/DerivedCoreProperties.txt

data/ucd/PropList.txt:
	$(MKDIR_P) data/ucd
	$(CURL) -o $@ $(UNICODE)/ucd/PropList.txt

data/ucd/Scripts.txt:
	$(MKDIR_P) data/ucd
	$(CURL) -o $@ $(UNICODE)/ucd/Scripts.txt

data/ucd/UnicodeData.txt:
	$(MKDIR_P) data/ucd
	$(CURL) -o $@ $(UNICODE)/ucd/UnicodeData.txt

data/ucd/auxiliary/SentenceBreakProperty.txt:
	$(MKDIR_P) data/ucd/auxiliary
	$(CURL) -o $@ $(UNICODE)/ucd/auxiliary/SentenceBreakProperty.txt

data/ucd/auxiliary/SentenceBreakTest.txt:
	$(MKDIR_P) data/ucd/auxiliary
	$(CURL) -o $@ $(UNICODE)/ucd/auxiliary/SentenceBreakTest.txt

data/ucd/auxiliary/WordBreakProperty.txt:
	$(MKDIR_P) data/ucd/auxiliary
	$(CURL) -o $@ $(UNICODE)/ucd/auxiliary/WordBreakProperty.txt

data/ucd/auxiliary/WordBreakTest.txt:
	$(MKDIR_P) data/ucd/auxiliary
	$(CURL) -o $@ $(UNICODE)/ucd/auxiliary/WordBreakTest.txt


# Generated Sources

src/private/sentsuppress.h: util/gen-sentsuppress.py \
		data/cldr/segments/de/suppressions.json \
		data/cldr/segments/en/suppressions.json \
		data/cldr/segments/es/suppressions.json \
		data/cldr/segments/fr/suppressions.json \
		data/cldr/segments/it/suppressions.json \
		data/cldr/segments/pt/suppressions.json \
		data/cldr/segments/ru/suppressions.json
	$(MKDIR_P) src/private
	./util/gen-sentsuppress.py > $@

src/private/stopwords.h: util/gen-stopwords.py \
		data/snowball/danish.txt data/snowball/dutch.txt \
		data/snowball/english.txt data/snowball/finnish.txt \
		data/snowball/french.txt data/snowball/german.txt \
		data/snowball/hungarian.txt data/snowball/italian.txt \
		data/snowball/norwegian.txt data/snowball/portuguese.txt \
		data/snowball/russian.txt data/snowball/spanish.txt \
		data/snowball/swedish.txt
	$(MKDIR_P) src/private
	./util/gen-stopwords.py > $@

src/unicode/sentbreakprop.h: util/gen-sentbreak.py util/property.py \
		data/ucd/auxiliary/SentenceBreakProperty.txt
	$(MKDIR_P) src/unicode
	./util/gen-sentbreak.py > $@

src/unicode/wordbreakprop.h: util/gen-wordbreak.py util/property.py \
		util/unicode_data.py \
		data/ucd/DerivedCoreProperties.txt \
		data/ucd/PropList.txt \
		data/ucd/auxiliary/WordBreakProperty.txt
	$(MKDIR_P) src/unicode
	./util/gen-wordbreak.py > $@


# Tests

tests/check_census: tests/check_census.o tests/testutil.o $(CORPUS_A)
	$(CC) -o $@ $^ $(LIBS) $(TEST_LIBS) $(LDFLAGS)

tests/check_data: tests/check_data.o tests/testutil.o $(CORPUS_A)
	$(CC) -o $@ $^ $(LIBS) $(TEST_LIBS) $(LDFLAGS)

tests/check_filter: tests/check_filter.o tests/testutil.o $(CORPUS_A)
	$(CC) -o $@ $^ $(LIBS) $(TEST_LIBS) $(LDFLAGS)

tests/check_intset: tests/check_intset.o tests/testutil.o $(CORPUS_A)
	$(CC) -o $@ $^ $(LIBS) $(TEST_LIBS) $(LDFLAGS)

tests/check_ngram: tests/check_ngram.o tests/testutil.o $(CORPUS_A)
	$(CC) -o $@ $^ $(LIBS) $(TEST_LIBS) $(LDFLAGS)

tests/check_search: tests/check_search.o tests/testutil.o $(CORPUS_A)
	$(CC) -o $@ $^ $(LIBS) $(TEST_LIBS) $(LDFLAGS)

tests/check_sentfilter: tests/check_sentfilter.o tests/testutil.o $(CORPUS_A)
	$(CC) -o $@ $^ $(LIBS) $(TEST_LIBS) $(LDFLAGS)

tests/check_sentscan: tests/check_sentscan.o tests/testutil.o $(CORPUS_A) \
		data/ucd/auxiliary/SentenceBreakTest.txt
	$(CC) -o $@ tests/check_sentscan.o tests/testutil.o $(CORPUS_A) \
		$(LIBS) $(TEST_LIBS) $(LDFLAGS)

tests/check_stem: tests/check_stem.o tests/testutil.o $(CORPUS_A)
	$(CC) -o $@ $^ $(LIBS) $(TEST_LIBS) $(LDFLAGS)

tests/check_stopword: tests/check_stopword.o tests/testutil.o $(CORPUS_A)
	$(CC) -o $@ $^ $(LIBS) $(TEST_LIBS) $(LDFLAGS)

tests/check_symtab: tests/check_symtab.o tests/testutil.o $(CORPUS_A)
	$(CC) -o $@ $^ $(LIBS) $(TEST_LIBS) $(LDFLAGS)

tests/check_termset: tests/check_termset.o tests/testutil.o $(CORPUS_A)
	$(CC) -o $@ $^ $(LIBS) $(TEST_LIBS) $(LDFLAGS)

tests/check_tree: tests/check_tree.o tests/testutil.o $(CORPUS_A)
	$(CC) -o $@ $^ $(LIBS) $(TEST_LIBS) $(LDFLAGS)

tests/check_wordscan: tests/check_wordscan.o tests/testutil.o $(CORPUS_A) \
		data/ucd/auxiliary/WordBreakTest.txt
	$(CC) -o $@ tests/check_wordscan.o tests/testutil.o $(CORPUS_A) \
		$(LIBS) $(TEST_LIBS) $(LDFLAGS)


# Special Rules

check: $(TESTS_T) $(TESTS_T:=.test)

clean:
	$(RM) -r $(ALL_O) $(ALL_T) $(TESTS_O) $(TESTS_T)

data: $(DATA) $(TESTS_DATA)

doc:
	doxygen

%.test: %
	$<

lib/%.o: lib/%.c
	$(CC) -c $(CFLAGS) $(LIB_CFLAGS) $(CPPFLAGS) $< -o $@

tests/%.o: tests/%.c
	$(CC) -c $(CFLAGS) $(TEST_CFLAGS) $(CPPFLAGS) $< -o $@


.PHONY: all check clean data doc


src/array.o: src/array.c src/error.h src/memory.h src/array.h
src/census.o: src/census.c src/array.h src/error.h src/memory.h src/table.h \
	src/census.h
src/data.o: src/data.c src/error.h src/table.h src/textset.h \
	src/symtab.h src/datatype.h src/data.h
src/datatype.o: src/datatype.c src/array.h src/error.h src/memory.h \
	src/render.h src/table.h src/textset.h \
	src/symtab.h src/data.h src/datatype.h
src/error.o: src/error.c src/error.h
src/filebuf.o: src/filebuf.c src/error.h src/memory.h src/filebuf.h
src/filter.o: src/filter.c src/array.h src/error.h src/memory.h src/render.h \
	src/table.h src/textset.h src/tree.h \
	src/stem.h src/symtab.h src/wordscan.h src/filter.h
src/intset.o: src/intset.c src/array.h src/error.h src/memory.h src/table.h \
	src/intset.h
src/main.o: src/main.c src/error.h src/filebuf.h src/table.h \
	src/textset.h src/stem.h src/symtab.h src/datatype.h
src/main_get.o: src/main_get.c src/error.h src/filebuf.h src/table.h \
	src/textset.h src/stem.h src/symtab.h \
	src/datatype.h src/data.h
src/main_ngrams.o: src/main_ngrams.c src/error.h src/filebuf.h src/stopword.h \
	src/table.h src/textset.h src/tree.h src/symtab.h \
	src/wordscan.h src/data.h src/datatype.h src/filter.h src/ngram.h
src/main_scan.o: src/main_scan.c src/error.h src/filebuf.h src/table.h \
	src/textset.h src/stem.h src/symtab.h src/datatype.h
src/main_sentences.o: src/main_sentences.c src/error.h src/filebuf.h \
	src/sentscan.h src/table.h src/textset.h src/stem.h \
	src/symtab.h src/data.h src/datatype.h
src/main_tokens.o: src/main_tokens.c src/error.h src/filebuf.h src/table.h \
	src/textset.h src/tree.h src/stopword.h src/symtab.h \
	src/wordscan.h src/data.h src/datatype.h src/filter.h
src/memory.o: src/memory.c src/memory.h
src/ngram.o: src/ngram.c src/array.h src/error.h src/memory.h src/table.h \
	src/tree.h src/ngram.h
src/render.o: src/render.c src/array.h src/error.h src/memory.h \
	src/render.h
src/search.o: src/search.c src/error.h src/memory.h src/table.h src/tree.h \
	src/textset.h src/termset.h src/stem.h \
	src/symtab.h src/wordscan.h src/render.h src/filter.h src/search.h
src/sentfilter.o: src/sentfilter.c src/private/sentsuppress.h \
	src/unicode/sentbreakprop.h src/error.h src/memory.h src/table.h \
	src/tree.h src/sentscan.h src/sentfilter.h
src/sentscan.o: src/sentscan.c src/unicode/sentbreakprop.h src/sentscan.h
src/stem.o: src/stem.c lib/libstemmer_c/include/libstemmer.h src/error.h \
	src/memory.h src/table.h src/textset.h src/wordscan.h src/stem.h
src/stopword.o: src/stopword.c src/stopword.h
src/symtab.o: src/symtab.c src/array.h src/error.h src/memory.h src/table.h \
	src/textset.h src/symtab.h
src/table.o: src/table.c src/error.h src/memory.h src/table.h
src/termset.o: src/termset.c src/array.h src/error.h src/memory.h src/table.h \
	src/tree.h src/termset.h
src/textset.o: src/textset.c src/array.h src/error.h src/memory.h src/table.h \
	src/textset.h
src/tree.o: src/tree.c src/array.h src/error.h src/memory.h src/table.h \
	src/tree.h
src/wordscan.o: src/wordscan.c src/error.h \
	src/unicode/wordbreakprop.h src/wordscan.h

tests/check_census.o: tests/check_census.c src/table.h src/census.h \
	tests/testutil.h
tests/check_data.o: tests/check_data.c src/error.h src/table.h \
	src/textset.h src/symtab.h src/data.h \
	src/datatype.h tests/testutil.h
tests/check_filter.o: tests/check_filter.c src/table.h \
	src/textset.h src/tree.h src/stem.h src/symtab.h \
	src/wordscan.h src/render.h src/filter.h src/census.h tests/testutil.h
tests/check_intset.o: tests/check_intset.c src/table.h src/intset.h \
	tests/testutil.h
tests/check_ngram.o: tests/check_ngram.c src/table.h src/tree.h src/ngram.h \
	tests/testutil.h
tests/check_search.o: tests/check_search.c src/table.h src/tree.h \
	src/termset.h src/textset.h src/stem.h \
	src/symtab.h src/wordscan.h src/render.h src/filter.h src/search.h \
	tests/testutil.h
tests/check_sentfilter.o: tests/check_sentfilter.c src/table.h \
	src/tree.h src/sentscan.h src/sentfilter.h tests/testutil.h
tests/check_sentscan.o: tests/check_sentscan.c src/sentscan.h tests/testutil.h
tests/check_stem.o: tests/check_stem.c src/table.h src/textset.h \
	src/stem.h tests/testutil.h
tests/check_stopword.o: tests/check_stopword.c src/stopword.h tests/testutil.h
tests/check_symtab.o: tests/check_symtab.c src/table.h \
	src/textset.h src/symtab.h tests/testutil.h
tests/check_termset.o: tests/check_termset.c src/table.h src/tree.h \
	src/termset.h tests/testutil.h
tests/check_tree.o: tests/check_tree.c src/table.h src/tree.h tests/testutil.h
tests/check_wordscan.o: tests/check_wordscan.c src/wordscan.h tests/testutil.h
tests/testutil.o: tests/testutil.c tests/testutil.h
