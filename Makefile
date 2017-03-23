CC      = cc -std=c99
CFLAGS  = -Wall -Wextra -pedantic -g -O2
LDFLAGS =
LIBS    = -lm

AR      = ar rcu
RANLIB  = ranlib
MKDIR_P = mkdir -p

CHECK_CFLAGS = `pkg-config --cflags check` \
	       -Wno-gnu-zero-variadic-macro-arguments
CHECK_LIBS = `pkg-config --libs check`

CORPUS_A = libcorpus.a
LIB_O	= src/array.o src/filebuf.o src/text.o src/xalloc.o

TESTS_T = tests/check_text
TESTS_O = tests/check_text.o tests/testutil.o

ALL_O = $(LIB_O)
ALL_T = $(CORPUS_A)
ALL_A = $(CORPUS_A)

all: $(ALL_T)

$(CORPUS_A): $(LIB_O)
	$(AR) $@ $(LIB_O)
	$(RANLIB) $@


# Tests

tests/check_text: tests/check_text.o tests/testutil.o $(CORPUS_A)
	$(CC) -o $@ $(LDFLAGS) $(LIBS) $(CHECK_LIBS) $^

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
src/text.o: src/text.c src/text.h
src/xalloc.o: src/xalloc.c src/xalloc.h
tests/testutil.o: tests/testutil.c src/text.h tests/testutil.h
