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

from enum import IntEnum
from dataclasses import dataclass

class StorageSize(IntEnum):
    UINT32 = 32
    UINT16 = 16
    UINT8 = 8

    def byte_size(self) -> int:
        return int(self / 8)

@dataclass
class SerializationData:
    def __init__(self, source: str = "", header: str = "", public_header: str = "", size: int =0) -> None:
        self.source = source
        self.header = header
        self.public_header = public_header
        self.size = size
