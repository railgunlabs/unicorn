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

from typing import Dict, Set, Type
import zipfile
import json

from .config import Config
from .tools import Codespace
from .feature import Feature
from .basics import StorageSize, SerializationData

# The bit for each binary property must match the properties index in
# the C enumeration in the public definitions header file.
BINARY_NONCHARACTER_CODE_POINT = 0x1
BINARY_ALPHABETIC = 0x2
BINARY_LOWERCASE = 0x4
BINARY_UPPERCASE = 0x8
BINARY_HEX_DIGIT = 0x10
BINARY_WHITE_SPACE = 0x20
BINARY_MATH = 0x40
BINARY_DASH = 0x80
BINARY_DIACRITIC = 0x100
BINARY_EXTENDER = 0x200
BINARY_IDEOGRAPHIC = 0x400
BINARY_QUOTATION_MARK = 0x800
BINARY_UNIFIED_IDEOGRAPH = 0x1000
BINARY_TERMINAL_PUNCTUATION = 0x2000

class _BinaryProperties(Feature):
    def pre_process(self, codespace: Codespace) -> None:
        codespace.register_property("binary_properties", 0, StorageSize.UINT16)

    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        # Make macro/definition printer a dependency of all binary property classes.
        # This ensures the '#define' is only added once.
        public_header = "#define UNICORN_FEATURE_BINARY_PROPERTIES\n"
        return SerializationData(public_header=public_header)

class BinaryProperties(Feature):
    @staticmethod
    def dependencies() -> Set[Type[Feature]]:
        return set([_BinaryProperties])

    def build(self, archive: zipfile.ZipFile, codespace: Codespace, property_name: str, property_flags: int, macro: str) -> SerializationData:
        file = archive.open('binary_properties.json')
        data = json.loads(file.read())
        mappings: Dict[str,bool] = data[property_name]
        file.close()

        binary_properties = codespace.get("binary_properties")
        for cp in mappings.keys():
            codespace.set_bitwise(int(cp,16), binary_properties, property_flags)

        header = "#define {0}\n".format(macro)
        return SerializationData(public_header=header)

class NoncharacterCodePointProperty(BinaryProperties):
    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        return self.build(archive, codespace, "isNoncharacterCodePoint", BINARY_NONCHARACTER_CODE_POINT, "UNICORN_FEATURE_NONCHARACTER_CODE_POINT")
    
class AlphabeticProperty(BinaryProperties):
    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        return self.build(archive, codespace, "isAlphabetic", BINARY_ALPHABETIC, "UNICORN_FEATURE_ALPHABETIC")
    
class LowercaseProperty(BinaryProperties):
    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        return self.build(archive, codespace, "isLowercase", BINARY_LOWERCASE, "UNICORN_FEATURE_LOWERCASE")
    
class UppercaseProperty(BinaryProperties):
    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        return self.build(archive, codespace, "isUppercase", BINARY_UPPERCASE, "UNICORN_FEATURE_UPPERCASE")
    
class HexDigitProperty(BinaryProperties):
    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        return self.build(archive, codespace, "isHexDigit", BINARY_HEX_DIGIT, "UNICORN_FEATURE_HEX_DIGIT")
    
class WhiteSpaceProperty(BinaryProperties):
    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        return self.build(archive, codespace, "isWhiteSpace", BINARY_WHITE_SPACE, "UNICORN_FEATURE_WHITE_SPACE")
    
class MathProperty(BinaryProperties):
    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        return self.build(archive, codespace, "isMath", BINARY_MATH, "UNICORN_FEATURE_MATH")
    
class DashProperty(BinaryProperties):
    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        return self.build(archive, codespace, "isDash", BINARY_DASH, "UNICORN_FEATURE_DASH")
    
class DiacriticProperty(BinaryProperties):
    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        return self.build(archive, codespace, "isDiacritic", BINARY_DIACRITIC, "UNICORN_FEATURE_DIACRITIC")
    
class ExtenderProperty(BinaryProperties):
    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        return self.build(archive, codespace, "isExtender", BINARY_EXTENDER, "UNICORN_FEATURE_EXTENDER")
    
class IdeographicProperty(BinaryProperties):
    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        return self.build(archive, codespace, "isIdeographic", BINARY_IDEOGRAPHIC, "UNICORN_FEATURE_IDEOGRAPHIC")
    
class QuotationMarkProperty(BinaryProperties):
    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        return self.build(archive, codespace, "isQuotationMark", BINARY_QUOTATION_MARK, "UNICORN_FEATURE_QUOTATION_MARK")
    
class UnifiedIdeographProperty(BinaryProperties):
    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        return self.build(archive, codespace, "isUnifiedIdeograph", BINARY_UNIFIED_IDEOGRAPH, "UNICORN_FEATURE_UNIFIED_IDEOGRAPH")
    
class TerminalPunctuationProperty(BinaryProperties):
    def process(self, archive: zipfile.ZipFile, codespace: Codespace, config: Config) -> SerializationData:
        return self.build(archive, codespace, "isTerminalPunctuation", BINARY_TERMINAL_PUNCTUATION, "UNICORN_FEATURE_TERMINAL_PUNCTUATION")
