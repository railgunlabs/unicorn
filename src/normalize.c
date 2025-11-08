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

#include "normalize.h"
#include "buffer.h"
#include "unidata.h"

#if defined(UNICORN_FEATURE_NFD)

static const int32_t hangul_S_base = 0xAC00;
static const int32_t hangul_L_base = 0x1100;
static const int32_t hangul_V_base = 0x1161;
static const int32_t hangul_T_base = 0x11A7;
static const int32_t hangul_V_count = 21;
static const int32_t hangul_T_count = 28;
static const int32_t hangul_S_count = 11172;

typedef uint32_t(*QuickCheckFunc)(unichar cp);

// The conditional logic in the following function is for algorithmically
// decomposing characters in the Korean alphabet, also known as Hangul.
// The algorithm is described in the Unicode Standard. Refer to it for details.
static unisize unicorn_get_canonical_decomposition(unichar character, unichar *decomposition)
{
    static const int32_t hangul_N_count = 588;
    const int32_t S_index = (int32_t)character - hangul_S_base;

    unisize unichar_count = 0;
    if ((S_index < 0) || (S_index >= hangul_S_count))
    {
        // Non-Hangul character: look up its decomposition in the decomposition mappings table.
#if defined(UNICORN_OPTIMIZE_FOR_SPEED)
        // When optimizing for speed, the decomposition is stored as UTF-32.
        // Each character can be copied as-is to the destination buffer.
        const unichar *chars = &uni_canonical_decomp_mappings[get_codepoint_data(character)->canonical_decomposition_mapping_offset];
        const unichar chars_len = chars[0];
        assert(chars_len < (unichar)LONGEST_RAW_DECOMPOSITION); // LCOV_EXCL_BR_LINE
        chars = &chars[1]; // Skip past length to the first code unit in the UTF-8 sequence.
        if (decomposition != NULL)
        {
            (void)memcpy(decomposition, chars, sizeof(chars[0]) * chars_len);
        }
        unichar_count += (unisize)chars_len;
#else
        // When optimizing for size, the decomposition is stored as UTF-8.
        // Each character must be decoded and copied to the destination buffer.
        const uint8_t *utf8 = &uni_canonical_decomp_mappings[get_codepoint_data(character)->canonical_decomposition_mapping_offset];
        const uint8_t utf8_len = utf8[0];
        assert(utf8_len < (uint8_t)LONGEST_RAW_DECOMPOSITION); // LCOV_EXCL_BR_LINE
        utf8 = &utf8[1]; // Skip past length to the first code unit in the UTF-8 sequence.

        unisize index = 0;
        while (index < (unisize)utf8_len)
        {
            unichar cp;
            const unistat status = uni_next(utf8, utf8_len, UNI_UTF8 | UNI_TRUST, &index, &cp);
            assert(status == UNI_OK); // LCOV_EXCL_BR_LINE
            if (decomposition != NULL)
            {
                decomposition[unichar_count] = cp;
            }
            unichar_count += 1;
        }
#endif
    }
    else
    {
        const int32_t L = hangul_L_base + (S_index / hangul_N_count);
        const int32_t V = hangul_V_base + ((S_index % hangul_N_count) / hangul_T_count);
        const int32_t T = hangul_T_base + (S_index % hangul_T_count);

        // Hangul character: compute its decomposition algorithmically.
        if (T != hangul_T_base)
        {
            if (decomposition != NULL)
            {
                decomposition[0] = (unichar)L;
                decomposition[1] = (unichar)V;
                decomposition[2] = (unichar)T;
            }
            unichar_count = 3;
        }
        else
        {
            if (decomposition != NULL)
            {
                decomposition[0] = (unichar)L;
                decomposition[1] = (unichar)V;
            }
            unichar_count = 2;
        }
    }

    return unichar_count;
}

void nstate_init(struct UNormalizeState *state, IsStable is_stable)
{
    (void)memset(state, 0, sizeof(state[0]));
    state->is_stable = is_stable;
    udynbuf_init(&state->span);
    udynbuf_init(&state->decomp);
}

void nstate_free(struct UNormalizeState *state)
{
    udynbuf_free(&state->span);
    udynbuf_free(&state->decomp);
}

