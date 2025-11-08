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

from typing import List, Set, Dict, Any, Optional, Tuple, cast
import sys
import enum
import json
import zipfile
import fnmatch

from .tools import Codepoint, Block

class Endian(enum.Enum):
    LITTLE = enum.auto()
    BIG = enum.auto()

class OptimizeFor(enum.Enum):
    SPEED = enum.auto()
    SIZE = enum.auto()

class NormalizationForm(enum.IntFlag):
    NONE = enum.auto()
    NFC = enum.auto()
    NFD = enum.auto()

class CaseConvert(enum.IntFlag):
    NONE = enum.auto()
    LOWER = enum.auto()
    UPPER = enum.auto()
    TITLE = enum.auto()

class CaseFold(enum.IntFlag):
    NONE = enum.auto()
    DEFAULT = enum.auto()
    CANONICAL = enum.auto()

class EncodingForm(enum.IntFlag):
    NONE = enum.auto()
    UTF8 = enum.auto()
    UTF16 = enum.auto()
    UTF32 = enum.auto()

class Segmentation(enum.IntFlag):
    NONE = enum.auto()
    GRAPHEME = enum.auto()
    WORD = enum.auto()
    SENTENCE = enum.auto()

class CharacterProperty(enum.IntFlag):
    NONE = enum.auto()
    ALPHABETIC = enum.auto()
    CANONICAL_COMBINING_CLASS = enum.auto()
    DASH = enum.auto()
    DIACRITIC = enum.auto()
    EXTENDER = enum.auto()
    GENERAL_CATEGORY = enum.auto()
    HEX_DIGIT = enum.auto()
    IDEOGRAPHIC = enum.auto()
    LOWERCASE = enum.auto()
    MATH = enum.auto()
    NONCHARACTER_CODE_POINT = enum.auto()
    NUMERIC_VALUE = enum.auto()
    QUOTATION_MARK = enum.auto()
    SIMPLE_LOWERCASE_MAPPING = enum.auto()
    SIMPLE_UPPERCASE_MAPPING = enum.auto()
    SIMPLE_TITLECASE_MAPPING = enum.auto()
    TERMINAL_PUNCTUATION = enum.auto()
    UNIFIED_IDEOGRAPH = enum.auto()
    UPPERCASE = enum.auto()
    WHITE_SPACE = enum.auto()

class Algorithms:
    def __init__(self) -> None:
        self.normalization = NormalizationForm.NONE
        self.case_convert = CaseConvert.NONE
        self.case_fold = CaseFold.NONE
        self.segmentation = Segmentation.NONE
        self.compression = False
        self.collation = False
        self.normalization_quick_check = False

class Config:
    def __init__(self) -> None:
        self.endian = Endian.BIG if sys.byteorder == "big" else Endian.LITTLE
        self.has_malloc = True
        self.character_storage = "uint32_t"
        self.optimize = OptimizeFor.SPEED
        self.stack_buffer_size = 32
        self.encoding_forms = EncodingForm.NONE
        self.algorithms = Algorithms()
        self.character_properties = CharacterProperty.NONE
    
    def character_storage_bytes(self) -> int:
        if self.character_storage.find("64") > 0:
            return 8  # e.g. "int64_t"
        return 4      # assume a 32-bit integer, e.g. "uint32_t"

def versiontuple(v: str) -> Tuple[int, int]:
    digits: List[int] = []
    for point in v.split("."):
        digits.append(int(point))
    while len(digits) < 2:
        digits.append(0)
    return (digits[0], digits[1])

# Perform loose matching between two character property names.
# See https://unicode.org/reports/tr44/#Matching_Symbolic
def is_loose_match(p1: str, p2: str) -> bool:
    # The following logic lowercases the string, removes any leading 'is', and
    # removes all underscores and whitespace. Example:
    #
    #   "Is_White_Space" -> "whitespace"
    #   "General Category" -> "generalcategory"
    #
    p1.lower().lstrip("is").replace("_", "").replace(" ", "")
    p2.lower().rstrip("is").replace("_", "").replace(" ", "")
    return p1 == p2

