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

#include "ducet.h"

#if defined(UNICORN_FEATURE_COLLATION)

static const int32_t UCOL_INVALID = -2;
static const int32_t UCOL_LESS = -1;
static const int32_t UCOL_EQUAL = 0;
static const int32_t UCOL_GREATER = 1;

struct SortKey
{
    struct CEDecoder ce_decoder;

    uniweighting weighting;
    int32_t strength;
    int32_t level;
    bool is_prev_variable;

    int32_t weights_index;
    int32_t weights_count;

    // Each collation element is stored in the DUCET with three weights therefore the maximum
    // number of individual weights will be the longest collation element sequence multiplied
    // by three. An additional plus three is added to account for the level separators when
    // the strength is quaternary.
    uint16_t weights[(UNICORN_MAX_COLLATION * 3) + 3];
};

static void sortkeybuf_init(struct SortKey *state, uniweighting weighting, unistrength strength)
{
    cebuf_init(&state->ce_decoder);
    state->weights_index = 0;
    state->weights_count = 0;
    state->is_prev_variable = false;
    state->level = 0;
    state->strength = (int32_t)strength;
    state->weighting = weighting;
}

static void sortkeybuf_free(struct SortKey *state)
{
    assert(state != NULL); // LCOV_EXCL_BR_LINE
    cebuf_free(&state->ce_decoder);
}

static unistat sortkeybuf_append(struct SortKey *state, uint16_t weight)
{
    unistat status;
    const int32_t new_weight_count = state->weights_count + 1;
    if ((size_t)new_weight_count > COUNT_OF(state->weights)) // LCOV_EXCL_BR_LINE: It is impossible to branch test this malfunction.
    {
        status = UNI_MALFUNCTION; // LCOV_EXCL_LINE
    }
    else
    {
        state->weights[state->weights_count] = weight;
        state->weights_count = new_weight_count;
        status = UNI_OK;
    }
    return status;
}

static bool sortkeybuf_is_empty(const struct SortKey *state)
{
    return state->weights_index == state->weights_count;
}

static inline uint16_t sortkeybuf_pop(struct SortKey *state)
{
    assert(!sortkeybuf_is_empty(state)); // LCOV_EXCL_BR_LINE
    const uint16_t weight = state->weights[state->weights_index];
    state->weights_index += 1;
    return weight;
}

static unistat sortkeybuf_append_run(struct SortKey *state, struct unitext *text)
{
    // LCOV_EXCL_START
    assert(state != NULL);
    assert(text != NULL);
    assert(sortkeybuf_is_empty(state));
    // LCOV_EXCL_STOP

    unistat status = UNI_OK;
    struct CEDecoder *decoder = &state->ce_decoder;

    // Reset state.
    state->weights_index = 0;
    state->weights_count = 0;

    // The sort key might be empty if the collation elements are ignorable.
    // In this case, no weights are appenede because they would all be zero.
    while ((status == UNI_OK) && sortkeybuf_is_empty(state) && (state->level < state->strength))
    {
        // Queue up a run of collation elements.
        status = cebuf_append_run(decoder, text);
        if (status == UNI_OK)
        {
            // If there are no more collation elements, then the text has been completely iterated.
            // If there are more collation element levels to process, then restart iteration from
            // the beginning of the text.
            if (cebuf_is_empty(decoder))
            {
                // Rewind the text to the beginning.
                text->index = 0;

                // Advance to next collation element weight level.
                state->level += 1;
                state->is_prev_variable = false;

                // Append a level separator (zero).
                // This is guaranteed to be lower than any weight in the resulting sort key.
                if (state->level < state->strength)
                {
                    status = sortkeybuf_append(state, 0);
                    continue;
                }
            }
        }

        while (status == UNI_OK)
        {
            // The following branch was moved inside the loop to avoid persistent side effects
            // in compliance with MISRA C:2012.
            if (cebuf_is_empty(decoder))
            {
                break;
            }

            const CE ce = cebuf_pop(decoder);
            uint16_t ce_weights[4] = {
                ce.primary,
                ce.secondary,
                ce.tertiary,
                (uint16_t)0x0,
            };

            // Shifts variable collation elements using the "shifted" option described here:
            // https://unicode.org/reports/tr10/#Variable_Weighting
            if (state->weighting == UNI_SHIFTED)
            {
                const uint16_t zero = (uint16_t)0;
                const uint16_t max = (uint16_t)0xFFFF;
                const uint16_t L1 = ce_weights[0];
                const uint16_t L2 = ce_weights[1];
                const uint16_t L3 = ce_weights[2];
                bool is_variable = (L1 >= MIN_VARIABLE_WEIGHT) && (L1 <= MAX_VARIABLE_WEIGHT);

                // The following checks are semantically sound, however, not all branches can be
                // taken because the DUCET, provided by the Unicode consortium, does not define
                // data that would allow all branches to be taken. So while the following checks
                // conform to Unicode Technical Report #10, not all combinations can be tested
                // and therefore branch coverage is ignored for these lines. Conformance is
                // verify against the official and unofficial collation test suite.

                if ((L1 == zero) && (L2 == zero) && (L3 == zero)) // LCOV_EXCL_BR_LINE
                {
                    ce_weights[3] = zero;
                }
                else if ((L1 == zero) && (L3 != zero) && state->is_prev_variable) // LCOV_EXCL_BR_LINE
                {
                    ce_weights[0] = zero;
                    ce_weights[1] = zero;
                    ce_weights[2] = zero;
                    ce_weights[3] = zero;
                    is_variable = true;
                }
                else if ((L1 != zero) && is_variable)
                {
                    ce_weights[0] = zero;
                    ce_weights[1] = zero;
                    ce_weights[2] = zero;
                    ce_weights[3] = L1;
                }
                else if ((L1 == zero) && (L3 != zero) && !state->is_prev_variable) // LCOV_EXCL_BR_LINE
                {
                    ce_weights[3] = max;
                }
                else if ((L1 != zero) && !is_variable) // LCOV_EXCL_BR_LINE
                {
                    ce_weights[3] = max;
                }
                else
                {
                    // No Action.
                }

                state->is_prev_variable = is_variable;
            }

            // Only append non-zero weights.
            const uint16_t weight = ce_weights[state->level];
            if (weight > (uint16_t)0)
            {
                status = sortkeybuf_append(state, weight);
            }
        }
    }

    return status;
}