void ustate_reset(struct UNormalizeState *ns)
{
    udynbuf_reset(&ns->decomp);
    ns->offset = 0;
}

static bool unorm_is_empty(const struct UNormalizeState *state)
{
    return state->offset == state->decomp.length;
}

#if defined(UNICORN_FEATURE_NFC)
static inline bool is_hangul_syllable_L(unichar ch)
{
    static const int32_t hangul_L_count = 19;
    const int32_t L_index = (int32_t)ch - hangul_L_base;
    return (L_index >= 0) && (L_index < hangul_L_count);
}

static inline bool is_hangul_syllable_V(unichar ch)
{
    const int32_t V_index = (int32_t)ch - hangul_V_base;
    return (V_index >= 0) && (V_index < hangul_V_count);
}

static inline bool is_hangul_syllable_T(unichar ch)
{
    const int32_t T_index = (int32_t)ch - hangul_T_base;
    return (T_index > 0) && (T_index < hangul_T_count);
}

static inline bool is_hangul_syllable_LV(unichar ch)
{
    const int32_t S_index = (int32_t)ch - hangul_S_base;
    return (S_index >= 0) && (S_index < hangul_S_count) && ((S_index % hangul_T_count) == 0);
}

static inline bool is_hangul_syllable_L_or_V_or_T(unichar ch)
{
    return (is_hangul_syllable_L(ch) || is_hangul_syllable_V(ch) || is_hangul_syllable_T(ch));
}
#endif

static inline int32_t get_ccc(unichar character)
{
    return (int32_t)get_codepoint_data(character)->canonical_combining_class;
}

// Check if the code point does not change under Normalization Form D (NFD).
bool unicorn_is_stable_nfd(unichar cp)
{
    bool is_stable;

    if (get_ccc(cp) > 0)
    {
        is_stable = false;
    }
    else if (unicorn_get_canonical_decomposition(cp, NULL) > 0)
    {
        is_stable = false;
    }
    else
    {
        is_stable = true;
    }

    return is_stable;
}

#if defined(UNICORN_FEATURE_NFC)
static bool is_stable_nfc(unichar ch)
{
    bool is_stable = true;
    const uint32_t flags = (uint32_t)get_codepoint_data(ch)->flags;

    // If this code point could be part of a composition, then treat it as unstable.
    // It may or may not compose with anything; it depends which code points surround it.
    if ((flags & CHAR_IS_COMPOSABLE) == CHAR_IS_COMPOSABLE)
    {
        is_stable = false;
    }
    // Hangul Jamo is a special case because its decompositions are algorithmically derived.
    else if (is_hangul_syllable_L_or_V_or_T(ch) || is_hangul_syllable_LV(ch))
    {
        is_stable = false;
    }
    else
    {
        is_stable = unicorn_is_stable_nfd(ch);
    }

    return is_stable;
}
#endif

static void sort_combining_marks(unichar codepoints[], int32_t length)
{
    for (int32_t i = 0; i < (length - 1); i++)
    {
        // Last 'i' code points are already in place so there is no need to inspect them again.
        for (int32_t j = 0; j < ((length - i) - 1); j++)
        {
            // Compare the Canonical Combining Classes of the code points.
            const int32_t ccc1 = get_ccc(codepoints[j]);
            const int32_t ccc2 = get_ccc(codepoints[j + 1]);
            if (ccc1 > ccc2)
            {
                const unichar temp = codepoints[j];
                codepoints[j] = codepoints[j + 1];
                codepoints[j + 1] = temp;
            }
        }
    }
}

#if defined(UNICORN_FEATURE_NFC)

static unichar find_canonical_composition(const struct CanonicalCompositionPair *const pairs, int32_t left, int32_t right, const unichar codepoint)
{
    unichar composition = UNICORN_LARGEST_CODE_POINT;
    if (right >= left)
    {
        const int32_t middle = left + ((right - left) / 2);
        const struct CanonicalCompositionPair *const pair = &pairs[middle];
        // Check the left or right subarray.
        if (pair->codepoint > codepoint)
        {
            composition = find_canonical_composition(pairs, left, middle - 1, codepoint); // cppcheck-suppress misra-c2012-17.2
        }
        else if (pair->codepoint < codepoint)
        {
            composition = find_canonical_composition(pairs, middle + 1, right, codepoint); // cppcheck-suppress misra-c2012-17.2
        }
        else
        {
            composition = pair->composed_codepoint;
        }
    }
    return composition;
}

