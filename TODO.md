Todo
====

C library
---------

Text segmentation according to [UAX #29][uax29]:

 - sentences.

Update type normalizer (dash, quote folding) to Unicode 10.0.


R interface
-----------

`read_json()` for reading JSON Lines, returning a `dataset` object.

`dataset.$` for extracting record fields. Arrays de-serialized as lists.

`dataset.[` for subsetting.

`text` data type.

`as.character.text()` for type conversion.

`tokens()` for word tokens.

`sentences()` for sentences.

`blocks()` for fixed-length chunks of words. Read up on time series block
bootstrap to get inspiration for interface.

`word_counts()` for word count matrix; groups argument for collapsing rows.

Bootstrapping. Not sure what the interface should look like, but should
support sentence-level, word-level, block-level.


[uax29]: http://unicode.org/reports/tr29/