#endif

UNICORN_API unistat uni_sortkeymk(const void *text, unisize text_len, uniattr text_attr, uniweighting weighting, unistrength strength, uint16_t *sortkey, size_t *sortkey_cap) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
#if defined(UNICORN_FEATURE_COLLATION)
    unistat status = UNI_OK;
    struct SortKey skbuf = {0};
    sortkeybuf_init(&skbuf, weighting, strength);

    // Do the null check here to simplify the 'else' body below.
    if (sortkey_cap == NULL)
    {
        uni_message("output buffer capacity is null");
        status = UNI_BAD_OPERATION;
    }
    else
    {
        // The parameters for the destination buffer are identical to character buffers.
        // Reuse the existing error checking logic rather than repeating it here.
        unisize unused1 = (*sortkey_cap > (size_t)0) ? UNISIZE_C(1) : UNISIZE_C(0);
        uniattr unused2 = UNI_SCALAR;
        status = unicorn_check_output_encoding(sortkey, &unused1, &unused2);
    }

    if (status == UNI_OK)
    {
        if ((strength < UNI_PRIMARY) || (strength > UNI_QUATERNARY))
        {
            uni_message("illegal strength");
            status = UNI_BAD_OPERATION;
        }
    }

    if (status == UNI_OK)
    {
        status = unicorn_check_input_encoding(text, text_len, &text_attr);
    }

    if (status == UNI_OK)
    {
        size_t sortkey_len = 0;
        struct unitext it = {text, 0, text_len, text_attr};
        for (;;)
        {
            // Queue up a batch of sort key weights.
            status = sortkeybuf_append_run(&skbuf, &it);

            // If either buffer is empty or an error occurred, then stop processing.
            if ((status != UNI_OK) || sortkeybuf_is_empty(&skbuf))
            {
                break;
            }

            // Copy sort key weights to the destination buffer.
            while (!sortkeybuf_is_empty(&skbuf))
            {
                const uint16_t weight = sortkeybuf_pop(&skbuf);
                if (sortkey_len < *sortkey_cap)
                {
                    sortkey[sortkey_len] = weight;
                }
                sortkey_len += (size_t)1;
            }
        }

        if (sortkey_len > *sortkey_cap)
        {
            status = UNI_NO_SPACE;
        }
        *sortkey_cap = sortkey_len;
    }

    sortkeybuf_free(&skbuf);
    return status;
#else
    uni_message("collation feature disabled");
    return UNI_FEATURE_DISABLED;
#endif
}

