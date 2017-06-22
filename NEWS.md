# corpus 0.7.0

* Added ability to search for specific terms.

* Added unicode character widths.


# corpus 0.6.0

* Added the ability to iterate backwards over the characters in a text.

* Added n-gram frequency counter.

* Added `termset` for storing a set of terms.

* Added `sentfitler` for suppressing sentence breaks.


# corpus 0.5.0

* Added new `filter` module with options for dropping, combining, and
  selecting terms from a token stream.

* Add the ability to exempt words from stemming.


# corpus 0.4.0

* Prefix functions and data types with `corpus_`, to avoid namespace
  clashes.

* Add stopwords (from the Snowball library).

* Fix Linux portability errors and warnings; everything should compile
  smoothly on Linux now.

* Simplify word types to None, Number, Letter, Kana, and Ideo, using
  the same definitions as the ICU library.


# corpus 0.3.0

* Add NFC Unicode normalization; use this instead of NFD in typemap.

* Add stemming (using the Snowball library).


# corpus 0.2.0

* Remove syslog in favor of custom logging in `error.h` interface;
  log to `stderr` by default.

* Switch to simpler `filebuf.h` interface.

* Add Windows portability code (tested on mingw64).


# corpus 0.1.0

* First milestone, with support for JSON decoding, text segmentation,
  and text normalization.
