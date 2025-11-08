#  Unicorn - Embeddable Unicode Algorithms
#  Copyright (c) 2024-2025 Railgun Labs
#
#  This software is dual-licensed: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License version 3 as
#  published by the Free Software Foundation. For the terms of this
#  license, see <https://www.gnu.org/licenses/>.
#
#  Alternatively, you can license this software under a proprietary
#  license, as set out in <https://railgunlabs.com/unicorn/license/>.

from typing import List, Dict, Set, Type
import zipfile
import json

from .config import Config
from .tools import Codespace, Codepoint, staging_table
from .feature import Feature
from .basics import StorageSize, SerializationData

mapping_to_index: Dict[Codepoint,int] = {}
case_mappings: List[Codepoint] = [0]
casing_codespace = Codespace()

MOST_SIGNIFICANT_BIT_MASKED = 0x8000
MAXIMUM_DIFF = 0x1FFF

class SimpleLowercaseMapping(Feature):
    @staticmethod
    def dependencies() -> Set[Type['Feature']]:
        return set([_SimpleCaseMappings])

    def pre_process(self, _: Codespace) -> None:
        casing_codespace.register_property("simple_lowercase_mapping", MAXIMUM_DIFF, StorageSize.UINT16),

    def process(self, archive: zipfile.ZipFile, _: Codespace, __: Config) -> SerializationData:
        file = archive.open('simple_case_mappings.json')
        data = json.loads(file.read())
        lowercase_mappings: Dict[str,str] = data["lowercase"]
        file.close()

        lowercase_property = casing_codespace.get("simple_lowercase_mapping")

        for cp, value in lowercase_mappings.items():
            codepoint = int(cp,16)
            lowercase_mapping = int(value,16)
            diff = codepoint - lowercase_mapping
            if abs(diff) > MAXIMUM_DIFF:
                if lowercase_mapping not in mapping_to_index:
                    mapping_to_index[lowercase_mapping] = len(case_mappings)
                    case_mappings.append(lowercase_mapping)
                casing_codespace.set(codepoint, lowercase_property, MOST_SIGNIFICANT_BIT_MASKED | mapping_to_index[lowercase_mapping])
            else:
                casing_codespace.set(codepoint, lowercase_property, diff + MAXIMUM_DIFF)

        public_header = "#define UNICORN_FEATURE_SIMPLE_LOWERCASE_MAPPINGS\n"
        return SerializationData(public_header=public_header)

class SimpleUppercaseMapping(Feature):
    @staticmethod
    def dependencies() -> Set[Type['Feature']]:
        return set([_SimpleCaseMappings])

    def pre_process(self, _: Codespace) -> None:
        casing_codespace.register_property("simple_uppercase_mapping", MAXIMUM_DIFF, StorageSize.UINT16),

    def process(self, archive: zipfile.ZipFile, _: Codespace, __: Config) -> SerializationData:
        file = archive.open('simple_case_mappings.json')
        data = json.loads(file.read())
        uppercase_mappings: Dict[str,str] = data["uppercase"]
        file.close()

        uppercase_property = casing_codespace.get("simple_uppercase_mapping")

        for cp, value in uppercase_mappings.items():
            codepoint = int(cp,16)
            uppercase_mapping = int(value,16)
            diff = codepoint - uppercase_mapping
            if abs(diff) > MAXIMUM_DIFF:
                if uppercase_mapping not in mapping_to_index:
                    mapping_to_index[uppercase_mapping] = len(case_mappings)
                    case_mappings.append(uppercase_mapping)
                casing_codespace.set(codepoint, uppercase_property, MOST_SIGNIFICANT_BIT_MASKED | mapping_to_index[uppercase_mapping])
            else:
                casing_codespace.set(codepoint, uppercase_property, diff + MAXIMUM_DIFF)

        public_header = "#define UNICORN_FEATURE_SIMPLE_UPPERCASE_MAPPINGS\n"
        return SerializationData(public_header=public_header)

class SimpleTitlecaseMapping(Feature):
    @staticmethod
    def dependencies() -> Set[Type['Feature']]:
        return set([_SimpleCaseMappings])

    def pre_process(self, _: Codespace) -> None:
        casing_codespace.register_property("simple_titlecase_mapping", MAXIMUM_DIFF, StorageSize.UINT16),

    def process(self, archive: zipfile.ZipFile, _: Codespace, __: Config) -> SerializationData:
        file = archive.open('simple_case_mappings.json')
        data = json.loads(file.read())
        titlecase_mappings: Dict[str,str] = data["titlecase"]
        file.close()

        titlecase_property = casing_codespace.get("simple_titlecase_mapping")

        for cp, value in titlecase_mappings.items():
            codepoint = int(cp,16)
            titlecase_mapping = int(value,16)
            diff = codepoint - titlecase_mapping
            if abs(diff) > MAXIMUM_DIFF:
                if titlecase_mapping not in mapping_to_index:
                    mapping_to_index[titlecase_mapping] = len(case_mappings)
                    case_mappings.append(titlecase_mapping)
                casing_codespace.set(codepoint, titlecase_property, MOST_SIGNIFICANT_BIT_MASKED | mapping_to_index[titlecase_mapping])
            else:
                casing_codespace.set(codepoint, titlecase_property, diff + MAXIMUM_DIFF)

        public_header = "#define UNICORN_FEATURE_SIMPLE_TITLECASE_MAPPINGS\n"
        return SerializationData(public_header=public_header)

class _SimpleCaseMappings(Feature):
    def post_process(self, archive: zipfile.ZipFile, __: Codespace, ___: Config) -> SerializationData:
        size = 0

        source = 'const unichar unicorn_simple_case_mappings[] = {'
        for index, codepoint in enumerate(case_mappings):
            if (index % 4) == 0:
                source += '\n    '
            source += 'UNICHAR_C(0x{:04X}), '.format(codepoint)
            size += 4
        source += '};\n\n'

        header = f"#define CASING_IN_TABLE(C) (((C) & (uint16_t){MOST_SIGNIFICANT_BIT_MASKED}) == (uint16_t){MOST_SIGNIFICANT_BIT_MASKED})\n"
        header += "#define GET_CASED_VALUE(C) ((C) & (uint16_t)0x3FFF)\n"
        header += f"#define CASING_DIFF {MAXIMUM_DIFF}\n"
        header += 'extern const unichar unicorn_simple_case_mappings[{0}];\n'.format(len(case_mappings))

        assert len(case_mappings) < 0xFFFF, "cannot use unsigned 16-bit integer anymore"

        data = staging_table(casing_codespace, "CharCaseData", "unicorn_get_character_casing")
        source += data.source
        header += data.header
        size += data.size

        return SerializationData(source, header, "", size)
