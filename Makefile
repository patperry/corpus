CC      = cc -std=c99
CFLAGS  = -Wall -Wextra -pedantic -g -O2
LDFLAGS =
LIBS    = -lm

AR      = ar rcu
RANLIB  = ranlib
MKDIR_P = mkdir -p

CORPUS_A = libcorpus.a
LIB_O	= src/array.o src/filebuf.o src/xalloc.o

ALL_O = $(LIB_O)
ALL_T = $(CORPUS_A)
ALL_A = $(CORPUS_A)

all: $(ALL_T)

$(CORPUS_A): $(LIB_O)
	$(AR) $@ $(LIB_O)
	$(RANLIB) $@


# Special Rules

clean:
	$(RM) -r $(ALL_O) $(ALL_T)

doc:
	doxygen

.PHONY: all clean doc

src/array.o: src/array.c src/errcode.h src/xalloc.h src/array.h
src/filebuf.o: src/filebuf.c src/array.h src/errcode.h src/xalloc.h \
	src/filebuf.h
src/xalloc.o: src/xalloc.c src/xalloc.h
