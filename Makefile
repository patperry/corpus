CC      = gcc -std=c99
CFLAGS  = -Wall -Wextra -pedantic -g -O2
LDFLAGS =
LIBS    = -lm

AR      = ar rcu
RANLIB  = ranlib
MKDIR_P = mkdir -p
CURL    = curl

CHECK_CFLAGS = `pkg-config --cflags check` \
	       -Wno-gnu-zero-variadic-macro-arguments
CHECK_LIBS = `pkg-config --libs check`

UNICODE = http://www.unicode.org/Public/9.0.0

CORPUS_A = libcorpus.a
LIB_O	= lib/strntod_c.o lib/strntoimax.o src/array.o src/census.o \
		  src/data.o src/datatype.o src/error.o src/filebuf.o src/render.o \
		  src/sentscan.o src/symtab.o src/table.o src/text.o src/token.o \
		  src/unicode.o src/wordscan.o src/xalloc.o

SNOWBALL = lib/libstemmer_c
STEMMER_O = $(SNOWBALL)/src_c/stem_UTF_8_arabic.o \
			$(SNOWBALL)/src_c/stem_UTF_8_danish.o \
			$(SNOWBALL)/src_c/stem_UTF_8_dutch.o \
			$(SNOWBALL)/src_c/stem_UTF_8_english.o \
			$(SNOWBALL)/src_c/stem_UTF_8_finnish.o \
			$(SNOWBALL)/src_c/stem_UTF_8_french.o \
			$(SNOWBALL)/src_c/stem_UTF_8_german.o \
			$(SNOWBALL)/src_c/stem_UTF_8_hungarian.o \
			$(SNOWBALL)/src_c/stem_UTF_8_italian.o \
			$(SNOWBALL)/src_c/stem_UTF_8_norwegian.o \
			$(SNOWBALL)/src_c/stem_UTF_8_porter.o \
			$(SNOWBALL)/src_c/stem_UTF_8_portuguese.o \
			$(SNOWBALL)/src_c/stem_UTF_8_romanian.o \
			$(SNOWBALL)/src_c/stem_UTF_8_russian.o \
			$(SNOWBALL)/src_c/stem_UTF_8_spanish.o \
			$(SNOWBALL)/src_c/stem_UTF_8_swedish.o \
			$(SNOWBALL)/src_c/stem_UTF_8_tamil.o \
			$(SNOWBALL)/src_c/stem_UTF_8_turkish.o \
			$(SNOWBALL)/runtime/api.o \
			$(SNOWBALL)/runtime/utilities.o \
			$(SNOWBALL)/libstemmer/libstemmer_utf8.o

CORPUS_T = corpus
CORPUS_O = src/main.o src/main_get.o src/main_scan.o src/main_sentences.o \
		   src/main_tokens.o

DATA    = data/ucd/CaseFolding.txt \
		  data/ucd/CompositionExclusions.txt \
		  data/ucd/auxiliary/SentenceBreakProperty.txt \
		  data/ucd/UnicodeData.txt \
		  data/ucd/auxiliary/WordBreakProperty.txt

TESTS_T = tests/check_census tests/check_data tests/check_sentscan  \
		  tests/check_symtab tests/check_text tests/check_token \
		  tests/check_unicode tests/check_wordscan
TESTS_O = tests/check_census.o tests/check_data.o tests/check_sentscan.o \
		  tests/check_symtab.o tests/check_text.o tests/check_token.o \
		  tests/check_unicode.o tests/check_wordscan.o tests/testutil.o

TESTS_DATA = data/ucd/NormalizationTest.txt \
			 data/ucd/auxiliary/SentenceBreakTest.txt \
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
	$(CC) -o $@ $(LDFLAGS) $(CORPUS_O) $(CORPUS_A) $(LIBS)


# Data

data/ucd/CaseFolding.txt:
	$(MKDIR_P) data/ucd
	$(CURL) -o $@ $(UNICODE)/ucd/CaseFolding.txt

data/ucd/CompositionExclusions.txt:
	$(MKDIR_P) data/ucd
	$(CURL) -o $@ $(UNICODE)/ucd/CompositionExclusions.txt

data/ucd/NormalizationTest.txt:
	$(MKDIR_P) data/ucd
	$(CURL) -o $@ $(UNICODE)/ucd/NormalizationTest.txt

