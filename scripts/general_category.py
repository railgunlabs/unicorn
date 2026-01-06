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

class GeneralCategoryProperty(Feature):
    def pre_process(self, codespace: Codespace) -> None:
        codespace.register_property("general_category", 29, StorageSize.UINT8)

    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        file = archive.open('gc.json')
        data = json.loads(file.read())
        characters: Dict[str,int] = data["characters"]
        file.close()

        gc_property = codespace.get("general_category")
        for cp, value in characters.items():
            codespace.set(int(cp,16), gc_property, value)

        public_header = "#define UNICORN_FEATURE_GC\n"
        return SerializationData(public_header=public_header)
