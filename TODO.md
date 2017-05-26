Todo
====

## Bugs

  * Need to set the word type properly for Ideographic and Kana words. See
    icu4c/source/data/brkitr/rules/word.txt for the rules that ICU uses, and
    icu4c/source/common/unicode/ubrk.h for the numeric values:

    None: [  0, 100)
    Num:  [100, 200)
    Let:  [200, 300)
    Kana: [300, 400)
    Ideo: [400, 500)

    It looks like type '300' (Kana) was removed; all Kana words get
    classified as Ideo.

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