UNICORN_API unistat uni_collate(const void *s1, unisize s1_len, uniattr s1_attr, const void *s2, unisize s2_len, uniattr s2_attr, uniweighting weighting, unistrength strength, int32_t *result) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
#if defined(UNICORN_FEATURE_COLLATION)
    unistat status = UNI_OK;
    const int32_t levels = (int32_t)strength;
    struct SortKey sk1;
    struct SortKey sk2;

    sortkeybuf_init(&sk1, weighting, strength);
    sortkeybuf_init(&sk2, weighting, strength);

    if (result == NULL)
    {
        uni_message("required argument 'result' is null");
        status = UNI_BAD_OPERATION;
    }
    else if ((levels < 1) || (levels > 4))
    {
        uni_message("illegal strength");
        status = UNI_BAD_OPERATION;
    }
    else
    {
        // No Action.
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
        int32_t relation = UCOL_INVALID;

        while (relation == UCOL_INVALID)
        {
            if (sortkeybuf_is_empty(&sk1))
            {
                status = sortkeybuf_append_run(&sk1, &it1);
            }

            if (sortkeybuf_is_empty(&sk2) && (status == UNI_OK))
            {
                status = sortkeybuf_append_run(&sk2, &it2);
            }

            // If either buffer is empty or an error occurred, then stop processing.
            if ((status != UNI_OK) || sortkeybuf_is_empty(&sk1) || sortkeybuf_is_empty(&sk2))
            {
                break;
            }

            while (!sortkeybuf_is_empty(&sk1) && !sortkeybuf_is_empty(&sk2))
            {
                const uint16_t w1 = sortkeybuf_pop(&sk1);
                const uint16_t w2 = sortkeybuf_pop(&sk2);

                // A < B
                if (w1 < w2)
                {
                    relation = UCOL_LESS;
                }
                // A > B
                else if (w1 > w2)
                {
                    relation = UCOL_GREATER;
                }
                else
                {
                    // No Action.
                }

                if (relation != UCOL_INVALID)
                {
                    break;
                }
            }
        }

        // If everything is OK, but all weights are equal then use the
        // length of the sort keys as the tie-breaker.
        if ((status == UNI_OK) && (relation == UCOL_INVALID))
        {
            // A = B
            if (sortkeybuf_is_empty(&sk1) && sortkeybuf_is_empty(&sk2))
            {
                relation = UCOL_EQUAL;
            }
            // len(A) > len(B)
            else if (sortkeybuf_is_empty(&sk1))
            {
                relation = UCOL_LESS;
            }
            // len(A) < len(B)
            else
            {
                relation = UCOL_GREATER;
            }
        }

        *result = relation;
    }

    sortkeybuf_free(&sk1);
    sortkeybuf_free(&sk2);
    return status;
#else
    uni_message("collation feature disabled");
    return UNI_FEATURE_DISABLED;
#endif
}

UNICORN_API unistat uni_sortkeycmp(const uint16_t *sk1, size_t sk1_len, const uint16_t *sk2, size_t sk2_len, int32_t *result) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
#if defined(UNICORN_FEATURE_COLLATION)
    unistat status = UNI_OK;

    if (sk1 == NULL)
    {
        uni_message("required argument 'sortkey1' is null");
        status = UNI_BAD_OPERATION;
    }
    else if (sk2 == NULL)
    {
        uni_message("required argument 'sortkey2' is null");
        status = UNI_BAD_OPERATION;
    }
    else if (result == NULL)
    {
        uni_message("required argument 'result' is null");
        status = UNI_BAD_OPERATION;
    }
    else
    {
        int32_t relation = UCOL_INVALID;
        size_t sort_key_length;

        if (sk1_len < sk2_len)
        {
            sort_key_length = sk1_len;
        }
        else
        {
            sort_key_length = sk2_len;
        }

        for (size_t i = 0; (i < sort_key_length) && (relation == UCOL_INVALID); i++)
        {
            // A < B
            if (sk1[i] < sk2[i])
            {
                relation = UCOL_LESS;
            }
            // A > B
            else if (sk1[i] > sk2[i])
            {
                relation = UCOL_GREATER;
            }
            else
            {
                // No Action.
            }
        }

        if (relation == UCOL_INVALID)
        {
            // len(A) < len(B)
            if (sk1_len < sk2_len)
            {
                relation = UCOL_LESS;
            }
            // len(A) > len(B)
            else if (sk1_len > sk2_len)
            {
                relation = UCOL_GREATER;
            }
            // A = B
            else
            {
                relation = UCOL_EQUAL;
            }
        }

        *result = relation;
    }

    return status;
#else
    uni_message("feature disabled");
    return UNI_FEATURE_DISABLED;
#endif
}