static unichar find_composition(unichar starter, unichar non_starter)
{
    // Find the sorted array with all characters the starter character can compose with.
    const struct CodepointData *character = get_codepoint_data(starter);
    const struct CanonicalCompositionPair *const pairs = &uni_canonical_comp_pairs[character->canonical_composition_mapping_offset];
    const int32_t composition_mapping_count = (int32_t)character->canonical_composition_mapping_count;
    assert(composition_mapping_count > 0); // LCOV_EXCL_BR_LINE: This condition is checked for by the caller.
    return find_canonical_composition(pairs, 0, composition_mapping_count, non_starter); // cppcheck-suppress misra-c2012-17.2
}

static void compose_combining_character_sequence(struct UDynamicBuffer *buffer, unisize index)
{
    bool restart;
    do
    {
        // If two characters are composed (starter + combining character), then iteration must
        // restart from the beginning since the newly composed character might compose with yet
        // another character in the sequence. By default, assume no characters are composed.
        restart = false;

        // Grab the current starter character.
        // This will change as characters are composed together.
        unichar starter = buffer->chars[index];

        // Check if this code point can be composed with another.
        const bool is_composable = get_codepoint_data(starter)->canonical_composition_mapping_count > (uint8_t)0;

        // This character should be a starter.
        if ((get_ccc(starter) != 0) || !is_composable)
        {
            break;
        }

        // Begin iteration from the next character.
        int32_t prev_ccc = -1;
        for (unisize offset = index + UNISIZE_C(1); (offset < buffer->length) && (restart == false); offset++)
        {
            const unichar non_starter = buffer->chars[offset];
            const int32_t ccc = get_ccc(non_starter);
            if (prev_ccc == 0)
            {
                break;
            }
            
            if (prev_ccc < ccc)
            {
                prev_ccc = ccc;
                const unichar precomposed_codepoint = find_composition(starter, non_starter);
                if (precomposed_codepoint != UNICORN_LARGEST_CODE_POINT)
                {
                    // Replace the non-starter with the pre-composed code point.
                    buffer->chars[index] = precomposed_codepoint;

                    // Remove the combining mark.
                    udynbuf_remove(buffer, offset);

                    // Restart iteration from the beginning of the non-starters.
                    restart = true;
                }
            }
        }
    } while (restart);
}

// Returns 'true' if the first two characters were Hangul Jamo Syllables and
// they were composed into a single composed Hangul Jamo.
static bool compose_hangul_jamo(struct UDynamicBuffer *buffer, unisize index)
{
    const unichar curr = buffer->chars[index];
    unichar next = FIRST_ILLEGAL_CODE_POINT;
    if (index < (buffer->length - 1))
    {
        next = buffer->chars[index + 1];
    }
    bool composed = false;

    // Check if the two characters are L and V.
    if (is_hangul_syllable_L(curr) && is_hangul_syllable_V(next))
    {
        // Make syllable of form LV.
        const int32_t L = (int32_t)curr;
        const int32_t V = (int32_t)next;
        const int32_t L_index = L - hangul_L_base;
        const int32_t V_index = V - hangul_V_base;
        const int32_t LV = hangul_S_base + (((L_index * hangul_V_count) + V_index) * hangul_T_count);
        buffer->chars[index] = (unichar)LV;
        composed = true;
    }
    // Check if the two characters are LV and T.
    else if (is_hangul_syllable_LV(curr) && is_hangul_syllable_T(next))
    {
        // Make syllable of form LVT.
        const int32_t LV = (int32_t)curr;
        const int32_t T = (int32_t)next;
        const int32_t T_index = T - hangul_T_base;
        buffer->chars[index] = (unichar)LV + (unichar)T_index;
        composed = true;
    }
    else
    {
        // No Action.
    }

    // If the current and next Hangul Jamo syllables were composed together, then
    // remove the next Hangul syllable. This is achieved by shifting down all the
    // characters in the buffer.
    if (composed)
    {
        assert(buffer->length >= 2); // LCOV_EXCL_BR_LINE: There must have been two syllables for this code to have been reached.
        udynbuf_remove(buffer, index + 1);
    }

    return composed;
}

