
import re

PATTERN = re.compile(r"""^([0-9A-Fa-f]+)        # (first code)
                          (\.\.([0-9A-Fa-f]+))? # (.. last code)?
                          \s*
                          ;                     # ;
                          \s*
                          (\w+)                 # (property name)
                          \s*
                          (\#.*)?$              # (# comment)?""", re.X)

UNICODE_MAX = 0x10FFFF


def read(filename):
    try:
        file = open(filename, "r")
    except FileNotFoundError:
        file = open("../" + filename, "r")
    
    code_props = [None] * (UNICODE_MAX + 1)
    prop_names = set()
    properties = set({})

    with file:
        for line in file:
            line = line.split("#")[0] # remove comment
            m = PATTERN.match(line)
            if m:
                first = int(m.group(1), 16)
                if m.group(3):
                    last = int(m.group(3), 16)
                else:
                    last = first
                name = m.group(4)
                for u in range(first, last + 1):
                    code_props[u] = name
                prop_names.add(name)

    return code_props
