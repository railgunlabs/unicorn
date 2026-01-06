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

// Consult Unicode Standard 15.0, Chapter 3.13 Default Case Algorithms for details.

#include "common.h"
#include "unidata.h"
#include "charbuf.h"
#include "charvec.h"

#if defined(UNICORN_FEATURE_LOWERCASE_CONVERT) || defined(UNICORN_FEATURE_TITLECASE_CONVERT) || defined(UNICORN_FEATURE_UPPERCASE_CONVERT)

enum TitleCaseState
{
    PROCESS_FIRST_CASED,     // Looking for first cased character.
    PROCESS_COMBINING_MARKS, // The first cased character was found; process its combining marks as title case.
    PROCESS_REMAINING_CHARS  // Processing remaining characters as lowercase.
};

static bool is_cased(unichar ch)
{
    const uint32_t flags = (uint32_t)unicorn_get_codepoint_data(ch)->flags;
    return ((flags & IS_CASED) == IS_CASED) ? true : false;
}

static bool is_case_ignorable(unichar ch)
{
    const uint32_t flags = (uint32_t)unicorn_get_codepoint_data(ch)->flags;
    return ((flags & IS_CASE_IGNORABLE) == IS_CASE_IGNORABLE) ? true : false;
}

// The character 'C' is preceded by a sequence consisting of a cased letter and then zero
// or more case-ignorable characters, and C is not followed by a sequence consisting of
// zero or more case-ignorable characters and then a cased letter. Regular expression:
// 
// Before:
// 
//     \p{cased} (\p{Case_Ignorable})* 'C'
//
// After:
// 
//     'C' !( (\p{Case_Ignorable})* \p{cased} )
//
static void final_sigma_before(struct unitext input, bool *match)
{
    unistat status;
    unichar cp;
    *match = false;

    do
    {
        cp = FIRST_ILLEGAL_CODE_POINT;
        status = uni_prev(input.data, input.length, input.encoding, &input.index, &cp);
        if (status != UNI_OK)
        {
            assert(status == UNI_DONE); // LCOV_EXCL_BR_LINE
            break;
        }
    } while (is_case_ignorable(cp));

    if (is_cased(cp))
    {
        *match = true;
    }
}

static unistat final_sigma_after(struct unitext input, bool *match)
{
    unistat status;
    unichar cp;
    *match = true;

    do
    {
        cp = FIRST_ILLEGAL_CODE_POINT;
        status = uni_next(input.data, input.length, input.encoding, &input.index, &cp);
        if (status != UNI_OK)
        {
            if (status == UNI_DONE)
            {
                status = UNI_OK;
            }
            break;
        }
    } while (is_case_ignorable(cp));

    if (status == UNI_OK)
    {
        if (is_cased(cp))
        {
            *match = false;
        }
    }

    return status;
}

static unistat are_conditions_met(const USpecialCasing *special_casing, struct unitext before, struct unitext after, bool *result)
{
    // Assume the conditions are met until proved otherwise.
    unistat status = UNI_OK;
    bool left = false;
    bool right = false;
    *result = true;

    // Check if all conditions are met.
    const uint32_t conditions = (uint32_t)special_casing->conditions;
    if (conditions != CTX_NONE)
    {
        // As of Unicode 17.0, the only non-locale specific contextual
        // casing rule that is observed is the final sigma rule.
        assert(conditions == CTX_FINAL_SIGMA); // LCOV_EXCL_BR_LINE

        final_sigma_before(before, &left);
        status = final_sigma_after(after, &right);
        if (status == UNI_OK)
        {
            if (!(left && right))
            {
                *result = false;
            }
        }
    }

    return status;
}

static unichar get_simple_mapping(unichar ch, unicaseconv type)
{
    unichar mapping;
    switch (type) // LCOV_EXCL_BR_LINE
    {
    case UNI_LOWER:
        mapping = uni_tolower(ch);
        break;

    case UNI_TITLE:
        mapping = uni_totitle(ch);
        break;

    case UNI_UPPER:
        mapping = uni_toupper(ch);
        break;

    // LCOV_EXCL_START
    default:
        mapping = ch;
        break;
    // LCOV_EXCL_STOP
    }
    return mapping;
}