// Implements the canonical composition algorithm.
// The algorithm is documented in the Unicode Standard Annex #15.
// See section 1.3: "Description of the Normalization Process" for an overview.
static void norm_compose(struct UDynamicBuffer *buffer)
{
    unisize index = 0;
    while (index < buffer->length)
    {
        if (is_hangul_syllable_L(buffer->chars[index]) ||
            is_hangul_syllable_LV(buffer->chars[index]))
        {
            if (!compose_hangul_jamo(buffer, index))
            {
                index += 1;
            }
        }
        else
        {
            compose_combining_character_sequence(buffer, index);
            index += 1;
        }
    }
}
#endif

static unisize decompose(unichar ch, unichar chars[LONGEST_UNICHAR_DECOMPOSITION])
{
    int32_t index = 0;
    int32_t length = 1;

    (void)memset(chars, 0, sizeof(unichar) * (size_t)LONGEST_UNICHAR_DECOMPOSITION);
    chars[0] = ch;

    while (index < length)
    {
        assert(index < LONGEST_UNICHAR_DECOMPOSITION); // LCOV_EXCL_BR_LINE

        unichar decomp[LONGEST_UNICHAR_DECOMPOSITION];
        const unisize decomp_length = unicorn_get_canonical_decomposition(chars[index], decomp);

        // This code point cannot be decomposed.
        // Move to the next code point.
        if (decomp_length == 0)
        {
            index += 1;
            continue;
        }

        // Move down all code points to make room for the decomposition.
        const int32_t codepoints_to_shift = length - (index + 1);
        (void)memmove(&chars[index + decomp_length], &chars[index + 1], sizeof(chars[0]) * (size_t)codepoints_to_shift);

        // Insert the decomposed code point.
        (void)memcpy(&chars[index], decomp, sizeof(chars[0]) * (size_t)decomp_length);
        length += (decomp_length - 1);
    }

    return length;
}

unistat unorm_append_run(struct UNormalizeState *state, struct unitext *it)
{
    unistat status = UNI_OK;
    unisize span_length = 0;
    unichar cp;
    struct unitext copy = *it;
    unichar decomp[LONGEST_UNICHAR_DECOMPOSITION];

    // More decomposed characters shouldn't be requested unless the buffer is empty.
    assert(unorm_is_empty(state)); // LCOV_EXCL_BR_LINE

    // Reset the buffer.
    ustate_reset(state);

    // Determine the span of characters until the next stable character.
    bool done = false;
    do
    {
        status = uni_next(copy.data, copy.length, copy.encoding, &copy.index, &cp);
        if (status != UNI_OK)
        {
            // If this is the first iteration of the loop and the iteration
            // says we've reached the end of iteration, then return zero.
            if ((status == UNI_DONE) && (span_length >= 1))
            {
                status = UNI_OK;
            }
            done = true;
        }
        else
        {
            span_length += 1;
            if (state->is_stable(cp))
            {
                done = true; // This character is stable.
            }
        }
    } while (!done);

    if (status == UNI_OK)
    {
        // Make sure the buffer is large enough to accommodate.
        status = udynbuf_reserve(&state->span, span_length);
    }

    unisize decomp_length = 0;
    if (status == UNI_OK)
    {
        // Copy over the span of characters.
        for (int32_t i = 0; i < span_length; i++)
        {
            // No need to check the return type here since the entire span of character
            // has already been scanned one (and no illegal characters were found).
            status = uni_next(it->data, it->length, it->encoding, &it->index, &state->span.chars[i]);
            assert(status == UNI_OK); // LCOV_EXCL_BR_LINE
        }

        // Calculate the length of the fully decomposed span.
        for (int32_t i = 0; i < span_length; i++)
        {
            decomp_length += decompose(state->span.chars[i], decomp);
        }

        // Resize the buffer to make room for the decomposition.
        const unisize new_capacity = state->decomp.length + decomp_length + UNISIZE_C(1);
        status = udynbuf_reserve(&state->decomp, new_capacity);
    }

    if (status == UNI_OK)
    {
        // Append the decomposed characters.
        unichar *full_decomp = &state->decomp.chars[state->decomp.length];
        for (int32_t i = 0; i < span_length; i++)
        {
            const unisize len = decompose(state->span.chars[i], decomp);
            udynbuf_append_unsafe(&state->decomp, decomp, len);
        }

        // Sort sequences of combining marks.
        unisize index = 0;
        while (index < decomp_length)
        {
            // Skip starters.
            const int32_t ccc = get_ccc(full_decomp[index]);
            if (ccc == 0)
            {
                index += 1;
                continue;
            }

            // Find the last combining mark in this range of combining marks.
            unisize end = index + 1;
            while (end != decomp_length)
            {
                if (get_ccc(state->decomp.chars[end]) == 0)
                {
                    break;
                }
                end += 1;
            }

            sort_combining_marks(&full_decomp[index], end - index);
            index = end;
        }

        state->r = decomp_length;
    }

    return status;
}

