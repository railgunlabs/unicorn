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

from typing import Dict, Set, Type
import zipfile
import json

from .config import Config
from .tools import Codespace
from .feature import Feature, FLAGS_PROPERTY
from .simple_case_mappings import SimpleLowercaseMapping, SimpleTitlecaseMapping, SimpleUppercaseMapping
from .segmentation import WordBreak
from .ccc import CanonicalCombiningClass
from .basics import SerializationData

class _CaseConversion(Feature):
    def pre_process(self, codespace: Codespace) -> None:
        codespace.register_property(*FLAGS_PROPERTY)

    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        file = archive.open('special_case_mappings.json')
        data = json.loads(file.read())
        source: str = data["source"]
        header: str = data["header"]
        size: int = data["size"]
        mappings: Dict[str,int] = data["characterFlags"]
        file.close()

        flags_property = codespace.get(FLAGS_PROPERTY[0])

        for cp, flags in mappings.items():
            codespace.set_bitwise(int(cp,16), flags_property, flags)

        return SerializationData(source=source, header=header, size=size)

class LowercaseConversion(Feature):
    @staticmethod
    def dependencies() -> Set[Type[Feature]]:
        return set([SimpleLowercaseMapping, _CaseConversion])

    def process(self, _: zipfile.ZipFile, __: Codespace, ___: Config) -> SerializationData:
        public_header = "#define UNICORN_FEATURE_LOWERCASE_CONVERT\n"
        return SerializationData(public_header=public_header)

class UppercaseConversion(Feature):
    @staticmethod
    def dependencies() -> Set[Type[Feature]]:
        return set([SimpleUppercaseMapping, _CaseConversion])

    def process(self, _: zipfile.ZipFile, __: Codespace, ___: Config) -> SerializationData:
        public_header = "#define UNICORN_FEATURE_UPPERCASE_CONVERT\n"
        return SerializationData(public_header=public_header)

class TitlecaseConversion(Feature):
    @staticmethod
    def dependencies() -> Set[Type[Feature]]:
        return set([SimpleTitlecaseMapping, WordBreak, CanonicalCombiningClass, LowercaseConversion])

    def process(self, _: zipfile.ZipFile, __: Codespace, ___: Config) -> SerializationData:
        public_header = "#define UNICORN_FEATURE_TITLECASE_CONVERT\n"
        return SerializationData(public_header=public_header)