data/ucd/auxiliary/SentenceBreakProperty.txt:
	$(MKDIR_P) data/ucd/auxiliary
	$(CURL) -o $@ $(UNICODE)/ucd/auxiliary/SentenceBreakProperty.txt

data/ucd/auxiliary/SentenceBreakTest.txt:
	$(MKDIR_P) data/ucd/auxiliary
	$(CURL) -o $@ $(UNICODE)/ucd/auxiliary/SentenceBreakTest.txt

data/ucd/UnicodeData.txt:
	$(MKDIR_P) data/ucd
	$(CURL) -o $@ $(UNICODE)/ucd/UnicodeData.txt

data/ucd/auxiliary/WordBreakProperty.txt:
	$(MKDIR_P) data/ucd/auxiliary
	$(CURL) -o $@ $(UNICODE)/ucd/auxiliary/WordBreakProperty.txt

data/ucd/auxiliary/WordBreakTest.txt:
	$(MKDIR_P) data/ucd/auxiliary
	$(CURL) -o $@ $(UNICODE)/ucd/auxiliary/WordBreakTest.txt


# Generated Sources

src/unicode/casefold.h: util/gen-casefold.py \
		data/ucd/CaseFolding.txt
	$(MKDIR_P) src/unicode
	./util/gen-casefold.py > $@

src/unicode/combining.h: util/gen-combining.py \
		data/ucd/UnicodeData.txt
	$(MKDIR_P) src/unicode
	./util/gen-combining.py > $@

src/unicode/compose.h: util/gen-compose.py \
		data/ucd/CompositionExclusions.txt data/ucd/UnicodeData.txt
	$(MKDIR_P) src/unicode
	./util/gen-compose.py > $@

src/unicode/decompose.h: util/gen-decompose.py \
		data/ucd/UnicodeData.txt
	$(MKDIR_P) src/unicode
	./util/gen-decompose.py > $@

src/unicode/sentbreakprop.h: util/gen-sentbreak.py \
		data/ucd/auxiliary/SentenceBreakProperty.txt
	$(MKDIR_P) src/unicode
	./util/gen-sentbreak.py > $@

src/unicode/wordbreakprop.h: util/gen-wordbreak.py \
		data/ucd/auxiliary/WordBreakProperty.txt
	$(MKDIR_P) src/unicode
	./util/gen-wordbreak.py > $@


# Tests

tests/check_census: tests/check_census.o tests/testutil.o $(CORPUS_A)
	$(CC) -o $@ $(LDFLAGS) $(LIBS) $(CHECK_LIBS) $^

tests/check_data: tests/check_data.o tests/testutil.o $(CORPUS_A)
	$(CC) -o $@ $(LDFLAGS) $(LIBS) $(CHECK_LIBS) $^

tests/check_sentscan: tests/check_sentscan.o tests/testutil.o $(CORPUS_A) \
		data/ucd/auxiliary/SentenceBreakTest.txt
	$(CC) -o $@ $(LDFLAGS) $(LIBS) $(CHECK_LIBS)  \
		tests/check_sentscan.o tests/testutil.o $(CORPUS_A)

tests/check_symtab: tests/check_symtab.o tests/testutil.o $(CORPUS_A)
	$(CC) -o $@ $(LDFLAGS) $(LIBS) $(CHECK_LIBS) $^

tests/check_text: tests/check_text.o tests/testutil.o $(CORPUS_A)
	$(CC) -o $@ $(LDFLAGS) $(LIBS) $(CHECK_LIBS) $^

tests/check_token: tests/check_token.o tests/testutil.o $(CORPUS_A)
	$(CC) -o $@ $(LDFLAGS) $(LIBS) $(CHECK_LIBS) $^

tests/check_unicode: tests/check_unicode.o $(CORPUS_A) \
		data/ucd/NormalizationTest.txt
	$(CC) -o $@ $(LDFLAGS) $(LIBS) $(CHECK_LIBS) \
		tests/check_unicode.o $(CORPUS_A)

tests/check_wordscan: tests/check_wordscan.o tests/testutil.o $(CORPUS_A) \
		data/ucd/auxiliary/WordBreakTest.txt
	$(CC) -o $@ $(LDFLAGS) $(LIBS) $(CHECK_LIBS)  \
		tests/check_wordscan.o tests/testutil.o $(CORPUS_A)