static inline unichar unorm_pop(struct UNormalizeState *state)
{
    assert(!unorm_is_empty(state)); // LCOV_EXCL_BR_LINE
    const unichar ch = state->decomp.chars[state->offset];
    state->offset += 1;
    return ch;
}

static unistat unrom_normalize_decompose(const void *src, unisize src_len, uniattr src_attr, void *dst, unisize *dst_len, uniattr dst_attr)
{
    struct UBuffer buffer = {NULL};
    unistat status = ubuffer_init(&buffer, dst, dst_len, dst_attr);

    if (status == UNI_OK)
    {
        status = unicorn_check_input_encoding(src, src_len, &src_attr);
    }

    if (status == UNI_OK)
    {
        struct UNormalizeState state;
        struct unitext it = {src, 0, src_len, src_attr};

        nstate_init(&state, &unicorn_is_stable_nfd);
        for (;;)
        {
            status = unorm_append_run(&state, &it);
            if (status != UNI_OK)
            {
                break;
            }

            ubuffer_append(&buffer, state.decomp.chars, state.decomp.length);
            ustate_reset(&state);
        }

        nstate_free(&state);
        if (status == UNI_DONE)
        {
            status = ubuffer_finalize(&buffer);
        }
    }

    return status;
}

#if defined(UNICORN_FEATURE_NFC)
static unistat unrom_normalize_compose(const void *src, unisize src_len, uniattr src_attr, void *dst, unisize *dst_len, uniattr dst_attr)
{
    struct UBuffer buffer = {NULL};
    unistat status = ubuffer_init(&buffer, dst, dst_len, dst_attr);

    if (status == UNI_OK)
    {
        status = unicorn_check_input_encoding(src, src_len, &src_attr);
    }

    if (status == UNI_OK)
    {
        struct UNormalizeState state;
        struct unitext it = {src, 0, src_len, src_attr};

        nstate_init(&state, &is_stable_nfc);
        for (;;)
        {
            status = unorm_append_run(&state, &it);
            if (status == UNI_OK)
            {
                norm_compose(&state.decomp);
            }

            if (status != UNI_OK)
            {
                break;
            }

            ubuffer_append(&buffer, state.decomp.chars, state.decomp.length);
            ustate_reset(&state);
        }

        nstate_free(&state);
        if (status == UNI_DONE)
        {
            status = ubuffer_finalize(&buffer);
        }
    }
    return status;
}
#endif

#endif

unistat uni_norm(uninormform form, const void *src, unisize src_len, uniattr src_attr, void *dst, unisize *dst_len, uniattr dst_attr)
{
    unistat status = UNI_OK;

    switch (form)
    {
    case UNI_NFC:
#if defined(UNICORN_FEATURE_NFC)
        status = unrom_normalize_compose(src, src_len, src_attr, dst, dst_len, dst_attr);
#else
        uni_message("feature disabled");
        status = UNI_FEATURE_DISABLED;
#endif
        break;

    case UNI_NFD:
#if defined(UNICORN_FEATURE_NFD)
        status = unrom_normalize_decompose(src, src_len, src_attr, dst, dst_len, dst_attr);
#else
        uni_message("feature disabled");
        status = UNI_FEATURE_DISABLED;
#endif
        break;

    default:
        uni_message("illegal normalization form");
        status = UNI_BAD_OPERATION;
        break;
    }

    return status;
}

