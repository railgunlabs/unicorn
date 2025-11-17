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

from typing import List, Set, Tuple, Type
import sys
import os
import argparse
import zipfile

from .tools import Codespace, staging_table
from .feature import Feature
from .config import parse, Endian, EncodingForm, CharacterProperty, CaseFold, CaseConvert, NormalizationForm, Segmentation, OptimizeFor

from .version import UNICODE_VERSION

from .binary_properties import NoncharacterCodePointProperty
from .binary_properties import AlphabeticProperty
from .binary_properties import LowercaseProperty
from .binary_properties import UppercaseProperty
from .binary_properties import HexDigitProperty
from .binary_properties import WhiteSpaceProperty
from .binary_properties import MathProperty
from .binary_properties import DashProperty
from .binary_properties import DiacriticProperty
from .binary_properties import ExtenderProperty
from .binary_properties import IdeographicProperty
from .binary_properties import QuotationMarkProperty
from .binary_properties import UnifiedIdeographProperty
from .binary_properties import TerminalPunctuationProperty

from .case_conversion import LowercaseConversion
from .case_conversion import UppercaseConversion
from .case_conversion import TitlecaseConversion

from .case_folding import DefaultCaseFolding
from .case_folding import CanonicalCaseFolding

from .encoding import EncodingUTF8
from .encoding import EncodingUTF16
from .encoding import EncodingUTF32

from .ccc import CanonicalCombiningClass

from .collation import CollationFeature

from .compression import ShortStringCompression

from .composition import CanonicalCompositionFeature
from .decomposition import CanonicalDecompositionFeature

from .quickcheck import NFCQuickCheck
from .quickcheck import NFDQuickCheck

from .segmentation import GraphemeBreak
from .segmentation import WordBreak
from .segmentation import SentenceBreak

from .general_category import GeneralCategoryProperty

from .numeric_value import NumericValueProperty

from .simple_case_mappings import SimpleLowercaseMapping
from .simple_case_mappings import SimpleUppercaseMapping
from .simple_case_mappings import SimpleTitlecaseMapping

class GeneratorArgs(argparse.Namespace):
    def __init__(self) -> None:
        self.config_file = "features.json"
        self.output_dir = ""
        self.verbose = False

def remove_blank_lines(s: str) -> str:
    return '\n'.join(line for line in s.splitlines() if line.strip())

