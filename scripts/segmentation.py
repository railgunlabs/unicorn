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

from typing import Set, Type
import zipfile
import json

from .config import Config
from .tools import Codespace
from .feature import Feature
from .basics import StorageSize, SerializationData

max_states = 0

class _Segmentation(Feature):
    def post_process(self, archive: zipfile.ZipFile, __: Codespace, ___: Config) -> SerializationData:
        gcb = archive.open('segmentation.json')
        data = json.loads(gcb.read())
        header = data["header"]
        gcb.close()

        header += "#define MAX_BREAK_STATES {0}\n".format(max_states)

        public_header = "#define UNICORN_FEATURE_SEGMENTATION\n"
        return SerializationData(header=header, public_header=public_header)

class GraphemeBreak(Feature):
    @staticmethod
    def dependencies() -> Set[Type[Feature]]:
        return set([_Segmentation])

    def pre_process(self, codespace: Codespace) -> None:
        codespace.register_property("gcb", 0, StorageSize.UINT8)
        codespace.register_property("incb", 0, StorageSize.UINT8)

    def process(self, archive: zipfile.ZipFile, codespace: Codespace, _: Config) -> SerializationData:
        gcb = archive.open('gcb.json')
        data = json.loads(gcb.read())
        header = data["header"]
        source = data["source"]
        size = data["size"]
        gcb.close()

        global max_states
        max_states = max(int(data["states"]), max_states)

        gcb_property = codespace.get("gcb")
        for d in data["gcb"]:
            codespace.set(d[0], gcb_property, d[1])

        incb_property = codespace.get("incb")
        for d in data["incb"]:
            codespace.set(d[0], incb_property, d[1])

        public_header = "#define UNICORN_FEATURE_GCB\n"
        return SerializationData(source, header, public_header, size)

class WordBreak(Feature):
    @staticmethod
    def dependencies() -> Set[Type[Feature]]:
        return set([_Segmentation])

    def pre_process(self, codespace: Codespace) -> None:
        codespace.register_property("wb", 0, StorageSize.UINT8)
        codespace.register_property("wbx", 0, StorageSize.UINT8)

    def process(self, archive: zipfile.ZipFile, codespace: Codespace, _: Config) -> SerializationData:
        wb = archive.open('wb.json')
        data = json.loads(wb.read())
        header = data["header"]
        source = data["source"]
        size = data["size"]
        wb.close()

        global max_states
        max_states = max(int(data["states"]), max_states)

        wb_property = codespace.get("wb")
        for d in data["wb"]:
            codespace.set(d[0], wb_property, d[1])

        wbx_property = codespace.get("wbx")
        for d in data["wbx"]:
            codespace.set(d[0], wbx_property, d[1])

        public_header = "#define UNICORN_FEATURE_WB\n"
        return SerializationData(source, header, public_header, size)

class SentenceBreak(Feature):
    @staticmethod
    def dependencies() -> Set[Type[Feature]]:
        return set([_Segmentation])

    def pre_process(self, codespace: Codespace) -> None:
        codespace.register_property("sb", 0, StorageSize.UINT8)

    def process(self, archive: zipfile.ZipFile, codespace: Codespace, _: Config) -> SerializationData:
        wb = archive.open('sb.json')
        data = json.loads(wb.read())
        header = data["header"]
        source = data["source"]
        size = data["size"]
        wb.close()

        global max_states
        max_states = max(int(data["states"]), max_states)

        wb_property = codespace.get("sb")
        for d in data["sb"]:
            codespace.set(d[0], wb_property, d[1])

        public_header = "#define UNICORN_FEATURE_SB\n"
        return SerializationData(source, header, public_header, size)