unistat uni_normcmp(const void *s1, unisize s1_len, uniattr s1_attr, const void *s2, unisize s2_len, uniattr s2_attr, bool *result) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
#if defined(UNICORN_FEATURE_NFD)
    struct UNormalizeState ns1;
    struct UNormalizeState ns2;
    unistat status = UNI_OK;

    nstate_init(&ns1, &unicorn_is_stable_nfd);
    nstate_init(&ns2, &unicorn_is_stable_nfd);

    if (result == NULL)
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
        *result = false;

        bool mismatch = false;
        while (!mismatch)
        {
            if (unorm_is_empty(&ns1))
            {
                status = unorm_append_run(&ns1, &it1);
                if (status == UNI_DONE)
                {
                    status = UNI_OK;
                }
            }

            if (unorm_is_empty(&ns2) && (status == UNI_OK))
            {
                status = unorm_append_run(&ns2, &it2);
                if (status == UNI_DONE)
                {
                    status = UNI_OK;
                }
            }

            // If either buffer is empty or an error occurred, then stop processing.
            if ((status != UNI_OK) || unorm_is_empty(&ns1) || unorm_is_empty(&ns2))
            {
                break;
            }

            while (!unorm_is_empty(&ns1) && !unorm_is_empty(&ns2))
            {
                const unichar cp1 = unorm_pop(&ns1);
                const unichar cp2 = unorm_pop(&ns2);
                if (cp1 != cp2)
                {
                    mismatch = true;
                    break;
                }
            }
        }

        if ((status == UNI_OK) && !mismatch)
        {
            // If both buffers are empty then the strings are equal.
            if (unorm_is_empty(&ns1) && unorm_is_empty(&ns2))
            {
                *result = true;
            }
        }
    }

    nstate_free(&ns1);
    nstate_free(&ns2);
    return status;
#else
    return UNI_FEATURE_DISABLED;
#endif
}

#if defined(UNICORN_FEATURE_NFC_QUICK_CHECK) || defined(UNICORN_FEATURE_NFD_QUICK_CHECK)
static unistat quick_check(struct unitext input, QuickCheckFunc quick_check, uninormchk *result)
{
    assert(result != NULL); // LCOV_EXCL_BR_LINE
    *result = UNI_YES;

    unistat status = UNI_OK;
    unisize index = 0;
    int32_t last_ccc = 0;
    while ((*result) != UNI_NO)
    {
        unichar cp;
        status = uni_next(input.data, input.length, input.encoding, &index, &cp);
        if (status != UNI_OK)
        {
            if (status == UNI_DONE)
            {
                status = UNI_OK;
            }
            break;
        }

        // The following checks verify combining characters are sorted.
        const int32_t ccc = get_ccc(cp);
        if ((ccc != 0) && (last_ccc > ccc))
        {
            *result = UNI_NO;
        }
        else
        {
            const uint32_t qc = quick_check(cp);
            const uint32_t r = (uint32_t)(*result);
            if (r < qc)
            {
                *result = (uninormchk)qc; // cppcheck-suppress premium-misra-c-2012-10.5 ; Intentional violation to unpack data from 32-bits to an enum.
            }
        }

        last_ccc = ccc;
    }
    return status;
}
#endif

#if defined(UNICORN_FEATURE_NFC_QUICK_CHECK)
static inline uint32_t quick_check_NFC(unichar character)
{
    const uint32_t flags = (uint32_t)get_codepoint_data(character)->quick_check_flags;
    return flags >> 4u;
}
#endif

#if defined(UNICORN_FEATURE_NFD_QUICK_CHECK)
static inline uint32_t quick_check_NFD(unichar character)
{
    const uint32_t flags = (uint32_t)get_codepoint_data(character)->quick_check_flags;
    return flags & 0xFu;
}
#endif

