/*
 *  Unicorn - Embeddable Unicode Algorithms
 *  Copyright (c) 2024-2026 Railgun Labs
 *
 *  This software is dual-licensed: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation. For the terms of this
 *  license, see <https://www.gnu.org/licenses/>.
 *
 *  Alternatively, you can license this software under a proprietary
 *  license, as set out in <https://railgunlabs.com/unicorn/license/>.
 */

#include "common.h"
#include "unidata.h"

UNICORN_API unigc uni_gc(unichar c) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
#if defined(UNICORN_FEATURE_GC)
    return (unigc)unicorn_get_codepoint_data(c)->general_category; // cppcheck-suppress premium-misra-c-2012-10.5 ; Intentional violation to unpack data from 32-bits to an enum.
#else
    (void)c;
    return UNI_UNASSIGNED;
#endif
}

UNICORN_API uint8_t uni_ccc(unichar c) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
#if defined(UNICORN_FEATURE_CCC)
    return unicorn_get_codepoint_data(c)->canonical_combining_class;
#else
    (void)c;
    return 0;
#endif
}

UNICORN_API bool uni_is(unichar c, unibp p) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
#if defined(UNICORN_FEATURE_BINARY_PROPERTIES)
    const uint32_t binary_props = (uint32_t)unicorn_get_codepoint_data(c)->binary_properties;
    const uint32_t prop = (uint32_t)p;
    const uint32_t bit_mask = (uint32_t)1 << prop;
    return ((binary_props & bit_mask) == bit_mask) ? true : false;
#else
    (void)c;
    (void)p;
    return false;
#endif
}

UNICORN_API const char *uni_numval(unichar c) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
#if defined(UNICORN_FEATURE_NUMERIC_VALUE)
    const char *numval = NULL;
    const uint8_t index = unicorn_get_codepoint_data(c)->numeric_value_offset;
    if (index > (uint8_t)0)
    {
        assert((size_t)index < COUNT_OF(unicorn_numeric_values)); // LCOV_EXCL_BR_LINE
        numval = unicorn_numeric_values[index];
    }
    return numval;
#else
    (void)c;
    return NULL;
#endif
}

UNICORN_API unichar uni_tolower(unichar c)
{
#if defined(UNICORN_FEATURE_SIMPLE_LOWERCASE_MAPPINGS)
    unichar lower;
    const uint16_t mapping = unicorn_get_character_casing(c)->simple_lowercase_mapping;
    const uint16_t index = GET_CASED_VALUE(mapping);
    if (CASING_IN_TABLE(mapping))
    {
        assert((size_t)index < COUNT_OF(unicorn_simple_case_mappings)); // LCOV_EXCL_BR_LINE
        lower = unicorn_simple_case_mappings[index];
    }
    else
    {
        const int32_t diff = (int32_t)index;
        const int32_t cp = (int32_t)c - (diff - CASING_DIFF);
        lower = (unichar)cp;
    }
    return lower;
#else
    return c;
#endif
}

UNICORN_API unichar uni_toupper(unichar c)
{
#if defined(UNICORN_FEATURE_SIMPLE_UPPERCASE_MAPPINGS)
    unichar upper;
    const uint16_t mapping = unicorn_get_character_casing(c)->simple_uppercase_mapping;
    const uint16_t index = GET_CASED_VALUE(mapping);
    if (CASING_IN_TABLE(mapping))
    {
        assert((size_t)index < COUNT_OF(unicorn_simple_case_mappings)); // LCOV_EXCL_BR_LINE
        upper = unicorn_simple_case_mappings[index];
    }
    else
    {
        const int32_t diff = (int32_t)index;
        const int32_t cp = (int32_t)c - (diff - CASING_DIFF);
        upper = (unichar)cp;
    }
    return upper;
#else
    return c;
#endif
}

UNICORN_API unichar uni_totitle(unichar c)
{
#if defined(UNICORN_FEATURE_SIMPLE_TITLECASE_MAPPINGS)
    unichar title;
    const uint16_t mapping = unicorn_get_character_casing(c)->simple_titlecase_mapping;
    const uint16_t index = GET_CASED_VALUE(mapping);
    if (CASING_IN_TABLE(mapping))
    {
        assert((size_t)index < COUNT_OF(unicorn_simple_case_mappings)); // LCOV_EXCL_BR_LINE
        title = unicorn_simple_case_mappings[index];
    }
    else
    {
        const int32_t diff = (int32_t)index;
        const int32_t cp = (int32_t)c - (diff - CASING_DIFF);
        title = (unichar)cp;
    }
    return title;
#else
    return c;
#endif
}
