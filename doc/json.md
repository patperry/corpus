JSON as understood by Corpus
============================

Overview
--------

Corpus includes support for parsing data in [JavaScript Object Notation
(JSON)][json] format. Beyond that, Corpus can type-check data values to ensure
that their types are compatible. We use the latter facility to determine
column types for tabular data stored in [newline-delimited JSON (NDJSON)
format][ndjson].

Here, we describe the type system Corpus uses for JSON values, and we document
the differences between JSON as understood by Corpus and JSON as formally
specified. Finally, we describe the JSON parser, which follows a very
different model from most other JSON parsers.


Typed JSON values
-----------------

### The type lattice

Every JSON value has a type. Types can be arranged in a lattice. The special
type `Null` is the "bottom" type; all other types are super-types of `Null`.
The special type `Any` is the "top" type; all other types are sub-types
of `Any`. Besides `Null` and `Any`, every type has a sub-type and a
super-type.

                                 Any
                                  |
                                  |
           +--------+-------+-----+------+-----------------+
           |        |       |            |                 |
           |      Real      |     +-------------+   +--------------+
        Boolean     |      Text   | Array Types |   | Record Types |
           |     Integer    |     +-------------+   +--------------+
           |        |       |            |                 |
           +--------+-------+-----+------+-----------------+    
                                  |
                                  |
                                 Null


In addition to `Any` and `Null`, the other atomic types are `Boolean`,
`Integer`, `Real`, and `Text`. Array types and record types are compound
types, defined in terms of other types.

The lattice denotes the sub-type and super-type relations. For example, every
value of type `Integer` is also of type `Real` and of type `Any`.
Also, every value of type `Null` is also of type `Text`.


### Array types

Array types are parameterized by the element type and the length. We require
arrays to have homogeneous element types, but, since we allow elements to have
type `Any`, any valid JSON array has a valid type in our system. The length of
an array can either be a non-negative number, or it can be a special value
(`-1`) interpreted as "variable".


Let `Array(t, n)` denote an array with element type `t` and length `n`. We can
define the lattice of array types in terms of the smallest super-type of a
pair of array types (their parent):

  + The parent of `Array(t, n)` and `Array(u, n)` is `Array(parent(t, u), n)`,
    where `parent(e, f)` is the parent type of `e` and `f`.

  + The parent of `Array(t, n)` and `Array(u, m)` for unequal `m` and `n`
    is `Array(parent(t, u), -1)`.


### Record types

Conceptually, a record type is a map that associates a type to every field
name. That is, a record type has an infinite set of fields. In practice, most
fields have type `Null`, and we can represent this map efficiently as a set
of (name, type) pairs for the fields with non-`Null` types.


Suppose that `R` and `S` are two record types. Their parent type is another
record type, `P`. To define `P`, we just need to define the types for the
fields of `P`. To do this, let `f` be any field name, and suppose that `R[f]`
is the type of the field in record `R`, and `S[f]` is the type of the field in
record `S`. Then, we set the parent record type for this field to `P[f] =
parent(R[f], S[f])`.


### Examples

Here are some example JSON values along with their types:

    Value         Type
    -------------------------------
    null          Null
    true          Boolean
    -1            Integer
    3.14          Real
    "hello"       Text
    []            Array(Null, 0)
    [1,2]         Array(Integer, 2)
    [1,false,3]   Array(Any, 3)
    [1,null,2.2]  Array(Real, 3)

Here are some examples of parents of record types:

    parent({"a": Boolean}, {"b": Text}) = {"a": Boolean, "b": Text}

    parent({"a": Integer}, {"a": Real}) = {"a": Real}

    parent({"a": Integer, "b": Real},
           {"c": Text, "b": Integer}) = {"a": Integer, "b": Real, "c": Text}


### Decoding null values

In our type system, the value `null` can have any type. In practice, we
interpret `null` as a missing value (`NA` in R).


Differences from formal JSON
----------------------------

JSON as understood by Corpus mostly agrees with the formal JSON specification,
but there are a few differences, which we outline below.

### Encoding

