#!/usr/bin/env python3

# Copyright 2016 Patrick O. Perry.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# figure out the changes in word break properties for characters under
# compatibility decompositions

import math

try:
    import property
    import unicode_data
except ModuleNotFoundError:
    from util import property
    from util import unicode_data


WORD_BREAK_PROPERTY = "data/ucd/auxiliary/WordBreakProperty.txt"
PROP_LIST = "data/ucd/PropList.txt"
DERIVED_CORE_PROPERTIES = "data/ucd/DerivedCoreProperties.txt"

code_props = property.read(WORD_BREAK_PROPERTY)
word_break_property = property.read(WORD_BREAK_PROPERTY, sets=True)

prop_list = property.read(PROP_LIST, sets=True)
white_space = prop_list['White_Space']

derived_core_properties = property.read(DERIVED_CORE_PROPERTIES, sets=True)
default_ignorable = derived_core_properties['Default_Ignorable_Code_Point']

for code in range(len(unicode_data.uchars)):
    u = unicode_data.uchars[code]
    if u is None or u.decomp is None:
        continue
    d = unicode_data.decompose(code)
    if d is None:
        continue
    p0 = code_props[code] or 'None'
    p1 = [code_props[d_i] or 'None' for d_i in d]
    if p0 != p1[0]:
        print("{:04X} {} {} -> {}".format(code, u.name, p0, p1))