static unihash uni_hash_char(unichar cp)
{
    unihash hash = (unihash)cp;
    hash = ((hash >> UNIHASH_C(16)) ^ hash) * UNIHASH_C(0x45D9F3B);
    hash = ((hash >> UNIHASH_C(16)) ^ hash) * UNIHASH_C(0x45D9F3B);
    hash = (hash >> UNIHASH_C(16)) ^ hash;
    return hash;
}

static unihash uni_xorshift(unihash x)
{
    unihash hash = x;
    hash ^= hash << UNIHASH_C(13);
    hash ^= hash >> UNIHASH_C(17);
    hash ^= hash << UNIHASH_C(5);
    return hash;
}

static const USpecialCasing *get_special_case_mapping_conditions(unichar codepoint)
{
    const unihash hash = uni_hash_char(codepoint);
    const uint32_t length = SPECIAL_CASE_MAPPINGS_COUNT;
    const uint32_t seed_index = hash % length;
    const uint32_t value_index = uni_xorshift(hash + (unihash)unicorn_special_casings_seeds[seed_index]) % length;
    const USpecialCasing *mappings = &unicorn_special_casings_values[value_index];
    if (mappings->codepoint != codepoint)
    {
        mappings = NULL;
    }
    return mappings;
}

static const unichar *get_special_case_mapping(const USpecialCasing *conditions, unicaseconv type, unisize *count)
{
    unisize charblock_offset = -1;
    unisize charblock_length = -1;

#if defined(UNICORN_FEATURE_UPPERCASE_CONVERT)
    if (type == UNI_UPPER)
    {
        charblock_offset = (unisize)uni_special_uppercase_mappings[conditions->case_mappings_offset].offset;
        charblock_length = (unisize)uni_special_uppercase_mappings[conditions->case_mappings_offset].count;
    }
#endif

#if defined(UNICORN_FEATURE_TITLECASE_CONVERT)
    if (type == UNI_TITLE)
    {
        charblock_offset = (unisize)uni_special_titlecase_mappings[conditions->case_mappings_offset].offset;
        charblock_length = (unisize)uni_special_titlecase_mappings[conditions->case_mappings_offset].count;
    }
#endif

#if defined(UNICORN_FEATURE_LOWERCASE_CONVERT)
    if (type == UNI_LOWER)
    {
        charblock_offset = (unisize)uni_special_lowercase_mappings[conditions->case_mappings_offset].offset;
        charblock_length = (unisize)uni_special_lowercase_mappings[conditions->case_mappings_offset].count;
    }
#endif

    // LCOV_EXCL_START
    assert(charblock_offset >= 0);
    assert(charblock_length >= 0);
    // LCOV_EXCL_STOP

    *count = (unisize)charblock_length;
    return &unicorn_special_case_codepoints[charblock_offset];
}

static unistat append_cased(struct CharBuf *buffer, struct unitext before, struct unitext after, unichar ch, unicaseconv casing)
{
    unistat status = UNI_OK;
    bool match = false;

    const USpecialCasing *special_casing = get_special_case_mapping_conditions(ch);
    if (special_casing != NULL)
    {
        status = are_conditions_met(special_casing, before, after, &match);
        if ((status == UNI_OK) && match)
        {
            // If all conditions are met, then perform the replacement.
            unisize chars_count;
            const unichar *chars = get_special_case_mapping(special_casing, casing, &chars_count);
            uni_charbuf_append(buffer, chars, chars_count);
        }
    }

    if ((status == UNI_OK) && !match)
    {
        // Fallback to simple case mapping.
        uni_charbuf_appendchar(buffer, get_simple_mapping(ch, casing));
    }

    return status;
}

static unistat check_mapping(struct unitext before, struct unitext after, unichar ch, unicaseconv casing, bool *result)
{
    unistat status = UNI_OK;
    bool match = false;
    *result = true;

    const USpecialCasing *special_casing = get_special_case_mapping_conditions(ch);
    if (special_casing != NULL)
    {
        status = are_conditions_met(special_casing, before, after, &match);
        if ((status == UNI_OK) && match)
        {
            // If all conditions are met, then perform the replacement.
            unisize chars_count = 0;
            const unichar *chars = get_special_case_mapping(special_casing, casing, &chars_count);
            for (unisize i = 0; (i < chars_count) && (*result == true); i++)
            {
                status = uni_next(before.data, before.length, before.encoding, &before.index, &ch);
                assert(status == UNI_OK); // LCOV_EXCL_BR_LINE

                if (chars[i] != ch)
                {
                    *result = false;
                }
            }
        }
    }

    if ((status == UNI_OK) && !match)
    {
        // Fallback to simple case mapping.
        if (ch != get_simple_mapping(ch, casing))
        {
            *result = false;
        }
    }

    return status;
}

