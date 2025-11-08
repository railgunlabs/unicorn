/*
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

// Consult Unicode Standard 15.0, Chapter 3.13 Default Case Algorithms for details.

#include "common.h"
#include "normalize.h"
#include "unidata.h"
#include "buffer.h"
#include "charbuf.h"

#if defined(UNICORN_FEATURE_CASEFOLD_DEFAULT)

struct CaseState
{
    int32_t offset;
    struct UDynamicBuffer tmp;
    struct UDynamicBuffer buf;
};

static unisize unicorn_get_casefolding(unichar character, unichar casefolding[UNICORN_MAX_CASEFOLDING])
{
    const unichar *data = &uni_casefold_default_mappings[get_codepoint_data(character)->full_casefold_mapping_offset];
    unisize codepoint_count = (unisize)data[0];
    if (codepoint_count == 0)
    {
        // This code point does not case fold.
        // Return it as-is.
        casefolding[0] = character;
        codepoint_count = 1;
    }
    else
    {
        assert(codepoint_count < UNICORN_MAX_CASEFOLDING); // LCOV_EXCL_BR_LINE
        (void)memcpy(casefolding, &data[1], sizeof(data[0]) * (size_t)codepoint_count);
    }
    return codepoint_count;
}

#if defined(UNICORN_FEATURE_CASEFOLD_CANONICAL)
static bool is_canonical_caseless_stable(unichar ch)
{
    bool is_stable;
    const uint32_t flags = (uint32_t)get_codepoint_data(ch)->flags;

    if ((flags & UNICORN_CHAR_NEEDS_NORMALIZATION) == UNICORN_CHAR_NEEDS_NORMALIZATION)
    {
        is_stable = false;
    }
    else if ((flags & UNICORN_CHAR_CHANGES_WHEN_CASEFOLDED) == UNICORN_CHAR_CHANGES_WHEN_CASEFOLDED)
    {
        is_stable = false;
    }
    else
    {
        is_stable = unicorn_is_stable_nfd(ch);
    }

    return is_stable;
}

static void init_casefold(struct CaseState *cs)
{
    (void)memset(cs, 0, sizeof(cs[0]));
    udynbuf_init(&cs->buf);
    udynbuf_init(&cs->tmp);
}

static void ucs_free(struct CaseState *cs)
{
    udynbuf_free(&cs->buf);
    udynbuf_free(&cs->tmp);
}

static void ucs_reset(struct CaseState *cs)
{
    udynbuf_reset(&cs->buf);
    udynbuf_reset(&cs->tmp);
    cs->offset = 0;
}

static unisize ucs_length(const struct CaseState *cs)
{
    return udynbuf_length(&cs->buf) - cs->offset;
}

static unichar ucs_pop(struct CaseState *cs)
{
    const unichar ch = cs->buf.chars[cs->offset];
    cs->offset += 1;
    return ch;
}

static unistat collect_stable_run(struct CaseState *s, struct unitext *it)
{
    unistat status = UNI_OK;
    unisize index = it->index;
    unichar cp = UNICHAR_C(0);

    while (udynbuf_length(&s->buf) < udynbuf_capacity(&s->buf))
    {
        status = uni_next(it->data, it->length, it->encoding, &index, &cp);
        const bool is_stable = is_canonical_caseless_stable(cp);
        if ((status == UNI_OK) && is_stable)
        {
            udynbuf_append_unsafe(&s->buf, &cp, 1);
            it->index = index;
        }
        else
        {
            break;
        }
    }

    if (status == UNI_DONE)
    {
        status = UNI_OK;
    }

    return status;
}

static unistat collect_unstable_run(struct unitext *it, unisize *span_length, bool *needs_normalization)
{
    // LCOV_EXCL_START
    assert(it != NULL);
    assert(span_length != NULL);
    assert(needs_normalization != NULL);
    assert(*span_length == 0);
    assert(*needs_normalization == false);
    // LCOV_EXCL_STOP

    // Count the number of consecutive unstable code points.
    unistat status = UNI_OK;
    for (;;)
    {
        unichar cp = UNICHAR_C(0);
        unisize index = it->index;
        status = uni_next(it->data, it->length, it->encoding, &index, &cp);

        const bool is_stable = is_canonical_caseless_stable(cp);
        if ((status != UNI_OK) || is_stable)
        {
            break;
        }

        // The Unicode Standard requires that canonical decomposition (NFD normalization) be performed
        // before case folding when the string contains the character U+0345 (Combining Greek Ypogegrammeni)
        // or any character that has it as part of its canonical decomposition, such as U+1FC3 (Greek Small
        // Letter Eta with Ypogegrammeni).
        const uint32_t flags = (uint32_t)get_codepoint_data(cp)->flags;
        if ((flags & UNICORN_CHAR_NEEDS_NORMALIZATION) == UNICORN_CHAR_NEEDS_NORMALIZATION)
        {
            *needs_normalization = true;
        }
        it->index = index;
        *span_length += 1;
    }

    if (status == UNI_DONE)
    {
        status = UNI_OK;
    }

    return status;
}

// The caller has already vetted the text for well-formedness therefore this function assumes successful return codes.
static unistat normalize_text(struct unitext *it, struct UDynamicBuffer *text)
{
    unistat status = UNI_OK;
    const unisize initial_index = it->index;
    struct UNormalizeState state;

    // Compute the length of the stable run before allocating a buffer large enough for it.
    unisize length = 0;
    nstate_init(&state, &unicorn_is_stable_nfd);
    for (;;)
    {
        status = unorm_append_run(&state, it);
        if (status == UNI_OK)
        {
            length += state.decomp.length;
            ustate_reset(&state);
        }
        else
        {
            if (status == UNI_DONE)
            {
                status = UNI_OK;
            }
            break;
        }
    }

    if (status == UNI_OK)
    {
        status = udynbuf_reserve(text, length);
        if (status == UNI_OK)
        {
            it->index = initial_index; // Rewind the text iterator to where it began.
            for (;;)
            {
                status = unorm_append_run(&state, it);
                if (status == UNI_OK)
                {
                    udynbuf_append_unsafe(text, state.decomp.chars, state.decomp.length);
                    ustate_reset(&state);
                }
                else
                {
                    assert(status == UNI_DONE); // LCOV_EXCL_BR_LINE
                    status = UNI_OK;
                    break;
                }
            }
        }
    }

    nstate_free(&state);
    return status;
}

static unistat collect_run(struct CaseState *s, struct unitext *it)
{
    unistat status;
    unichar cp;
    const unisize startpos = it->index;
    ucs_reset(s);

    // Retrieve the current code point.
    status = uni_next(it->data, it->length, it->encoding, &it->index, &cp);
    it->index = startpos;

    if (status == UNI_OK)
    {
        // If the current code point is stable, then collect a run of all stable code points.
        // These are code points which do not change when case folded or normalized.
        if (is_canonical_caseless_stable(cp))
        {
            status = collect_stable_run(s, it);
        }
        else
        {
            // Count the number of consecutive unstable code points.
            unisize span_length = 0;
            bool needs_normalization = false;
            status = collect_unstable_run(it, &span_length, &needs_normalization);
            if (status == UNI_OK)
            {
                // Normalize the buffer if needed.
                if (needs_normalization)
                {
                    struct unitext tmp = {it->data, startpos, it->index, it->encoding};
                    status = normalize_text(&tmp, &s->buf);
                }
                else
                {
                    // Resize the buffer and copy the code points.
                    status = udynbuf_reserve(&s->buf, span_length);
                    if (status == UNI_OK)
                    {
                        it->index = startpos;
                        for (unisize i = 0; i < span_length; i++)
                        {
                            // There should be no errors since the unstable run was pre-calculated above;
                            // in other words only known-to-be-valid characters are iterated here.
                            (void)uni_next(it->data, it->length, it->encoding, &it->index, &cp);
                            udynbuf_append_unsafe(&s->buf, &cp, 1);
                        }
                    }
                }
            }

            if (status == UNI_OK)
            {
                // Case fold the text.
                span_length = 0;
                for (unisize i = 0; i < s->buf.length; i++)
                {
                    unichar unused[UNICORN_MAX_CASEFOLDING];
                    span_length += unicorn_get_casefolding(s->buf.chars[i], unused);
                }

                status = udynbuf_reserve(&s->tmp, span_length);
                if (status == UNI_OK)
                {
                    for (unisize i = 0; i < s->buf.length; i++)
                    {
                        unichar chars[UNICORN_MAX_CASEFOLDING];
                        const unisize chars_length = unicorn_get_casefolding(s->buf.chars[i], chars);
                        udynbuf_append_unsafe(&s->tmp, chars, chars_length);
                    }

                    // Normalize the text.
                    span_length = 0;
                    status = uni_norm(UNI_NFD, s->tmp.chars, s->tmp.length, UNI_SCALAR | UNI_TRUST, NULL, &span_length, UNI_SCALAR);
                    if (status == UNI_OK)
                    {
                        status = udynbuf_reserve(&s->buf, span_length);
                        if (status == UNI_OK)
                        {
                            status = uni_norm(UNI_NFD, s->tmp.chars, s->tmp.length, UNI_SCALAR | UNI_TRUST, s->buf.chars, &span_length, UNI_SCALAR);
                            if (status == UNI_OK)
                            {
                                s->buf.length = span_length;
                            }
                        }
                    }
                }
            }
        }
    }

    return status;
}
#endif

static unistat to_casefold(const void *src, unisize src_len, uniattr src_attr, void *dst, unisize *dst_len, uniattr dst_attr)
{
    struct UBuffer buffer = {NULL};
    unistat status = ubuffer_init(&buffer, dst, dst_len, dst_attr);

    if (status == UNI_OK)
    {
        status = unicorn_check_input_encoding(src, src_len, &src_attr);
    }

    if (status == UNI_OK)
    {
        struct unitext input = {src, 0, src_len, src_attr};
        for (;;)
        {
            unichar cp;
            status = uni_next(input.data, input.length, input.encoding, &input.index, &cp);
            if (status != UNI_OK)
            {
                break;
            }

            unichar chars[UNICORN_MAX_CASEFOLDING];
            const unisize chars_count = unicorn_get_casefolding(cp, chars);
            ubuffer_append(&buffer, chars, chars_count);
        }

        if (status == UNI_DONE)
        {
            status = ubuffer_finalize(&buffer);
        }
    }

    return status;
}

static unistat caseless_binary_compare(const void *s1, unisize s1_len, uniattr s1_attr, const void *s2, unisize s2_len, uniattr s2_attr, bool *equal)
{
    struct CFRun
    {
        unisize index;
        unisize length;
        struct unitext it;
        unichar chars[UNICORN_MAX_CASEFOLDING];
    };

    unistat status = UNI_OK;
    if (equal == NULL)
    {
        uni_message("required argument 'result' is null");
        status = UNI_BAD_OPERATION;
    }

    if (status == UNI_OK)
    {
        status = unicorn_check_input_encoding(s1, s1_len, &s1_attr);
    }

    if (status == UNI_OK)
    {
        status = unicorn_check_input_encoding(s2, s2_len, &s2_attr);
    }

    if (status == UNI_OK)
    {
        struct CFRun run1 = {.it = {s1, 0, s1_len, s1_attr}};
        struct CFRun run2 = {.it = {s2, 0, s2_len, s2_attr}};
        unichar cp;
        *equal = false;
        
        bool mismatch = false;
        while (!mismatch)
        {
            if (run1.index == run1.length)
            {
                run1.index = 0;
                run1.length = 0;
                status = uni_next(run1.it.data, run1.it.length, run1.it.encoding, &run1.it.index, &cp);
                if (status == UNI_OK)
                {
                    run1.length = unicorn_get_casefolding(cp, run1.chars);
                }
                else if (status == UNI_DONE)
                {
                    status = UNI_OK;
                }
                else
                {
                    // No Action.
                }
            }

            if ((run2.index == run2.length) && (status == UNI_OK))
            {
                run2.index = 0;
                run2.length = 0;
                status = uni_next(run2.it.data, run2.it.length, run2.it.encoding, &run2.it.index, &cp);
                if (status == UNI_OK)
                {
                    run2.length = unicorn_get_casefolding(cp, run2.chars);
                }
                else if (status == UNI_DONE)
                {
                    status = UNI_OK;
                }
                else
                {
                    // No Action.
                }
            }

            if ((status != UNI_OK) || (run1.length == 0) || (run2.length == 0))
            {
                break;
            }

            while ((run1.index < run1.length) &&
                   (run2.index < run2.length))
            {
                if (run1.chars[run1.index] != run2.chars[run2.index])
                {
                    mismatch = true;
                    break;
                }
                run1.index += 1;
                run2.index += 1;
            }
        }

        if ((status == UNI_OK) && !mismatch)
        {
            // If both buffers are empty then the strings are equal.
            if ((run1.length == 0) && (run2.length == 0))
            {
                *equal = true;
            }
        }
    }

    return status;
}

#if defined(UNICORN_FEATURE_CASEFOLD_CANONICAL)
static unistat to_casefold_normalized(const void *src, unisize src_len, uniattr src_attr, void *dst, unisize *dst_len, uniattr dst_attr)
{
    unistat status;
    struct UBuffer buf;
    struct CaseState cs = {0};

    status = ubuffer_init(&buf, dst, dst_len, dst_attr);

    if (status == UNI_OK)
    {
        status = unicorn_check_input_encoding(src, src_len, &src_attr);
    }

    if (status == UNI_OK)
    {
        struct unitext it = {src, 0, src_len, src_attr};
        init_casefold(&cs);

        for (;;)
        {
            status = collect_run(&cs, &it);
            if ((ucs_length(&cs) == 0) || (status != UNI_OK))
            {
                break;
            }
            ubuffer_append(&buf, cs.buf.chars, cs.buf.length);
        }

        if (status == UNI_DONE)
        {
            status = ubuffer_finalize(&buf);
        }
    }

    ucs_free(&cs);
    return status;
}

static unistat caseless_normalized_compare(const void *s1, unisize s1_len, uniattr s1_attr, const void *s2, unisize s2_len, uniattr s2_attr, bool *is_equal)
{
    unistat status = UNI_OK;
    struct CaseState cs1 = {0};
    struct CaseState cs2 = {0};

    if (is_equal == NULL)
    {
        uni_message("required argument 'result' is null");
        status = UNI_BAD_OPERATION;
    }
    
    if (status == UNI_OK)
    {
        status = unicorn_check_input_encoding(s1, s1_len, &s1_attr);
    }

    if (status == UNI_OK)
    {
        status = unicorn_check_input_encoding(s2, s2_len, &s2_attr);
    }

    if (status == UNI_OK)
    {
        struct unitext it1 = {s1, 0, s1_len, s1_attr};
        struct unitext it2 = {s2, 0, s2_len, s2_attr};
        init_casefold(&cs1);
        init_casefold(&cs2);

        bool mismatch = false;
        while (!mismatch)
        {
            if (ucs_length(&cs1) == 0)
            {
                status = collect_run(&cs1, &it1);
                if (status == UNI_DONE)
                {
                    status = UNI_OK;
                }
            }

            if ((ucs_length(&cs2) == 0) && (status == UNI_OK))
            {
                status = collect_run(&cs2, &it2);
                if (status == UNI_DONE)
                {
                    status = UNI_OK;
                }
            }

            // If either buffer is empty, then stop processing.
            if ((ucs_length(&cs1) == 0) || (ucs_length(&cs2) == 0) || (status != UNI_OK))
            {
                break;
            }

            while ((ucs_length(&cs1) > 0) && (ucs_length(&cs2) > 0))
            {
                const unichar cp1 = ucs_pop(&cs1);
                const unichar cp2 = ucs_pop(&cs2);
                if (cp1 != cp2)
                {
                    mismatch = true;
                    break;
                }
            }
        }
        
        if (status == UNI_OK)
        {
            // If both buffers are empty then the strings are equal; otherwise return the
            // lexicographical location of the first character in the non-empty string.
            if (!mismatch && (ucs_length(&cs1) == 0) && (ucs_length(&cs2) == 0))
            {
                *is_equal = true;
            }
            else
            {
                *is_equal = false;
            }
        }
    }

    ucs_free(&cs1);
    ucs_free(&cs2);
    return status;
}
#endif

static unistat is_casefold(const void *text, unisize length, uniattr encoding, bool *result)
{
    unistat status = unicorn_check_input_encoding(text, length, &encoding);

    if (status == UNI_OK)
    {
        *result = true;
        struct unitext input = {text, 0, length, encoding};
        while ((status == UNI_OK) && (*result))
        {
            // Don't update the iterator here; update it when comparing characters in the inner loop.
            unichar cp;
            unisize tmp = input.index;
            status = uni_next(input.data, input.length, input.encoding, &tmp, &cp);
            if (status == UNI_OK)
            {
                // Case fold the character and compare it against the input text.
                unichar chars[UNICORN_MAX_CASEFOLDING];
                const unisize chars_count = unicorn_get_casefolding(cp, chars);
                for (unisize i = 0; (i < chars_count); i++)
                {
                    status = uni_next(input.data, input.length, input.encoding, &input.index, &cp);

                    // Decoding the character will always be successful because case folding always transforms
                    // one code point into one or more different code points and therefore if case folding
                    // does occur, then there will only be a mismatch on the first character which has
                    // already been checked/decoded by the outer loop.
                    assert(status == UNI_OK); // LCOV_EXCL_BR_LINE

                    if (cp != chars[i])
                    {
                        *result = false;
                        break;
                    }
                }
            }
        }

        if (status == UNI_DONE)
        {
            status = UNI_OK;
        }
    }

    return status;
}

#if defined(UNICORN_FEATURE_CASEFOLD_CANONICAL)
static unistat is_casefold_canonical(const void *text, unisize length, uniattr encoding, bool *result)
{
    unistat status = unicorn_check_input_encoding(text, length, &encoding);

    if (status == UNI_OK)
    {
        struct CaseState cs;
        struct unitext input = {text, 0, length, encoding};
        struct unitext prev;

        init_casefold(&cs);
        *result = true;

        while (*result)
        {
            prev = input;
            status = collect_run(&cs, &input);
            if (status != UNI_OK)
            {
                break;
            }

            while (ucs_length(&cs) > 0)
            {
                unichar cp;
                status = uni_next(prev.data, prev.length, prev.encoding, &prev.index, &cp);

                // Normalization can only ever expand the text and any malformed characters
                // will have been decoded when collecting the normalization run therefore
                // there is no chance of a malformed character at this point.
                assert(status == UNI_OK); // LCOV_EXCL_BR_LINE

                if (cp != ucs_pop(&cs))
                {
                    *result = false;
                    break;
                }
            }
        }

        if (status == UNI_DONE)
        {
            status = UNI_OK;
        }

        ucs_free(&cs);
    }

    return status;
}
#endif

#endif

unistat uni_casefold(unicasefold casing, const void *src, unisize src_len, uniattr src_attr, void *dst, unisize *dst_len, uniattr dst_attr) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
    unistat status = UNI_OK;

    if (casing == UNI_DEFAULT)
    {
#if defined(UNICORN_FEATURE_CASEFOLD_DEFAULT)
        status = to_casefold(src, src_len, src_attr, dst, dst_len, dst_attr);
#else
        status = UNI_FEATURE_DISABLED;
#endif
    }
    else if (casing == UNI_CANONICAL)
    {
#if defined(UNICORN_FEATURE_CASEFOLD_CANONICAL)
        status = to_casefold_normalized(src, src_len, src_attr, dst, dst_len, dst_attr);
#else
        status = UNI_FEATURE_DISABLED;
#endif
    }
    else
    {
        uni_message("illegal case fold form");
        status = UNI_BAD_OPERATION;
    }

    return status;
}

unistat uni_casefoldcmp(unicasefold casing, const void *s1, unisize s1_len, uniattr s1_attr, const void *s2, unisize s2_len, uniattr s2_attr, bool *result) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
    unistat status = UNI_OK;

    if (casing == UNI_DEFAULT)
    {
#if defined(UNICORN_FEATURE_CASEFOLD_DEFAULT)
        status = caseless_binary_compare(s1, s1_len, s1_attr, s2, s2_len, s2_attr, result);
#else
        status = UNI_FEATURE_DISABLED;
#endif
    }
    else if (casing == UNI_CANONICAL)
    {
#if defined(UNICORN_FEATURE_CASEFOLD_CANONICAL)
        status = caseless_normalized_compare(s1, s1_len, s1_attr, s2, s2_len, s2_attr, result);
#else
        status = UNI_FEATURE_DISABLED;
#endif
    }
    else
    {
        uni_message("illegal case fold form");
        status = UNI_BAD_OPERATION;
    }

    return status;
}

unistat uni_casefoldchk(unicasefold casing, const void *text, unisize text_len, uniattr text_attr, bool *result) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
    unistat status = UNI_OK;

    if (result == NULL)
    {
        uni_message("required argument 'result' is null");
        status = UNI_BAD_OPERATION;
    }
    else if (casing == UNI_DEFAULT)
    {
#if defined(UNICORN_FEATURE_CASEFOLD_DEFAULT)
        status = is_casefold(text, text_len, text_attr, result);
#else
        status = UNI_FEATURE_DISABLED;
#endif
    }
    else if (casing == UNI_CANONICAL)
    {
#if defined(UNICORN_FEATURE_CASEFOLD_CANONICAL)
        status = is_casefold_canonical(text, text_len, text_attr, result);
#else
        status = UNI_FEATURE_DISABLED;
#endif
    }
    else
    {
        uni_message("illegal case fold form");
        status = UNI_BAD_OPERATION;
    }

    return status;
}
