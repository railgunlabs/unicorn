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

import zipfile

from .config import Config
from .tools import Codespace
from .feature import Feature
from .basics import SerializationData

class ShortStringCompression(Feature):
    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        public_header = "#define UNICORN_FEATURE_COMPRESSION\n"
        return SerializationData(public_header=public_header)