def parse_algorithms(config: Config, algorithms: Any) -> None:
    if not isinstance(algorithms, Dict):
        print("error: expected object for 'algorithms'")
        sys.exit(1)

    for key, value in algorithms.items():
        if key == "normalization":
            if not isinstance(value, List):
                print("error: expected string list for 'normalization'")
                sys.exit(1)
            for normform in value:
                if not isinstance(normform, str):
                    print("error: expected string list for 'normalization'")
                    sys.exit(1)
                if normform.lower() == "nfc":
                    config.algorithms.normalization |= NormalizationForm.NFC
                elif normform.lower() == "nfd":
                    config.algorithms.normalization |= NormalizationForm.NFD
                else:
                    print("warning: ignoring unknown normalization form: {0}".format(normform))
        elif key == "normalizationQuickCheck":
            if not isinstance(value, bool):
                print("error: expected boolean value for 'normalizationQuickCheck'")
                sys.exit(1)
            config.algorithms.normalization_quick_check = value
        elif key == "caseConversion":
            if not isinstance(value, List):
                print("error: expected string list for 'caseConversion'")
                sys.exit(1)
            for casing in value:
                if not isinstance(casing, str):
                    print("error: expected string list for 'caseConversion'")
                    sys.exit(1)
                if casing.lower() == "lower":
                    config.algorithms.case_convert |= CaseConvert.LOWER
                elif casing.lower() == "upper":
                    config.algorithms.case_convert |= CaseConvert.UPPER
                elif casing.lower() == "title":
                    config.algorithms.case_convert |= CaseConvert.TITLE
                else:
                    print("warning: ignoring unknown case conversion target: {0}".format(casing))
        elif key == "caseFolding":
            if not isinstance(value, List):
                print("error: expected string list for 'caseFolding'")
                sys.exit(1)
            for casing in value:
                if not isinstance(casing, str):
                    print("error: expected string list for 'caseFolding'")
                    sys.exit(1)
                if casing.lower() == "default":
                    config.algorithms.case_fold |= CaseFold.DEFAULT
                elif casing.lower() == "canonical":
                    config.algorithms.case_fold |= CaseFold.CANONICAL
                else:
                    print("warning: ignoring unknown case fold target: {0}".format(casing))
        elif key == "segmentation":
            if not isinstance(value, List):
                print("error: expected string list for 'segmentation'")
                sys.exit(1)
            for normform in value:
                if not isinstance(normform, str):
                    print("error: expected string list for 'segmentation'")
                    sys.exit(1)
                if normform.lower() == "grapheme":
                    config.algorithms.segmentation |= Segmentation.GRAPHEME
                elif normform.lower() == "word":
                    config.algorithms.segmentation |= Segmentation.WORD
                elif normform.lower() == "sentence":
                    config.algorithms.segmentation |= Segmentation.SENTENCE
                else:
                    print("warning: ignoring unknown segmentation form: {0}".format(normform))
        elif key == "compression":
            if not isinstance(value, bool):
                print("error: expected boolean value for 'compression'")
                sys.exit(1)
            config.algorithms.compression = value
        elif key == "collation":
            if not isinstance(value, bool):
                print("error: expected boolean value for 'collation'")
                sys.exit(1)
            config.algorithms.collation = value
        else:
            print("warning: ignoring unknown algorithm: {0}".format(key))

