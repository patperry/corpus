Todo
====

## Features

  * Change `text_filter_combine()` to use `CORPUS_FILTER_SCAN_TOKENS`
    instead of `CORPUS_FILTER_SCAN_TOKENS`. This can only happen if stemming
    is done after combining.

  * Some sort of index for allowing KWIC queries.

  * URL, twitter handling. Notes:
    + CoreNLP/src/edu/stanford/nlp/process/PTBLexer.flex

    + allowed: alphanumerics, the special characters "$-_.+!*'(),", and
      reserved characters used for their reserved purposes may be used
      unencoded within a URL.

    + reserved: ";", "/", "?", ":", "@", "=" and "&"

    + encode a byte: %XX (hex) UTF-8 allowed
