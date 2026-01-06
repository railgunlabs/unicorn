#  Unicorn - Embeddable Unicode Algorithms
#  Copyright (c) 2024-2026 Railgun Labs
#
#  This software is dual-licensed: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License version 3 as
#  published by the Free Software Foundation. For the terms of this
#  license, see <https://www.gnu.org/licenses/>.
#
#  Alternatively, you can license this software under a proprietary
#  license, as set out in <https://railgunlabs.com/unicorn/license/>.

from typing import List, Dict, Any, Optional
from dataclasses import dataclass
from .basics import StorageSize, SerializationData

Codepoint = int

MAX_CODEPOINTS = 0x110000  # Defined by the Unicode Consortium.
BUCKET_SIZE = 128  # The number of codepoints in a single stage1 bucket.

@dataclass
class Block:
    name: str
    first: Codepoint
    last: Codepoint

    def __hash__(self) -> int:
        return hash(self.name)

    def __eq__(self, other: Any) -> bool:
        if isinstance(other, Block):
            return self.name == other.name
        return False

def parse_codepoints(chars: str) -> List[Codepoint]:
    return [int(cp, 16) for cp in chars.split()]

class CodespaceChar:
    def __init__(self, property_count: int) -> None:
        self.property_values = [0] * property_count

    def __hash__(self) -> int:
        hash_val = 5381
        for value in self.property_values:
            hash_val ^= hash(value)
        return hash_val

    def __eq__(self, other: Any) -> bool:
        if isinstance(other, CodespaceChar):
            if len(self.property_values) != len(other.property_values):
                return False
            for i in range(len(self.property_values)):
                if self.property_values[i] != other.property_values[i]:
                    return False
            return True
        return False

class Property:
    def __init__(self, index: int, name: str, default_value: int, storage_size: StorageSize) -> None:
        self.id = index
        self.field_name = name
        self.default_value = default_value
        self.storage_size = storage_size

class Codespace:
    def __init__(self) -> None:
        self.characters: Dict[Codepoint, CodespaceChar] = {}
        self.properties: Dict[str, Property] = {}
        self.default_character: Optional[CodespaceChar] = None

    def register_property(self, property: str, default_value: int, storage_size: StorageSize) -> int:
        if property not in self.properties:
            self.properties[property] = Property(len(self.properties), property, default_value, storage_size)
        return self.properties[property].id

    def get(self, property: str) -> int:
        return self.properties[property].id

    def finalize(self) -> None:
        # Instantiate the default character.
        self.default_character = CodespaceChar(len(self.properties))
        for p in self.properties.values():
            self.default_character.property_values[p.id] = p.default_value

    def _get(self, cp: Codepoint) -> CodespaceChar:
        if cp in self.characters:
            return self.characters[cp]
        else:
            # The character hasn't been modified yet; create it now and
            # set all its properties to the default value.
            char = CodespaceChar(len(self.properties))
            for p in self.properties.values():
                char.property_values[p.id] = p.default_value
            self.characters[cp] = char
            return char

    # Set a property on a character.
    def set(self, cp: Codepoint, property: int, value: int) -> None:
        self._get(cp).property_values[property] = value

    # Bitwise OR the property on a character.
    def set_bitwise(self, cp: Codepoint, property: int, value: int) -> None:
        self._get(cp).property_values[property] |= value