#if defined(UNICORN_FEATURE_TITLECASE_CONVERT)
static unistat to_titlecase(const void *src, unisize src_len, uniattr src_attr, void *dst, unisize *dst_len, uniattr dst_attr)
{
    struct CharBuf buffer = {NULL};
    unistat status = uni_charbuf_init(&buffer, dst, dst_len, dst_attr);

    if (status == UNI_OK)
    {
        status = uni_check_input_encoding(src, src_len, &src_attr);
    }

    if (status == UNI_OK)
    {
        unisize index = 0;
        while (status == UNI_OK)
        {
            unisize next = index;
            status = uni_nextbrk(UNI_WORD, src, src_len, src_attr, &next);
            if (status == UNI_OK)
            {
                // Track whether the first cased character was encountered or not;
                // the first cased character should be title cased and all remaining
                // cased characters should be lowercased.
                enum TitleCaseState state = PROCESS_FIRST_CASED;

                while (index < next)
                {
                    unichar cp;
                    const struct unitext prev_text = {src, index, src_len, src_attr};
                    (void)uni_next(src, src_len, src_attr, &index, &cp);
                    const struct unitext next_text = {src, index, src_len, src_attr};
                    const uint8_t ccc = unicorn_get_codepoint_data(cp)->canonical_combining_class;
                    const bool cased = is_cased(cp);

                    if ((state == PROCESS_FIRST_CASED) && cased)
                    {
                        status = append_cased(&buffer, prev_text, next_text, cp, UNI_TITLE);
                        state = PROCESS_COMBINING_MARKS;
                    }
                    else if ((state == PROCESS_COMBINING_MARKS) && (ccc > (uint8_t)0))
                    {
                        status = append_cased(&buffer, prev_text, next_text, cp, UNI_TITLE);
                    }
                    else
                    {
                        status = append_cased(&buffer, prev_text, next_text, cp, UNI_LOWER);
                        if (state == PROCESS_COMBINING_MARKS)
                        {
                            state = PROCESS_REMAINING_CHARS;
                        }
                    }
                }
            }
        }

        if (status == UNI_DONE)
        {
            status = uni_charbuf_finalize(&buffer);
        }
    }

    return status;
}

static unistat is_titlecase(const void *src, unisize src_len, uniattr src_attr, bool *result)
{
    unistat status = uni_check_input_encoding(src, src_len, &src_attr);

    if (status == UNI_OK)
    {
        unisize index = 0;
        while (status == UNI_OK)
        {
            unisize next = index;
            status = uni_nextbrk(UNI_WORD, src, src_len, src_attr, &next);
            if (status == UNI_OK)
            {
                // Track whether the first cased character was encountered or not;
                // the first cased character should be title cased and all remaining
                // cased characters should be lowercased.
                enum TitleCaseState state = PROCESS_FIRST_CASED;

                while (index < next)
                {
                    unichar cp;
                    const struct unitext prev_text = {src, index, src_len, src_attr};
                    (void)uni_next(src, src_len, src_attr, &index, &cp);
                    const struct unitext next_text = {src, index, src_len, src_attr};
                    const uint8_t ccc = unicorn_get_codepoint_data(cp)->canonical_combining_class;
                    const bool cased = is_cased(cp);

                    if ((state == PROCESS_FIRST_CASED) && cased)
                    {
                        status = check_mapping(prev_text, next_text, cp, UNI_TITLE, result);
                        state = PROCESS_COMBINING_MARKS;
                    }
                    else if ((state == PROCESS_COMBINING_MARKS) && (ccc > (uint8_t)0))
                    {
                        status = check_mapping(prev_text, next_text, cp, UNI_TITLE, result);
                    }
                    else
                    {
                        status = check_mapping(prev_text, next_text, cp, UNI_LOWER, result);
                        if (state == PROCESS_COMBINING_MARKS)
                        {
                            state = PROCESS_REMAINING_CHARS;
                        }
                    }

                    if (*result == false)
                    {
                        break;
                    }
                }
            }

            if (*result == false)
            {
                break;
            }
        }

        if (status == UNI_DONE)
        {
            status = UNI_OK;
        }
    }

    return status;
}
#endif

