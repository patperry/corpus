#!/usr/bin/env python3

import glob
import os.path


files = glob.glob('data/snowball/*.txt')
if len(files) == 0:
    files = glob.glob('../data/snowball/*.txt')

names = []
stopwords = {}

for fn in files:
    name = os.path.basename(fn)[:-len('.txt')]
    names.append(name)

    if name == 'russian':
        enc = 'windows-1251'
    else:
        enc = 'latin-1'

    with open(fn, encoding=enc) as f:
        lines = f.readlines()

    words = []
    for l in lines:
        w = l.partition('|')[0].strip()
        if len(w) > 0:
            words.extend(w.split())
    if name == 'english':
        words.append('will')
    words.sort()
    stopwords[name] = words

def encode(word):
    enc = ['"']
    for c in bytes(word, 'utf-8'):
        if c < 128:
            enc.append(chr(c))
        else:
            enc.append('\\{0:o}'.format(c))
    enc.append('"')
    return ''.join(enc)


print("/* This file is automatically generated. DO NOT EDIT!")
print("   Instead, edit gen-stopwords.py and re-run.  */")
print("")
print("#ifndef STOPWORDS_H")
print("#define STOPWORDS_H")
print("")
print("#include <stddef.h>")
print("#include <stdint.h>")
print("#include <string.h>")
print("")
print("struct stopword_list {")
print("\tconst char *name;")
print("\tint offset;")
print("\tint length;")
print("};")
print("")
print("static const char *stopword_list_names[] = {")
for i in range(len(names)):
    name = names[i]
    print("\t\"", name, "\"", sep="", end="")
    if i + 1 != len(names):
        print(",")
    else:
        print(",")
        print("\tNULL")
print("};")
print("")
print("static struct stopword_list stopword_lists[] = {")
off = 0
for i in range(len(names)):
    name = names[i]
    words = stopwords[name]
    print("\t{\"", name, "\", ", off, ", ", len(words), "}", sep="", end="")
    if i + 1 != len(names):
        print(",")
    else:
        print(",")
        print("\t{NULL, 0, 0}")
    off += len(words) + 1
print("};")
print("")
print("static const char * stopword_strings[] = {")
for i in range(len(names)):
    name = names[i]
    words = stopwords[name]
    j = 0
    if i > 0:
        print("\n")
    print("\t/*", name, "*/", end="")
    for w in words:
        if j % 1 == 0:
            print("\n\t", end="")
        else:
            print(" ", end="")
        print(encode(w), ",", sep="", end="")
        j += 1
    print("\n\tNULL", end="")
    if i + 1 < len(names):
        print(",", end="")
    else:
        print("")
print("};")
print("")
print("static const char **stopword_names(void)")
print("{")
print("\treturn (const char **)stopword_list_names;")
print("}")
print("")
print("static const uint8_t **stopword_list(const char *name, int *lenptr)")
print("{")
print("\tconst struct stopword_list *ptr = stopword_lists;")
print("")
print("\twhile (ptr->name != NULL && strcmp(ptr->name, name) != 0) {")
print("\t\tptr++;")
print("\t}")
print("")
print("\tif (ptr->name == NULL) {")
print("\t\tif(lenptr) {")
print("\t\t\t*lenptr = 0;")
print("\t\t}")
print("\t\treturn NULL;")
print("\t}")
print("")
print("\tif(lenptr) {")
print("\t\t*lenptr = ptr->length;")
print("\t}")
print("\treturn (const uint8_t **)(stopword_strings + ptr->offset);")
print("}")
print("")
print("#endif /* STOPWORDS_H */")
