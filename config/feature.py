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

from typing import Set, Tuple, Type
import zipfile

from .basics import StorageSize, SerializationData
from .tools import Codespace
from .config import Config

class Feature(object):
    def __init__(self) -> None:
        self.total_size = 0
    def pre_process(self, codespace: Codespace) -> None:
        pass
    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        return SerializationData()
    def post_process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        return SerializationData()
    @staticmethod
    def dependencies() -> Set[Type['Feature']]:
        return set()

FLAGS_PROPERTY: Tuple[str, int, StorageSize] = ("flags", 0, StorageSize.UINT8)

IS_COMPOSABLE = 0x1
IS_CASED = 0x2
IS_CASE_IGNORABLE = 0x4
IS_NORMALIZATION_NEEDED = 0x8
IS_CHANGES_WHEN_CASEFOLDED = 0x10