# This function generates the header/source Unicode data tables
# with the feature the end-user requests.
def generate(args: GeneratorArgs, archive: zipfile.ZipFile) -> Tuple[str, str, str]:
    header = ""
    source = ""
    public_header = ""
    klasses: Set[Type[Feature]] = set()
    config = parse(args.config_file, archive)

    public_header += "#include <stdint.h>\n"
    if config.endian == Endian.BIG:
        public_header += "#define UNICORN_BIG_ENDIAN\n"
    else:
        public_header += "#define UNICORN_LITTLE_ENDIAN\n"
    public_header += "#define UNICORN_STACK_BUFFER_SIZE {0}\n".format(config.stack_buffer_size)
    public_header += "typedef {0} unichar;\n".format(config.character_storage)

    if config.has_malloc:
        public_header += "#define UNICORN_HAVE_C_MEMORY_ROUTINES\n"

    header += '#include "unicorn.h"\n'
    verson_components = UNICODE_VERSION.split(".")
    header += "#define UNICODE_VERSION_MAJOR {0}\n".format(verson_components[0])
    header += "#define UNICODE_VERSION_MINOR {0}\n".format(verson_components[1])
    header += "#define UNICODE_VERSION_PATCH {0}\n".format(verson_components[2])
 
    if config.optimize == OptimizeFor.SPEED:
        header += "#define UNICORN_OPTIMIZE_FOR_SPEED // cppcheck-suppress misra-c2012-2.5\n"
    else:
        header += "#define UNICORN_OPTIMIZE_FOR_SIZE\n"

    # Add case conversion.
    if config.algorithms.case_convert & CaseConvert.LOWER:
        klasses.add(LowercaseConversion)
    if config.algorithms.case_convert & CaseConvert.UPPER:
        klasses.add(UppercaseConversion)
    if config.algorithms.case_convert & CaseConvert.TITLE:
        klasses.add(TitlecaseConversion)

    # Add binary character properties.
    if config.character_properties & CharacterProperty.NONCHARACTER_CODE_POINT:
        klasses.add(NoncharacterCodePointProperty)
    if config.character_properties & CharacterProperty.ALPHABETIC:
        klasses.add(AlphabeticProperty)
    if config.character_properties & CharacterProperty.LOWERCASE:
        klasses.add(LowercaseProperty)
    if config.character_properties & CharacterProperty.UPPERCASE:
        klasses.add(UppercaseProperty)
    if config.character_properties & CharacterProperty.HEX_DIGIT:
        klasses.add(HexDigitProperty)
    if config.character_properties & CharacterProperty.WHITE_SPACE:
        klasses.add(WhiteSpaceProperty)
    if config.character_properties & CharacterProperty.MATH:
        klasses.add(MathProperty)
    if config.character_properties & CharacterProperty.DASH:
        klasses.add(DashProperty)
    if config.character_properties & CharacterProperty.DIACRITIC:
        klasses.add(DiacriticProperty)
    if config.character_properties & CharacterProperty.EXTENDER:
        klasses.add(ExtenderProperty)
    if config.character_properties & CharacterProperty.IDEOGRAPHIC:
        klasses.add(IdeographicProperty)
    if config.character_properties & CharacterProperty.QUOTATION_MARK:
        klasses.add(QuotationMarkProperty)
    if config.character_properties & CharacterProperty.UNIFIED_IDEOGRAPH:
        klasses.add(UnifiedIdeographProperty)
    if config.character_properties & CharacterProperty.TERMINAL_PUNCTUATION:
        klasses.add(TerminalPunctuationProperty)

    # Add short string compression.
    if config.algorithms.compression:
        klasses.add(ShortStringCompression)

    # Add segmentation.
    if config.algorithms.segmentation & Segmentation.GRAPHEME:
        klasses.add(GraphemeBreak)
    if config.algorithms.segmentation & Segmentation.WORD:
        klasses.add(WordBreak)
    if config.algorithms.segmentation & Segmentation.SENTENCE:
        klasses.add(SentenceBreak)

    # Add normalization.
    if config.algorithms.normalization & NormalizationForm.NFC:
        klasses.add(CanonicalCompositionFeature)
    if config.algorithms.normalization & NormalizationForm.NFD:
        klasses.add(CanonicalDecompositionFeature)

    # Add normalization quick check.
    if config.algorithms.normalization_quick_check:
        if config.algorithms.normalization & NormalizationForm.NFC:
            klasses.add(NFCQuickCheck)
        if config.algorithms.normalization & NormalizationForm.NFD:
            klasses.add(NFDQuickCheck)

    # Add collation.
    if config.algorithms.collation:
        klasses.add(CollationFeature)

    # Add case folding.
    if config.algorithms.case_fold & CaseFold.DEFAULT:
        klasses.add(DefaultCaseFolding)
    if config.algorithms.case_fold & CaseFold.CANONICAL:
        klasses.add(CanonicalCaseFolding)

    # Add the General_Category character property.
    if config.character_properties & CharacterProperty.GENERAL_CATEGORY:
        klasses.add(GeneralCategoryProperty)

    # Add the Canonical_Combining_Class character property.
    if config.character_properties & CharacterProperty.CANONICAL_COMBINING_CLASS:
        klasses.add(CanonicalCombiningClass)

    # Add the Numeric_Value character property.
    if config.character_properties & CharacterProperty.NUMERIC_VALUE:
        klasses.add(NumericValueProperty)

    # Add case mappings.
    if config.character_properties & CharacterProperty.SIMPLE_LOWERCASE_MAPPING:
        klasses.add(SimpleLowercaseMapping)
    if config.character_properties & CharacterProperty.SIMPLE_UPPERCASE_MAPPING:
        klasses.add(SimpleUppercaseMapping)
    if config.character_properties & CharacterProperty.SIMPLE_TITLECASE_MAPPING:
        klasses.add(SimpleTitlecaseMapping)

    # Add encoding forms.
    if config.encoding_forms & EncodingForm.UTF8:
        klasses.add(EncodingUTF8)
    if config.encoding_forms & EncodingForm.UTF16:
        klasses.add(EncodingUTF16)
    if config.encoding_forms & EncodingForm.UTF32:
        klasses.add(EncodingUTF32)

    # Gather up all dependencies.
    while True:
        all_klasses = klasses.copy()
        for klass in klasses:
            all_klasses = all_klasses.union(klass.dependencies())
        if klasses == all_klasses:
            break
        klasses = all_klasses

    # Sort classes alphabetically, otherwise they will sort by their address
    # which causes the data to generate in a different order every time.
    sorted_klasses = sorted(list(klasses), key=lambda x: x.__class__.__name__)

    codespace = Codespace()
    size = 0

    # Create instances and perform pre-processing.
    instances: List[Feature] = []
    for klass in sorted_klasses:
        instance = klass()
        instance.pre_process(codespace)
        instances.append(instance)

    # Perform processing.
    for instance in instances:
        data = instance.process(archive, codespace, config)
        source += data.source + "\n"
        header += data.header + "\n"
        public_header += data.public_header
        size += data.size
        instance.total_size += data.size

    # Perform post-processing.
    for instance in instances:
        data = instance.post_process(archive, codespace, config)
        source += data.source + "\n"
        header += data.header + "\n"
        public_header += data.public_header
        size += data.size
        instance.total_size += data.size

    data = staging_table(codespace, "CodepointData", "unicorn_get_codepoint_data")
    source += data.source
    header += data.header
    size += data.size

    # Report total size.
    if args.verbose:
        for instance in instances:
            if instance.total_size > 0:
                print("Added: {} ({:.2f} kB)".format(instance.__class__.__name__, instance.total_size / 1024))
            else:
                print("Added: {}".format(instance.__class__.__name__))
        print("Character table size: ({:.2f} kB)".format(data.size / 1024))
        print("Total size: {:.2f} kB".format(size / 1024))

    # Remove all blank lines.
    source = remove_blank_lines(source)
    header = remove_blank_lines(header)
    public_header = remove_blank_lines(public_header)

    # Source files should have a new line at the end.
    source += "\n"
    header += "\n"
    public_header += "\n"

    return (header, source, public_header)