def staging_table(codespace: Codespace, struct: str, function: str) -> SerializationData:
    # Don't do anything if there are no properties registered. This could happen if
    # the JSON configuration does not specify any features that require a staging table.
    if len(codespace.properties) == 0:
        return SerializationData()
    codespace.finalize()

    # This dictionary is used to keep an ordered set of all unique code points.
    # Many of the code points within the Unicode code space have overlapping properties so
    # only unique code points need to be serialized.
    unique_codepoints: Dict[CodespaceChar, int] = {}

    # Add the 'null' (e.g. zero property values) code point to the set of unique code points.
    # This code point will be used when attempting to retrieve an invalid code point from the C API.
    assert codespace.default_character is not None
    unique_codepoints[codespace.default_character] = 0

    # Track the largest code point that does not have default values.
    largest_unique_codepoint = 0

    # Now add all the code points to create an ordered set.
    for codepoint, character in codespace.characters.items():
        if character not in unique_codepoints:
            unique_codepoints[character] = len(unique_codepoints)
        # Check if this character has all default values (or not).
        if unique_codepoints[character] != 0:
            largest_unique_codepoint = max(largest_unique_codepoint, codepoint)

    # Build a Two-stage table for storing all code points.
    # This is recommended by Chapter 5.1 of The Unicode Standard.
    stage1: List[int] = []
    stage2: List[int] = []
    stage2_tables: List[List[int]] = []
    properties = sorted(codespace.properties.values(), key=lambda x: x.storage_size, reverse=True)

    for code in range(largest_unique_codepoint+1):
        # Only build stage2 tables on bucket boundaries.
        if (code % BUCKET_SIZE) != 0:
            continue

        # Build a stage2 table for the current range of codepoints.
        # This table may be discarded if it's a duplicate of another table.
        table: List[int] = []

        for code2 in range(code, code + BUCKET_SIZE):
            if code2 in codespace.characters:
                character = codespace.characters[code2] # Grab the codepoint.
                if character in unique_codepoints: # Find its index within the ordered set of code points.
                    table += [unique_codepoints[character]]
                    continue
            # Only a subset of the avaiable Unicode character space is mapped to real characters.
            # The current codepoint happens to not exists so default to the null codepoint.
            table += [0]

        # Check if this table was already generated.
        if table in stage2_tables:
            stage1 += [stage2_tables.index(table) * BUCKET_SIZE]
        else:
            stage1 += [len(stage2)]
            stage2 += table
            stage2_tables += [table]

    size = 0
    source = ""
    source += f'const struct {struct} *{function}(unichar cp)\n'
    source += '{\n'

    # Write unique characters.
    source += f'    static const struct {struct} {function}_unicode_codepoints[] = {{\n'
    for char in unique_codepoints.keys():
        source += "        {"
        for p in properties:
            source += "{0}u,".format(char.property_values[p.id])
            size += p.storage_size.byte_size()
        source += "},\n"
    source += '    };\n\n'

    # Write stage1 table.
    source += f'    static const uint16_t {function}_stage1_table[] = {{'
    for index, value in enumerate(stage1):
        if (index % 8) == 0:
            source += '\n'
            source += '        '
        source += '%du, ' % (value)
        size += 2
    source += '\n'
    source += '    };\n\n'

    # Write stage2 table.
    source += f'    static const uint16_t {function}_stage2_table[] = {{'
    for index, value in enumerate(stage2):
        if (index % 8) == 0:
            source += '\n'
            source += '        '
        source += '%du, ' % (value)
        size += 2
    source += '\n'
    source += '    };\n\n'

    source += f'    const struct {struct} *data = NULL;\n'
    source += '    if (cp > UNICHAR_C({0}))\n'.format(largest_unique_codepoint)
    source += '    {\n'
    source += '        data = &{0}_unicode_codepoints[0]; // code point out of range\n'.format(function)
    source += '    }\n'
    source += '    else\n'
    source += '    {\n'
    source += '        const uint16_t stage2_offset = {0}_stage1_table[cp >> UNICHAR_C({1})];\n'.format(function, BUCKET_SIZE.bit_length() - 1)
    source += '        const uint16_t codepoint_index = {0}_stage2_table[stage2_offset + (cp & UNICHAR_C({1}))];\n'.format(function, BUCKET_SIZE - 1)
    source += '        data = &{0}_unicode_codepoints[codepoint_index];\n'.format(function)
    source += '    }\n'
    source += '\n'
    source += '    return data;\n'
    source += '}\n\n'

    storage_string = {
        StorageSize.UINT8: "uint8_t",
        StorageSize.UINT16: "uint16_t",
        StorageSize.UINT32: "uint32_t",
    }
    header = ""
    header += f"struct {struct} {{\n"
    for p in properties:
        header += f"     {storage_string[p.storage_size]} {p.field_name};\n"
    header += "};\n"
    header += f'const struct {struct} *{function}(unichar cp);\n'

    return SerializationData(source, header, "", size)
