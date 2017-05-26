# unicode_data

import collections
import re


UNICODE_DATA = 'data/ucd/UnicodeData.txt'
UNICODE_MAX = 0x10FFFF

try:
    unicode_data = open(UNICODE_DATA, "r")
except FileNotFoundError:
    unicode_data = open("../" + UNICODE_DATA, "r")

field_names = [
    'name',     # Name
    'category', # General_Category
    'ccc',      # Canonical_Combining_Class
    'bidi',     # Bidi_Class
    'decomp',   # Decomposition_Type, Decomposition_Mapping
    'decimal',  # Numeric_Value (Numeric_Type = Decimal)
    'digit',    # Numeric_Value (Numeric_Type = Decimal, Digit)
    'numeric',  # Numeric_Value (Numeric_Type = Decimal, Digit, Numeric)
    'mirrored', # Bidi_Mirrored
    'old_name', # Unicode_1_Name
    'comment',  # ISO_Comment
    'ucase',    # Simple_Uppercase_Mapping
    'lcase',    # Simple_Lowercase_Mapping
    'tcase'     # Simple_Titlecase_Mapping
    ]

UChar = collections.namedtuple('UChar', field_names)
ids = UChar._make(range(1, len(field_names) + 1))

Decomp = collections.namedtuple('Decomp', ['type', 'map'])

DECOMP_PATTERN = re.compile(r"""^(<(\w+)>)?\s* # decomposition type
                                 ((\s*[0-9A-Fa-f]+)+) # decomposition mapping
                                 \s*$""", re.X)
RANGE_PATTERN = re.compile(r"""^<([^,]+),\s*    # range name
                                 (First|Last) # first or last
                                 >$""", re.X)

def parse_decomp(code, field):
    if field != '':
        m = DECOMP_PATTERN.match(field)
        assert m
        d_type = m.group(2)
        d_map = tuple([int(x, 16) for x in m.group(3).split()])
        return Decomp(type=d_type, map=d_map)

    elif code in range(0xAC00, 0xD7A4):
        return Decomp(type='hangul', map=None)
    else:
        return None


def parse_code(field):
    if field != '':
        return int(field, 16)
    else:
        return None

def parse_int(field):
    if field != '':
        return int(field)
    else:
        return None


uchars = [None] * (UNICODE_MAX + 1)

with unicode_data:
    for line in unicode_data:
        fields = line.strip().split(';')
        code = int(fields[0], 16)
        uchars[code] = UChar(name = fields[ids.name],
                             category = fields[ids.category],
                             ccc = parse_int(fields[ids.ccc]),
                             bidi = fields[ids.bidi],
                             decomp = parse_decomp(code, fields[ids.decomp]),
                             decimal = fields[ids.decimal],
                             digit = fields[ids.digit],
                             numeric = fields[ids.numeric],
                             mirrored = fields[ids.mirrored],
                             old_name = fields[ids.old_name],
                             comment = fields[ids.comment],
                             ucase = parse_code(fields[ids.ucase]),
                             lcase = parse_code(fields[ids.lcase]),
                             tcase = parse_code(fields[ids.tcase]))


utype = None

for code in range(len(uchars)):
    u = uchars[code]
    if u is None:
        uchars[code] = utype
    else:
        m = RANGE_PATTERN.match(u.name)
        if m:
            if m.group(2) == 'First':
                utype = u._replace(name = '<' + m.group(1) + '>')
            else:
                utype = None
