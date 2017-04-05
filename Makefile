CC      = cc -std=c99
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

UNICODE = http://www.unicode.org/Public/8.0.0

CORPUS_A = libcorpus.a
LIB_O	= lib/strntod_c.o lib/strntoimax.o src/array.o src/data.o \
		  src/datatype.o src/filebuf.o src/symtab.o src/table.o \
		  src/text.o src/token.o src/unicode.o src/wordscan.o \
		  src/xalloc.o

CORPUS_T = corpus
CORPUS_O = src/main.o

DATA    = data/ucd/CaseFolding.txt data/ucd/UnicodeData.txt \
		  data/ucd/auxiliary/WordBreakProperty.txt

TESTS_T = tests/check_data tests/check_symtab tests/check_text \
		  tests/check_token tests/check_unicode tests/check_wordscan
TESTS_O = tests/check_data.o tests/check_symtab.o tests/check_text.o \
		  tests/check_token.o tests/check_unicode.o tests/check_wordscan.o \
		  tests/testutil.o

TESTS_DATA = data/ucd/NormalizationTest.txt \
			 data/ucd/auxiliary/WordBreakTest.txt

ALL_O = $(LIB_O) $(CORPUS_O)
ALL_T = $(CORPUS_A) $(CORPUS_T)
ALL_A = $(CORPUS_A)


# Products

all: $(ALL_T)

$(CORPUS_A): $(LIB_O)
	$(AR) $@ $(LIB_O)
	$(RANLIB) $@

$(CORPUS_T): $(CORPUS_O) $(CORPUS_A)
	$(CC) -o $@ $(LDFLAGS) $(CORPUS_O) $(CORPUS_A) $(LIBS)


# Data

data/ucd/CaseFolding.txt:
	$(MKDIR_P) data/ucd
	$(CURL) -o $@ $(UNICODE)/ucd/CaseFolding.txt

data/ucd/NormalizationTest.txt:
	$(MKDIR_P) data/ucd
	$(CURL) -o $@ $(UNICODE)/ucd/NormalizationTest.txt

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

src/unicode/decompose.h: util/gen-decompose.py \
		data/ucd/UnicodeData.txt
	$(MKDIR_P) src/unicode
	./util/gen-decompose.py > $@

src/unicode/wordbreakprop.h: util/gen-wordbreak.py \
		data/ucd/auxiliary/WordBreakProperty.txt
	$(MKDIR_P) src/unicode
	./util/gen-wordbreak.py > $@


# Tests

tests/check_data: tests/check_data.o tests/testutil.o $(CORPUS_A)
	$(CC) -o $@ $(LDFLAGS) $(LIBS) $(CHECK_LIBS) $^

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


src/array.o: src/array.c src/errcode.h src/xalloc.h src/array.h
src/data.o: src/data.c src/errcode.h src/table.h src/text.h src/token.h \
	src/symtab.h src/datatype.h src/data.h
src/datatype.o: src/datatype.c src/array.h src/errcode.h src/table.h \
	src/text.h src/token.h src/symtab.h src/xalloc.h src/data.h src/datatype.h
src/filebuf.o: src/filebuf.c src/array.h src/errcode.h src/xalloc.h \
    src/filebuf.h
src/main.o: src/main.c src/errcode.h src/filebuf.h src/table.h src/text.h \
	src/token.h src/symtab.h src/datatype.h
src/symtab.o: src/symtab.c src/array.h src/errcode.h src/table.h src/text.h \
	src/token.h src/xalloc.h src/symtab.h
src/table.o: src/table.c src/errcode.h src/xalloc.h src/table.h
src/text.o: src/text.c src/errcode.h src/unicode.h src/xalloc.h src/text.h
src/token.o: src/token.c src/errcode.h src/text.h src/unicode.h src/xalloc.h \
    src/token.h
src/unicode.o: src/unicode.c src/unicode/casefold.h src/unicode/combining.h \
    src/unicode/decompose.h src/errcode.h src/unicode.h
src/wordscan.o: src/wordscan.c src/wordscan.h
src/xalloc.o: src/xalloc.c src/xalloc.h

tests/check_data.o: tests/check_data.c src/errcode.h src/table.h src/text.h \
	src/token.h src/symtab.h src/data.h src/datatype.h tests/testutil.h
tests/check_symtab.o: tests/check_symtab.c src/table.h src/text.h src/token.h \
	src/symtab.h tests/testutil.h
tests/check_text.o: tests/check_text.c src/text.h src/unicode.h tests/testutil.h
tests/check_token.o: tests/check_token.c src/text.h src/token.h src/unicode.h \
    tests/testutil.h
tests/check_unicode.o: tests/check_unicode.c src/unicode.h tests/testutil.h
tests/check_wordscan: tests/check_wordscan.c src/text.h src/token.h \
	src/unicode.h src/wordscan.h tests/testutil.h
tests/testutil.o: tests/testutil.c src/text.h tests/testutil.h
