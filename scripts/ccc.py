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

from typing import Dict
import zipfile
import json

from .config import Config
from .tools import Codespace
from .feature import Feature
from .basics import StorageSize, SerializationData

class CanonicalCombiningClass(Feature):
    def pre_process(self, codespace: Codespace) -> None:
        codespace.register_property("canonical_combining_class", 0, StorageSize.UINT8)

    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        file = archive.open('ccc.json')
        mappings: Dict[str,int] = json.loads(file.read())["ccc"]
        file.close()

        ccc_property = codespace.get("canonical_combining_class")
        for cp, ccc in mappings.items():
            codespace.set(int(cp,16), ccc_property, ccc)

        public_header = "#define UNICORN_FEATURE_CCC\n"
        return SerializationData(public_header=public_header)
