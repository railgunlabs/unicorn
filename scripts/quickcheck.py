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
from .ccc import CanonicalCombiningClass

class NFCQuickCheck(Feature):
    @staticmethod
    def dependencies() -> Set[Type[Feature]]:
        return set([CanonicalCombiningClass])

    def pre_process(self, codespace: Codespace) -> None:
        codespace.register_property("quick_check_flags", 0, StorageSize.UINT8)

    def process(self, archive: zipfile.ZipFile, codespace: Codespace, _: Config) -> SerializationData:
        file = archive.open('quickcheck.json')
        data = json.loads(file.read())
        nfc_quick_check: Dict[str,int] = data["quickCheckNFC"]
        file.close()

        qc_flags_property = codespace.get("quick_check_flags")
        for cp, qc_value in nfc_quick_check.items():
            codespace.set_bitwise(int(cp,16), qc_flags_property, qc_value << 4)

        public_header = "#define UNICORN_FEATURE_NFC_QUICK_CHECK\n"
        return SerializationData(public_header=public_header)

class NFDQuickCheck(Feature):
    @staticmethod
    def dependencies() -> Set[Type[Feature]]:
        return set([CanonicalCombiningClass])

    def pre_process(self, codespace: Codespace) -> None:
        codespace.register_property("quick_check_flags", 0, StorageSize.UINT8)

    def process(self, archive: zipfile.ZipFile, codespace: Codespace, _: Config) -> SerializationData:
        file = archive.open('quickcheck.json')
        data = json.loads(file.read())
        nfd_quick_check: Dict[str,int] = data["quickCheckNFD"]
        file.close()

        qc_flags_property = codespace.get("quick_check_flags")
        for cp, qc_value in nfd_quick_check.items():
            codespace.set_bitwise(int(cp,16), qc_flags_property, qc_value)

        public_header = "#define UNICORN_FEATURE_NFD_QUICK_CHECK\n"
        return SerializationData(public_header=public_header)
