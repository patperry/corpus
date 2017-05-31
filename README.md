Corpus (C Library)
==================

[![Build Status](https://api.travis-ci.org/patperry/corpus.svg?branch=master)](https://travis-ci.org/patperry/corpus)
[![Coverage Status](https://codecov.io/github/patperry/corpus/coverage.svg?branch=master)](https://codecov.io/github/patperry/corpus?branch=master)


Text corpus analysis.


Overview
--------

Corpus is C library for analyzing text data, with full support for Unicode. It
is designed to support processing large data files that do not fit into
memory.


Features
--------

### JSON support

In principle Corpus can support arbitrary input, but it is designed to
analyze text stored in [JavaScript Object Notation (JSON)][json] format. The
`text` objects provided by the library can either refer to raw UTF-8 encoded
text, or they can refer to a JSON string with backslash (`\`) escapes like
`\n`, `\t`, and `\u2603`. On 64-bit architectures, text objects take 16 bytes:
an 8-byte pointer to the data, 1 bit indicating whether to interpret backslash
as a JSON-style escape, 1 bit indicating whether the text includes a byte
sequence that may decode to a non-ASCII character, and 62 bits to store the
encoding size, in bytes.

In typical usage, you will memory-map a [newline-demimited JSON][ndjson]
data file, validate and type-check the data values using the `datatype.h`
interface, extract the appropriate fields using the `data.h` interface, and
create text objects that point into the file. By memory mapping the file,
you can let the operating system move data between the hard drive and RAM
whenever necessary.  You can process a large data set seamlessly without
loading everything into RAM at the same time.

For more information on JSON support in Corpus, see the notes on
[JSON as understood by Corpus][corpus-json].


### Text segmentation

Corpus can segment text into sentences or words according to the rules
described in [Unicode Standard Annex #29][segmentation]. To segment text
into sentences or words, use the `sentscan.h` or `wordscan.h` interface,
respectively.


### Text normalization

Corpus supports the following text normalization transformations:

 + transforming to Unicode NFC or NFKC normal form;

 + performing Unicode case folding (using the default mappings, not
   the locale-specific ones);

 + performing quote folding, replacing quote characters like single
   quotes, double quotes, and apostrophes with ASCII single quote (`'`);

 + removing Unicode default ignorable characters like zero-width-space
   and soft hyphen;

 + stemming, using one of the algorithms supported by the [Snowball][snowball]
   stemming library.

These normalizations can be applied to arbitrary text, but they are designed
to be applied to individual word tokens, so that the results can be cached
and re-used. The `symtab.h` and `token.h` interfaces support these
normalizations.


R interface
-----------

Corpus is designed to be embedded into other language environments. The
[R interface][rcorpus] is under development concurrently with the library.



Building from source
--------------------

There are no dependencies, but to build the documentation, you will need the
[Doxygen][doxygen] program, and to build the tests, you will need
to install the [Check Unit Testing][check] library. Running `make` will
build `libcorpus.a` and the `corpus` command-line tool. Running `make doc`
will run Doxygen to build the documentation. Running `make check` will
run the tests.


Windows support
---------------

Everything should work on Windows, the only platform-specific code is the
memory-mapping used internally by the `filebuf.h` interface.


License
-------

Corpus is released under the [Apache Licence, Version 2.0][apache]. The
stemming algorithms used by Corpus come from the [Snowball][snowball]
library and are subject to the conditions of the
[3-clause BSD license][snowball-lic]. Portions of Corpus rely on data
from the [Unicode Character Database][ucd] and are
subject to the terms of the [Unicode Licence][unicode].


[apache]: https://www.apache.org/licenses/LICENSE-2.0.html
[check]: https://libcheck.github.io/check/
[corpus-json]: https://github.com/patperry/corpus/blob/master/doc/json.md
[doxygen]: http://www.stack.nl/~dimitri/doxygen/
[json]: http://www.json.org/
[ndjson]: http://ndjson.org/
[rcorpus]: https://github.com/patperry/r-corpus
[segmentation]: http://unicode.org/reports/tr29/
[snowball]: http://snowballstem.org/
[snowball-lic]: http://snowballstem.org/license.html
[ucd]: http://unicode.org/ucd/
[unicode]: http://www.unicode.org/copyright.html#License
