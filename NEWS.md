
# corpus 0.3.0

* Add NFC Unicode normalization; use this instead of NFD in typemap

* Add stemming (using the Snowball library)


# corpus 0.2.0

* Remove syslog in favor of custom logging in `error.h` interface;
  log to `stderr` by default.

* Switch to simpler `filebuf.h` interface.

* Add Windows portability code (tested on mingw64).


# corpus 0.1.0

* First milestone, with support for JSON decoding, text segmentation,
  and text normalization.
