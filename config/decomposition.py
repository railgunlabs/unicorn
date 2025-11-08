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

from .config import Config, OptimizeFor
from .tools import Codepoint, Codespace
from .feature import Feature
from .basics import StorageSize, SerializationData
from .ccc import CanonicalCombiningClass
from .encoding import EncodingUTF8

class CanonicalDecompositionFeature(Feature):
    @staticmethod
    def dependencies() -> Set[Type[Feature]]:
        return set([CanonicalCombiningClass, EncodingUTF8])

    def pre_process(self, codespace: Codespace) -> None:
        codespace.register_property("canonical_decomposition_mapping_offset", 0, StorageSize.UINT16)

    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        decomp_property = codespace.get("canonical_decomposition_mapping_offset")
        if config.optimize == OptimizeFor.SPEED:
            file = archive.open('normalize_for_speed.json')
        else:
            file = archive.open('normalize_for_size.json')
        data = json.loads(file.read())
        mappings: List[Tuple[Codepoint,int]] = data["mappings"]
        source: str = data["source"]
        header: str = data["header"]
        size: int = data["size"]
        file.close()

        # Compute the size (in bytes) occupied by the decomposition mapping data.
        if config.optimize == OptimizeFor.SPEED:
            size *= config.character_storage_bytes()

        # Associate code points with their canonical decomposition mapping.
        for mapping in mappings:
            cp = mapping[0]
            offset = mapping[1]
            codespace.set(cp, decomp_property, offset)

        public_header = "#define UNICORN_FEATURE_NFD\n"
        return SerializationData(source, header, public_header, size)