Beyond requiring Unicode, formal JSON is agnostic to the encoding. Corpus
requires that the data be encoded in valid UTF-8.


### Numbers

Corpus has a broader definition of a "number" than is given in the formal JSON
specification:

 + Corpus allows a leading plus sign (`+`) before positive numbers. Formal
   JSON does not.

   *Examples:* `+1`, `+3.14`
   
 + When a number contains a decimal point, Corpus requires that there be
   at least one digit immediately preceding or immediately following the
   decimal point. Formal JSON requires *both* a leading and a following digit.

   *Examples:* `1.`, `.2`, `.2e-10`

 + Corpus interprets `Infinity` and `NaN` as infinity and not-a-number,
   respectively, and allows an optional sign (`+` or `-`) before these
   sequences.  This allows Corpus to parse the "JSON" generated by Python
   (`json.dumps(float('nan'))` or `json.dumps(float('inf'))`). Formal JSON
   does not allow these values.

   *Examples:* `Infinity`, `NaN`, `-NaN`


### Text (strings)

Corpus has a definition of a "text" that is more restrictive than a formal
JSON string:

 + Formal JSON allows arbitrary escape sequences of the form `\uXXXX` where
   `XXXX` is a sequence of four hexadecimal digits. Unlike formal JSON,
   when `XXXX` is a UTF-16 high surrogate, Corpus requires that the
   next sequence of characters in the string be `\uYYYY`, where `YYYY` is
   a UTF-16 low surrogate.

 + Unlike formal JSON, Corpus does allow escape sequences of the form `\uYYYY`
   where `YYYY` is a UTF-16 low surrogate unless the sequence is preceded by
   `\uXXXX`, where `XXXX` is a UTF-16 high surrogate.


### Arrays

Corpus does not allow arrays longer than `INT_MAX` elements (2147483647 on
most systems). Formal JSON does not place a limit on the maximum array
length, but JavaScript allows arrays up to [4294967295 elements][array-len].


### Records (objects)

Corpus has a definition of a "record" that is more restrictive than a formal
JSON object:

 + Corpus does not allow records to have more than `INT_MAX` fields.

 + Corpus requires that all field names be unique. Formal JSON does not.
   Moreover, Corpus requires that, when decoded to
   Unicode [Normalized Composed Form (NFC)][nfc] the normalized
   field names are all unique.

   *Examples:*
    `{" ": 1, "\u0020": 2}`,
    `{"\u00e8": 3, "e\u0300": 4}`
    (valid JSON but not accepted by Corpus)


Decoding JSON values
--------------------

Most JSON libraries decode JSON-encoded values in a single pass; whenever the
parser encounters a new value it calls a client-supplied callback function.
Corpus takes a different approach, which requires two passes over the input
data. In the first pass, Corpus validates the input data and determines its
type. In the second pass, the client decodes the typed value to a native type.


The first pass over the value (a call to `corpus_data_assign`) scans the input
and determines its type. If, in the process of scanning, Corpus encounters a
new data type, it adds this type to the passed-in schema object, assigning a
new integer ID for the type. After scanning, Corpus initializes a
`struct corpus_data` value containing a pointer to the encoded value, its size
(in bytes), and the integer ID of the value's type. The first pass over the
data does not allocate any memory, except to add new types to the data schema
if necessary.


Once the value has been typed, the client can use `corpus_data_bool`,
`corpus_data_int`, `corpus_data_double`, or `corpus_data_text` to decode the
value to a native type. If the value is an array, the client can iterate over
its values using the `corpus_data_items` function. If the value is a record,
the client can iterate over its fields using `corpus_data_fields`, or she can
access specific fields by name using the `corpus_data_field` function.


The relevant interfaces are [data.h][data_h] and [datatype.h][datatype_h].


[array-len]: https://stackoverflow.com/a/6155063
[data_h]: https://github.com/patperry/corpus/blob/master/src/data.h
[datatype_h]: https://github.com/patperry/corpus/blob/master/src/datatype.h
[json]: http://json.org/
[ndjson]: http://ndjson.org/
[nfc]: http://unicode.org/reports/tr15/
