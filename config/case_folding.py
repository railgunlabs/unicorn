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

from typing import List, Set, Type, Tuple
import zipfile
import json

from .config import Config
from .tools import Codespace, Codepoint
from .feature import Feature, FLAGS_PROPERTY, IS_NORMALIZATION_NEEDED, IS_CHANGES_WHEN_CASEFOLDED
from .basics import StorageSize, SerializationData
from .decomposition import CanonicalDecompositionFeature

class DefaultCaseFolding(Feature):
    def pre_process(self, codespace: Codespace) -> None:
        codespace.register_property("full_casefold_mapping_offset", 0, StorageSize.UINT16)
        codespace.register_property(*FLAGS_PROPERTY)

    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        file = archive.open('casefold.json')
        data = json.loads(file.read())
        source: str = data["source"]
        header: str = data["header"]
        size: int = data["size"]
        has_greek: List[Codepoint] = data["hasGreek"]
        case_foldings: List[Tuple[Codepoint,int]] = data["caseFoldings"]
        changes_when_casefolded: List[Codepoint] = data["changesWhenCasefolded"]
        file.close()

        flags_property = codespace.get(FLAGS_PROPERTY[0])
        casefold_property = codespace.get("full_casefold_mapping_offset")

        for cp in has_greek:
            codespace.set_bitwise(cp, flags_property, IS_NORMALIZATION_NEEDED)

        for cp in changes_when_casefolded:
            codespace.set_bitwise(cp, flags_property, IS_CHANGES_WHEN_CASEFOLDED)

        for cp,offset in case_foldings:
            codespace.set(cp, casefold_property, offset)

        header += "#define UNICORN_CHAR_NEEDS_NORMALIZATION {0}u\n".format(IS_NORMALIZATION_NEEDED)
        header += "#define UNICORN_CHAR_CHANGES_WHEN_CASEFOLDED {0}u\n".format(IS_CHANGES_WHEN_CASEFOLDED)

        public_header = "#define UNICORN_FEATURE_CASEFOLD_DEFAULT\n"
        return SerializationData(source, header, public_header, size)

class CanonicalCaseFolding(Feature):
    @staticmethod
    def dependencies() -> Set[Type[Feature]]:
        return set([DefaultCaseFolding, CanonicalDecompositionFeature])

    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        public_header = "#define UNICORN_FEATURE_CASEFOLD_CANONICAL\n"
        return SerializationData(public_header=public_header)