def compile(args: GeneratorArgs) -> int:
    if not os.path.exists("unicode.bin"):
        print("error: cannot find unicode.bin")
        return 1

    archive = zipfile.ZipFile("unicode.bin", "r")
    unidata_header, unidata_source, unidata_public_header = generate(args, archive)

    copyright = """/*
 *  Unicorn - Embeddable Unicode Algorithms
 *  Copyright (c) 2024-2025 Railgun Labs
 *
 *  This software is dual-licensed: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation. For the terms of this
 *  license, see <https://www.gnu.org/licenses/>.
 *
 *  Alternatively, you can license this software under a proprietary
 *  license, as set out in <https://railgunlabs.com/unicorn/license/>.
 */

"""

    filename = os.path.join(args.output_dir, "unidata.c")
    fp = open(filename, "w", encoding="utf-8", newline="\n")
    fp.write(copyright)
    # Cppcheck takes an excruciatingly long time to process the Unicode data tables.
    # The following preprocessor check prevents Cppecheck from processing the tables
    # which drastically speeds up its work.
    fp.write("#if !defined(__cppcheck__)\n")
    fp.write('#include "unidata.h"\n')
    fp.write('#include "common.h"\n')
    fp.write(unidata_source)
    fp.write("#endif\n")
    fp.close()
    print("writing:", os.path.realpath(filename))

    filename = os.path.join(args.output_dir, "unidata.h")
    fp = open(filename, "w", encoding="utf-8", newline="\n")
    fp.write(copyright)
    fp.write('#ifndef UNICORN_DATA_H\n')
    fp.write('#define UNICORN_DATA_H\n')
    fp.write(unidata_header)
    fp.write('#endif\n')
    fp.close()
    print("writing:", os.path.realpath(filename))

    filename = os.path.join(args.output_dir, "_unicorn.h")
    fp = open(filename, "w", encoding="utf-8", newline="\n")
    fp.write(copyright)
    fp.write(unidata_public_header)
    fp.close()
    print("writing:", os.path.realpath(filename))
    return 0

if __name__ == "__main__":
    # This script relies on Python 3.6's ordred dictionary feature.
    # Prior versions of python did not order dictionary entries.
    if isinstance(sys.version_info, tuple):
        py_major = sys.version_info[0]
        py_minor = sys.version_info[1]
    else:
        py_major = sys.version_info.major
        py_minor = sys.version_info.minor
        
    if py_major < 3 or py_minor < 10:
        raise Exception("This script requires Python 3.10 or newer")

    parser = argparse.ArgumentParser(description="Build Unicorn source and header.")
    parser.add_argument("--config", dest="config_file", action="store", help="path to configuration file")
    parser.add_argument("--output", dest="output_dir", action="store", help="path to write generated C source files")
    parser.add_argument("--verbose", dest="verbose", action="store_true", help="report added features and their size contributions")

    args = GeneratorArgs()
    parser.parse_args(namespace=args)
    sys.exit(compile(args))
