Todo
====

## Features

  
  * Use Unicdoe character class table in isspace()

  * Some sort of index for allowing KWIC queries.

  * URL, twitter handling. Notes:
    + CoreNLP/src/edu/stanford/nlp/process/PTBLexer.flex

    + allowed: alphanumerics, the special characters "$-_.+!*'(),", and
      reserved characters used for their reserved purposes may be used
      unencoded within a URL.

    + reserved: ";", "/", "?", ":", "@", "=" and "&"

    + encode a byte: %XX (hex) UTF-8 allowed