# Special Rules

clean:
	$(RM) -r $(ALL_O) $(ALL_T) $(TESTS_O) $(TESTS_T)

check: $(TESTS_T) $(TESTS_T:=.test)

data: $(DATA) $(TESTS_DATA)

doc:
	doxygen

%.test: %
	$<

tests/%.o: tests/%.c
	$(CC) -c $(CFLAGS) $(CHECK_CFLAGS) $(CPPFLAGS) $< -o $@

.PHONY: all check clean data doc


src/array.o: src/array.c src/error.h src/xalloc.h src/array.h
src/census.o: src/census.c src/array.h src/error.h src/table.h \
	src/xalloc.h src/census.h
src/data.o: src/data.c src/error.h src/table.h src/text.h src/token.h \
	src/symtab.h src/datatype.h src/data.h
src/datatype.o: src/datatype.c src/array.h src/error.h src/render.h \
	src/table.h src/text.h src/token.h src/symtab.h src/xalloc.h \
	src/data.h src/datatype.h
src/error.o: src/error.c src/error.h
src/filebuf.o: src/filebuf.c src/error.h src/xalloc.h src/filebuf.h
src/main.o: src/main.c src/error.h src/filebuf.h src/table.h src/text.h \
	src/token.h src/symtab.h src/datatype.h
src/main_get.o: src/main_get.c src/error.h src/filebuf.h src/table.h \
	src/text.h src/token.h src/symtab.h src/datatype.h
src/main_scan.o: src/main_scan.c src/error.h src/filebuf.h src/table.h \
	src/text.h src/token.h src/symtab.h src/datatype.h
src/main_sentences.o: src/main_sentences.c src/error.h src/filebuf.h \
	src/sentscan.h src/table.h src/text.h src/data.h src/datatype.h
src/main_tokens.o: src/main_tokens.c src/error.h src/filebuf.h src/table.h \
	src/text.h src/token.h src/symtab.h src/data.h src/datatype.h src/wordscan.h
src/render.o: src/render.c src/array.h src/error.h src/text.h \
	src/unicode.h src/xalloc.h src/render.h
src/sentscan.o: src/sentscan.c src/text.h src/unicode/sentbreakprop.h \
	src/sentscan.h
src/symtab.o: src/symtab.c src/array.h src/error.h src/table.h src/text.h \
	src/token.h src/xalloc.h src/symtab.h
src/table.o: src/table.c src/error.h src/xalloc.h src/table.h
src/text.o: src/text.c src/error.h src/unicode.h src/xalloc.h src/text.h
src/token.o: src/token.c src/error.h src/text.h src/unicode.h src/xalloc.h \
    src/token.h
src/unicode.o: src/unicode.c src/unicode/casefold.h src/unicode/combining.h \
    src/unicode/compose.h src/unicode/decompose.h src/error.h src/unicode.h
src/wordscan.o: src/wordscan.c src/text.h src/unicode/wordbreakprop.h \
	src/wordscan.h
src/xalloc.o: src/xalloc.c src/xalloc.h

tests/check_census.o: tests/check_census.c src/table.h src/census.h \
	tests/testutil.h
tests/check_data.o: tests/check_data.c src/error.h src/table.h src/text.h \
	src/token.h src/symtab.h src/data.h src/datatype.h tests/testutil.h
tests/check_sentscan.o: tests/check_sentscan.c src/text.h src/token.h \
	src/unicode.h src/wordscan.h tests/testutil.h
tests/check_symtab.o: tests/check_symtab.c src/table.h src/text.h src/token.h \
	src/symtab.h tests/testutil.h
tests/check_text.o: tests/check_text.c src/error.h src/text.h src/unicode.h \
	tests/testutil.h
tests/check_token.o: tests/check_token.c src/text.h src/token.h src/unicode.h \
    tests/testutil.h
tests/check_unicode.o: tests/check_unicode.c src/unicode.h tests/testutil.h
tests/check_sentscan: tests/check_sentscan.c src/text.h src/token.h \
	src/unicode.h src/wordscan.h tests/testutil.h
tests/check_wordscan: tests/check_wordscan.c src/text.h src/token.h \
	src/unicode.h src/wordscan.h tests/testutil.h
tests/testutil.o: tests/testutil.c src/text.h tests/testutil.h
