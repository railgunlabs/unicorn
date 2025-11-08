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

from typing import List, Set, Tuple, Type
import zipfile
import json

from .config import Config
from .tools import Codepoint, Codespace
from .feature import Feature, IS_COMPOSABLE, FLAGS_PROPERTY
from .basics import StorageSize, SerializationData
from .decomposition import CanonicalDecompositionFeature

class CanonicalCompositionFeature(Feature):
    @staticmethod
    def dependencies() -> Set[Type[Feature]]:
        return set([CanonicalDecompositionFeature])

    def pre_process(self, codespace: Codespace) -> None:
        codespace.register_property("canonical_composition_mapping_offset", 0, StorageSize.UINT16)
        codespace.register_property("canonical_composition_mapping_count", 0, StorageSize.UINT8)
        codespace.register_property(*FLAGS_PROPERTY)

    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        file = archive.open('composition.json')
        data = json.loads(file.read())
        mappings: List[Tuple[Codepoint,int,int]] = data["compositionMappings"]
        size: int = data["compositionSize"]
        source: str = data["compositionSource"]
        header: str = data["compositionHeader"]
        file.close()

        offset_property = codespace.get("canonical_composition_mapping_offset")
        count_property = codespace.get("canonical_composition_mapping_count")
        flags_property = codespace.get(FLAGS_PROPERTY[0])

        for value_triplet in mappings:
            first = value_triplet[0]
            offset = value_triplet[1]
            count = value_triplet[2]
            codespace.set(first, offset_property, offset)
            codespace.set(first, count_property, count)
            codespace.set_bitwise(first, flags_property, IS_COMPOSABLE)

        public_header = "#define UNICORN_FEATURE_NFC\n"
        return SerializationData(source, header, public_header, size)