unistat uni_normqchk(uninormform form, const void *text, unisize text_len, uniattr text_attr, uninormchk *result) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
#if defined(UNICORN_FEATURE_NFC_QUICK_CHECK) || defined(UNICORN_FEATURE_NFD_QUICK_CHECK)
    unistat status = UNI_OK;

    if (result == NULL)
    {
        uni_message("required argument 'result' is null");
        status = UNI_BAD_OPERATION;
    }

    if (status == UNI_OK)
    {
        status = unicorn_check_input_encoding(text, text_len, &text_attr);
    }

    if (status == UNI_OK)
    {
        QuickCheckFunc fn = NULL; // cppcheck-suppress misra-c2012-17.7
        switch (form)
        {
        case UNI_NFC:
#if defined(UNICORN_FEATURE_NFC_QUICK_CHECK)
            fn = &quick_check_NFC;
#else
            uni_message("cannot quick check for NFC because the feature is disabled");
            status = UNI_FEATURE_DISABLED;
#endif
            break;

        case UNI_NFD:
#if defined(UNICORN_FEATURE_NFD_QUICK_CHECK)
            fn = &quick_check_NFD;
#else
            uni_message("cannot quick check for NFD because the feature is disabled");
            status = UNI_FEATURE_DISABLED;
#endif
            break;

        default:
            uni_message("illegal normalization form");
            status = UNI_BAD_OPERATION;
            break;
        }

        if (status == UNI_OK)
        {
            struct unitext input = {text, 0, text_len, text_attr};
            status = quick_check(input, fn, result);
        }
    }

    return status;
#else
    uni_message("feature disabled");
    return UNI_FEATURE_DISABLED;
#endif
}

unistat uni_normchk(uninormform form, const void *text, unisize text_len, uniattr text_attr, bool *result) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
#if defined(UNICORN_FEATURE_NFD)
    unistat status = UNI_OK;
    IsStable is_stable = NULL; // cppcheck-suppress misra-c2012-17.7

#if defined(UNICORN_FEATURE_NFC)
    bool need_composition = false;
#endif

    if (result == NULL)
    {
        uni_message("required argument 'result' is null");
        status = UNI_BAD_OPERATION;
    }
    else if (form == UNI_NFC)
    {
#if defined(UNICORN_FEATURE_NFC)
        is_stable = &is_stable_nfc;
        need_composition = true;
#else
        uni_message("cannot check for NFC because the feature is disabled");
        status = UNI_FEATURE_DISABLED;
#endif
    }
    else if (form == UNI_NFD)
    {
        is_stable = &unicorn_is_stable_nfd;
    }
    else
    {
        uni_message("illegal normalization form");
        status = UNI_BAD_OPERATION;
    }

    if (status == UNI_OK)
    {
        status = unicorn_check_input_encoding(text, text_len, &text_attr);
    }

    if (status == UNI_OK)
    {
        struct unitext it = {text, 0, text_len, text_attr};
        struct unitext copy = it;
        struct UNormalizeState state = {0};
        bool matches = true;

        nstate_init(&state, is_stable);
        while (matches)
        {
            status = unorm_append_run(&state, &it);
            if (status != UNI_OK)
            {
                if (status == UNI_DONE)
                {
                    status = UNI_OK;
                }
                break;
            }

#if defined(UNICORN_FEATURE_NFC)
            if (need_composition)
            {
                norm_compose(&state.decomp);
            }
#endif

            while (!unorm_is_empty(&state))
            {
                unichar cp = FIRST_ILLEGAL_CODE_POINT;
                status = uni_next(copy.data, copy.length, copy.encoding, &copy.index, &cp);

                // The normalization append run routine already checks the text for malformed characters.
                // Under no circumstances should a malformed character be detected at this point.
                assert(status == UNI_OK); // LCOV_EXCL_BR_LINE

                if (cp != unorm_pop(&state))
                {
                    matches = false;
                    break;
                }
            }
        }

        if (status == UNI_OK)
        {
            *result = matches;
        }

        nstate_free(&state);
    }

    return status;
#else
    uni_message("feature disabled");
    return UNI_FEATURE_DISABLED;
#endif
}