def parse(filename: Optional[str], archive: zipfile.ZipFile) -> Config:
    config = Config() # initialize with default values
    if filename is None:
        return config
    
    # Read the contents of the configuration file.
    try:
        f = open(filename, "r", encoding="utf-8")
    except FileNotFoundError:
        print("error: file not found: {0}".format(filename))
        sys.exit(1)
    data: Dict[str,Any] = json.load(f)
    f.close()

    # Extract the contents of the configuration file.
    if "version" not in data:
        print("error: missing version: {0}".format(filename))
        sys.exit(1)
    version = versiontuple(cast(str, data["version"]))
    if version[0] < 1:
        print("error: illegal version: {0}".format(version))
        sys.exit(1)

    # Extract fields.
    for key, value in data.items():
        if key == "version":
            continue
        elif key == "endian":
            if not isinstance(value, str):
                print("error: expected string value for 'endian'")
                sys.exit(1)
            if value.lower() == "little":
                config.endian = Endian.LITTLE
            elif value.lower() == "big":
                config.endian = Endian.BIG
            elif value.lower() == "native":
                pass # Nothing to do here as the native byte order is already the default value.
            else:
                print("warning: expected 'little' or 'big' for 'endian'")
        elif key == "optimizeFor":
            if not isinstance(value, str):
                print("error: expected string value for 'optimizeFor'")
                sys.exit(1)
            if value.lower() == "speed":
                config.optimize = OptimizeFor.SPEED
            elif value.lower() == "size":
                config.optimize = OptimizeFor.SIZE
            else:
                print("warning: expected 'speed' or 'size' for 'optimizeFor'")
        elif key == "hasStandardAllocators":
            if not isinstance(value, bool):
                print("error: expected boolean value for 'hasStandardAllocators'")
                sys.exit(1)
            config.has_malloc = value
        elif key == "characterStorage":
            if not isinstance(value, str) or len(value.strip()) == 0:
                print("error: expected string value for 'characterStorage'")
                sys.exit(1)
            config.character_storage = value
        elif key == "stackBufferSize":
            if not isinstance(value, int) or value < 1:
                print("error: expected a positive integer value for 'stackBufferSize'")
                sys.exit(1)
            MINIMUM_STACK_BUFFER_SIZE = 4
            if value < MINIMUM_STACK_BUFFER_SIZE:
                print(f"warning: rounding up 'stackBufferSize' from {value} to {MINIMUM_STACK_BUFFER_SIZE} (the minimum)")
                value = MINIMUM_STACK_BUFFER_SIZE
            config.stack_buffer_size = value
        elif key == "excludeCharacterBlocks":
            if not isinstance(value, List):
                print("error: expected string list for 'excludeCharacterBlocks'")
                sys.exit(1)
            print("warning: the 'excludeCharacterBlocks' configuration feature is deprecated")
        elif key == "encodingForms":
            if not isinstance(value, List):
                print("error: expected string list for 'encodingForms'")
                sys.exit(1)
            for encoding in value:
                if not isinstance(encoding, str):
                    print("error: expected string list for 'encodingForms'")
                    sys.exit(1)
                if encoding.lower() == "utf-8":
                    config.encoding_forms |= EncodingForm.UTF8
                elif encoding.lower() == "utf-16":
                    config.encoding_forms |= EncodingForm.UTF16
                elif encoding.lower() == "utf-32":
                    config.encoding_forms |= EncodingForm.UTF32
                else:
                    print("warning: expected 'utf-8' or 'utf-16' or 'utf-32' for 'encodingForms'")
        elif key == "characterProperties":
            if not isinstance(value, List):
                print("error: expected string list for 'characterProperties'")
                sys.exit(1)
            for charprop in value:
                if not isinstance(charprop, str):
                    print("error: expected string list for 'characterProperties'")
                    sys.exit(1)
                if is_loose_match(charprop, "Alphabetic"):
                    config.character_properties |= CharacterProperty.ALPHABETIC
                elif is_loose_match(charprop, "Canonical_Combining_Class"):
                    config.character_properties |= CharacterProperty.CANONICAL_COMBINING_CLASS
                elif is_loose_match(charprop, "Dash"):
                    config.character_properties |= CharacterProperty.DASH
                elif is_loose_match(charprop, "Diacritic"):
                    config.character_properties |= CharacterProperty.DIACRITIC
                elif is_loose_match(charprop, "Extender"):
                    config.character_properties |= CharacterProperty.EXTENDER
                elif is_loose_match(charprop, "General_Category"):
                    config.character_properties |= CharacterProperty.GENERAL_CATEGORY
                elif is_loose_match(charprop, "Hex_Digit"):
                    config.character_properties |= CharacterProperty.HEX_DIGIT
                elif is_loose_match(charprop, "Ideographic"):
                    config.character_properties |= CharacterProperty.IDEOGRAPHIC
                elif is_loose_match(charprop, "Lowercase"):
                    config.character_properties |= CharacterProperty.LOWERCASE
                elif is_loose_match(charprop, "Math"):
                    config.character_properties |= CharacterProperty.MATH
                elif is_loose_match(charprop, "Noncharacter_Code_Point"):
                    config.character_properties |= CharacterProperty.NONCHARACTER_CODE_POINT
                elif is_loose_match(charprop, "Numeric_Value"):
                    config.character_properties |= CharacterProperty.NUMERIC_VALUE
                elif is_loose_match(charprop, "Quotation_Mark"):
                    config.character_properties |= CharacterProperty.QUOTATION_MARK
                elif is_loose_match(charprop, "Simple_Lowercase_Mapping"):
                    config.character_properties |= CharacterProperty.SIMPLE_LOWERCASE_MAPPING
                elif is_loose_match(charprop, "Simple_Uppercase_Mapping"):
                    config.character_properties |= CharacterProperty.SIMPLE_UPPERCASE_MAPPING
                elif is_loose_match(charprop, "Simple_Titlecase_Mapping"):
                    config.character_properties |= CharacterProperty.SIMPLE_TITLECASE_MAPPING
                elif is_loose_match(charprop, "Terminal_Punctuation"):
                    config.character_properties |= CharacterProperty.TERMINAL_PUNCTUATION
                elif is_loose_match(charprop, "Unified_Ideograph"):
                    config.character_properties |= CharacterProperty.UNIFIED_IDEOGRAPH
                elif is_loose_match(charprop, "Uppercase"):
                    config.character_properties |= CharacterProperty.UPPERCASE
                elif is_loose_match(charprop, "White_Space"):
                    config.character_properties |= CharacterProperty.WHITE_SPACE
                else:
                    print("warning: ignoring unknown character property: {0}".format(charprop))
        elif key == "algorithms":
            parse_algorithms(config, value)
        else:
            print("warning: ignoring unknown feature: {0}".format(key))
    return config
