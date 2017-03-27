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
LIB_O	= src/array.o src/filebuf.o src/symtab.o src/table.o src/text.o \
		  src/token.o src/unicode.o src/xalloc.o

DATA    = data/ucd/CaseFolding.txt data/ucd/UnicodeData.txt

TESTS_T = tests/check_text tests/check_token tests/check_unicode
TESTS_O = tests/check_text.o tests/check_token.o tests/check_unicode.o \
		  tests/testutil.o

TESTS_DATA = data/ucd/NormalizationTest.txt

ALL_O = $(LIB_O)
ALL_T = $(CORPUS_A)
ALL_A = $(CORPUS_A)

all: $(ALL_T)

$(CORPUS_A): $(LIB_O)
	$(AR) $@ $(LIB_O)
	$(RANLIB) $@

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


# Tests

tests/check_text: tests/check_text.o tests/testutil.o $(CORPUS_A)
	$(CC) -o $@ $(LDFLAGS) $(LIBS) $(CHECK_LIBS) $^

tests/check_token: tests/check_token.o tests/testutil.o $(CORPUS_A)
	$(CC) -o $@ $(LDFLAGS) $(LIBS) $(CHECK_LIBS) $^

tests/check_unicode: tests/check_unicode.o $(CORPUS_A) \
		data/ucd/NormalizationTest.txt
	$(CC) -o $@ $(LDFLAGS) $(LIBS) $(CHECK_LIBS) \
		tests/check_unicode.o $(CORPUS_A)


# Special Rules

clean:
	$(RM) -r $(ALL_O) $(ALL_T) $(TESTS_O) $(TESTS_T)

check: $(TESTS_T) $(TESTS_T:=.test)

doc:
	doxygen

%.test: %
	$<

tests/%.o: tests/%.c
	$(CC) -c $(CFLAGS) $(CHECK_CFLAGS) $(CPPFLAGS) $< -o $@

.PHONY: all check clean doc

src/array.o: src/array.c src/errcode.h src/xalloc.h src/array.h
src/filebuf.o: src/filebuf.c src/array.h src/errcode.h src/xalloc.h \
    src/filebuf.h
src/symtab.o: src/symtab.c src/array.h src/errcode.h src/table.h src/text.h \
	src/token.h src/xalloc.h src/symtab.h
src/table.o: src/table.c src/errcode.h src/xalloc.h src/table.h
src/text.o: src/text.c src/errcode.h src/unicode.h src/xalloc.h src/text.h
src/token.o: src/token.c src/errcode.h src/text.h src/unicode.h src/xalloc.h \
    src/token.h
src/unicode.o: src/unicode.c src/unicode/casefold.h src/unicode/combining.h \
    src/unicode/decompose.h src/errcode.h src/unicode.h
src/xalloc.o: src/xalloc.c src/xalloc.h

tests/check_text.o: tests/check_text.c src/text.h src/unicode.h tests/testutil.h
tests/check_token.o: tests/check_token.c src/text.h src/token.h src/unicode.h \
    tests/testutil.h
tests/check_unicode.o: tests/check_unicode.c src/unicode.h tests/testutil.h
tests/testutil.o: tests/testutil.c src/text.h tests/testutil.h