static unistat to_case(unicaseconv casing, const void *src, unisize src_len, uniattr src_attr, void *dst, unisize *dst_len, uniattr dst_attr)
{
    struct CharBuf buffer = {NULL};
    unistat status = uni_charbuf_init(&buffer, dst, dst_len, dst_attr);

    if (status == UNI_OK)
    {
        status = uni_check_input_encoding(src, src_len, &src_attr);
    }

    if (status == UNI_OK)
    {
        struct unitext text = {src, 0, src_len, src_attr};
        while (status == UNI_OK)
        {
            unichar cp;
            struct unitext prev_text = text;
            status = uni_next(text.data, text.length, text.encoding, &text.index, &cp);
            if (status == UNI_OK)
            {
                status = append_cased(&buffer, prev_text, text, cp, casing);
            }
        }

        if (status == UNI_DONE)
        {
            status = uni_charbuf_finalize(&buffer);
        }
    }

    return status;
}

static unistat is_case(unicaseconv casing, const void *string, unisize length, uniattr encoding, bool *result)
{
    unistat status = uni_check_input_encoding(string, length, &encoding);
    
    if (status == UNI_OK)
    {
        *result = true;
        struct unitext text = {string, 0, length, encoding};
        while ((status == UNI_OK) && (*result))
        {
            unichar cp;
            struct unitext prev_text = text;
            status = uni_next(text.data, text.length, text.encoding, &text.index, &cp);
            if (status == UNI_OK)
            {
                status = check_mapping(prev_text, text, cp, casing, result);
            }
        }

        if (status == UNI_DONE)
        {
            status = UNI_OK;
        }
    }
    
    return status;
}
#endif

UNICORN_API unistat uni_caseconv(unicaseconv casing, const void *src, unisize src_len, uniattr src_attr, void *dst, unisize *dst_len, uniattr dst_attr) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
    unistat status = UNI_OK;

    switch (casing)
    {
    case UNI_LOWER:
#if defined(UNICORN_FEATURE_LOWERCASE_CONVERT)
        status = to_case(casing, src, src_len, src_attr, dst, dst_len, dst_attr);
#else
        status = UNI_FEATURE_DISABLED;
#endif
        break;

    case UNI_UPPER:
#if defined(UNICORN_FEATURE_UPPERCASE_CONVERT)
        status = to_case(casing, src, src_len, src_attr, dst, dst_len, dst_attr);
#else
        status = UNI_FEATURE_DISABLED;
#endif
        break;

    case UNI_TITLE:
#if defined(UNICORN_FEATURE_TITLECASE_CONVERT)
        status = to_titlecase(src, src_len, src_attr, dst, dst_len, dst_attr);
#else
        status = UNI_FEATURE_DISABLED;
#endif
        break;

    default:
        uni_message("illegal casing");
        status = UNI_BAD_OPERATION;
        break;
    }

    return status;
}

UNICORN_API unistat uni_caseconvchk(unicaseconv casing, const void *text, unisize text_len, uniattr text_attr, bool *result) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
    unistat status = UNI_OK;

    if (result == NULL)
    {
        uni_message("required argument 'result' is null");
        status = UNI_BAD_OPERATION;
    }
    else
    {
        switch (casing)
        {
        case UNI_LOWER:
#if defined(UNICORN_FEATURE_LOWERCASE_CONVERT)
            status = is_case(casing, text, text_len, text_attr, result);
#else
            status = UNI_FEATURE_DISABLED;
#endif
            break;

        case UNI_UPPER:
#if defined(UNICORN_FEATURE_UPPERCASE_CONVERT)
            status = is_case(casing, text, text_len, text_attr, result);
#else
            status = UNI_FEATURE_DISABLED;
#endif
            break;

        case UNI_TITLE:
#if defined(UNICORN_FEATURE_TITLECASE_CONVERT)
            status = is_titlecase(text, text_len, text_attr, result);
#else
            status = UNI_FEATURE_DISABLED;
#endif
            break;

        default:
            uni_message("illegal case");
            status = UNI_BAD_OPERATION;
            break;
        }
    }

    return status;
}
