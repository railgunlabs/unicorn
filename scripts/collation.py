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
from .feature import Feature
from .basics import StorageSize, SerializationData
from .decomposition import CanonicalDecompositionFeature

class CollationFeature(Feature):
    @staticmethod
    def dependencies() -> Set[Type[Feature]]:
        return set([CanonicalDecompositionFeature])

    def pre_process(self, codespace: Codespace) -> None:
        codespace.register_property("collation_subtype", 0, StorageSize.UINT8)

    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        file = archive.open('collation.json')
        data = json.loads(file.read())
        subtypes: Dict[str,int] = data["subtypes"]
        size: int = data["size"]
        header: str = data["header"]
        source: str = data["source"]
        file.close()

        # Associate collation subtypes with code points.
        subtype_property = codespace.get("collation_subtype")
        for key,subtype in subtypes.items():
            codepoint = int(key, 16)
            codespace.set(codepoint, subtype_property, subtype)

        public_header = "#define UNICORN_FEATURE_COLLATION\n"
        return SerializationData(source, header, public_header, size)
