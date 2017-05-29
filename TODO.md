Todo
====

## Features

  * Some sort of index for allowing KWIC queries.


## Sentence Segmentation

Handle abbreviations correctly. Some references:

  * http://www.unicode.org/reports/tr35/tr35-general.html#Segmentation_Exceptions
  * http://icu-project.org/apiref/icu4j/com/ibm/icu/text/FilteredBreakIteratorBuilder.html

  * https://github.com/stanfordnlp/CoreNLP/blob/master/src/edu/stanford/nlp/process/PTBLexer.flex#L707

  * https://tech.grammarly.com/blog/posts/How-to-Split-Sentences.html


The [CLDR](http://cldr.unicode.org/) data is at

    http://www.unicode.org/Public/cldr/31.0.1/

    http://unicode.org/Public/cldr/latest

Download `core.zip`. The files are in the `common/segments` directory.

Basically, you have a list of of "suppressions" (Mr., Dr., Nov., ...) and
you make sure never to break a sentence after one of these suppressions. The
data seems a little incomplete (e.g., some months are there, but not "Apr.",
"Jun.", "Jul.", "Oct."). Maybe take some abbreviations from the CoreNLP lexer.

ICU implementation:

   * icu4c/source/common/filteredbrk.cpp
