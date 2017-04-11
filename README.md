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

In typical usage, you will memory-map a [JSON Lines][jsonlines] data file,
validate and type-check the data values using the `datatype.h` interface,
extract the appropriate fields using the `data.h` interface, and create text
objects that point into the file. By memory mapping the file, you can let the
operating system move data between the hard drive and RAM whenever necessary.
You can process a large data set seamlessly without loading everything into
RAM at the same time.


### Text segmentation

Corpus can segment text into sentences or words according to the rules
described in [Unicode Standard Annex #29][segmentation]. To segment text
into sentences or words, use the `sentscan.h` or `wordscan.h` interface,
respectively.


### Text normalization

Corpus supports the following text normalization transformations:

 + transforming to Unicode NFD or NFKD normal form;

 + performing Unicode case folding (using the default mappings, not
   the locale-specific ones);

 + performing dash folding, replacing dash characters like em dash
   and en dash with ASCII dash (`-`);

 + performing quote folding, replacing quote characters like single
   quotes, double quotes, and apostrophes with ASCII single quote (`'`);

 + removing non-white-space control characters like the ASCII bell;

 + removing Unicode default ignorable characters like zero-width-space
   and soft hyphen;

 + removing white space characters.

These normalizations can be applied to arbitrary text, but they are designed
to be applied to individual word tokens, so that the results can be cached
and re-used. The `symtab.h` and `token.h` interfaces support these
normalizations.


R interface
-----------

Corpus is designed to be embedded into other language environments. The
[R interface][rcorpus] is under development concurrently with the library.


License
-------

Corpus is released under the [Apache Licence, Version 2.0][apache].


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

We do not currently support Windows, due to two dependencies. First, we use
the UNIX-specific memory-map system calls. Second, we use the UNIX-specific
`syslog` interface for logging.  There are Windows equivalents to UNIX `mmap`,
so it should be straightforward to remove the first dependency. For the second
dependency, the plan is to eventually move away from `syslog` to a custom
logging interface.


[apache]: https://www.apache.org/licenses/LICENSE-2.0.html
[check]: https://libcheck.github.io/check/
[doxygen]: http://www.stack.nl/~dimitri/doxygen/
[json]: http://www.json.org/
[jsonlines]: http://jsonlines.org/
[rcorpus]: https://github.com/patperry/r-corpus
[segmentation]: http://unicode.org/reports/tr29/
