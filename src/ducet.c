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

static int32_t decode_collation_element_sequence(const uint32_t weight_table_index, CE collation_elements[UNICORN_MAX_COLLATION])
{
    assert(weight_table_index > 0u); // LCOV_EXCL_BR_LINE
    const uint32_t *collation_element = &unicorn_collation_mappings[weight_table_index];
    int32_t count = 0;
    for (;;)
    {
        assert(count < UNICORN_MAX_COLLATION); // LCOV_EXCL_BR_LINE
        
        const uint32_t ce = *collation_element;
        const uint32_t weights = ce & ~CONTINUATION_FLAG;
        CE *element = &collation_elements[count];
        element->primary = (uint16_t)((weights >> 15u) & 0xFFFFu);
        element->secondary = (uint16_t)((weights >> 6u) & 0x1FFu);
        element->tertiary = (uint16_t)(weights & 0x1Fu);
        count += 1;

        if ((ce & CONTINUATION_FLAG) == 0u)
        {
            break; // The end of the collation element array was reached.
        }
        collation_element = &collation_element[1];
    }
    return count;
}

static const struct CollationNode *find_child_collation_node(const struct CollationNode nodes[], int32_t left, int32_t right, unichar codepoint)
{
    const struct CollationNode *node = NULL;
    if (right >= left)
    {
        const int32_t middle = left + ((right - left) / 2);
        node = &nodes[middle];
        const unichar cp = GET_NODE_CODE_POINT(node);
        if (cp != codepoint)
        {
            // Check the left or right subarray.
            if (cp> codepoint)
            {
                node = find_child_collation_node(nodes, left, middle - 1, codepoint); // cppcheck-suppress misra-c2012-17.2
            }
            else
            {
                node = find_child_collation_node(nodes, middle + 1, right, codepoint); // cppcheck-suppress misra-c2012-17.2
            }
        }
    }
    return node;
}

static void collect_collation_elements(struct UDynamicBuffer *charbuf, struct CEDecoder *state)
{
    // Consume the initial starter character.
    const unichar character = charbuf->chars[0];
    udynbuf_remove(charbuf, 0);

    const uint32_t collation_mapping_offset = uni_get_collation_data(character)->value;
    if (collation_mapping_offset == 0u)
    {
        // The variable names 'AAAA' and 'BBBB' are the names used by the Unicode Standard to describe
        // the two collation elements that are algorithmically constructed for various scripts.
        // See tr10, 10.1.3 Implicit Weights, Table 16.
        CE *const aaaa = &state->ces[state->ces_count];
        CE *const bbbb = &state->ces[state->ces_count + 1];
        state->ces_count += 2;
        switch (get_codepoint_data(character)->collation_subtype)
        {
        case COLLATION_TANGUT:
            aaaa->primary = (uint16_t)0xFB00;
            aaaa->secondary = (uint16_t)0x20;
            aaaa->tertiary = (uint16_t)0x2;
            bbbb->primary = (uint16_t)((character - UNICHAR_C(0x17000)) | UNICHAR_C(0x8000));
            break;
        case COLLATION_TANGUT_COMPONENTS:
            aaaa->primary = (uint16_t)0xFB01;
            aaaa->secondary = (uint16_t)0x20;
            aaaa->tertiary = (uint16_t)0x2;
            bbbb->primary = (uint16_t)((character - UNICHAR_C(0x17000)) | UNICHAR_C(0x8000));
            break;
        case COLLATION_NUSHU:
            aaaa->primary = (uint16_t)0xFB02;
            aaaa->secondary = (uint16_t)0x20;
            aaaa->tertiary = (uint16_t)0x2;
            bbbb->primary = (uint16_t)((character - UNICHAR_C(0x1B170)) | UNICHAR_C(0x8000));
            break;
        case COLLATION_KHITAN:
            aaaa->primary = (uint16_t)0xFB03;
            aaaa->secondary = (uint16_t)0x20;
            aaaa->tertiary = (uint16_t)0x2;
            bbbb->primary = (uint16_t)((character - UNICHAR_C(0x18B00)) | UNICHAR_C(0x8000));
            break;
        case COLLATION_CORE_HAN:
            aaaa->primary = (uint16_t)(UNICHAR_C(0xFB40) + (character >> UNICHAR_C(15)));
            aaaa->secondary = (uint16_t)0x20;
            aaaa->tertiary = (uint16_t)0x2;
            bbbb->primary = (uint16_t)((character & UNICHAR_C(0x7FFF)) | UNICHAR_C(0x8000));
            break;
        case COLLATION_OTHER_HAN:
            aaaa->primary = (uint16_t)(UNICHAR_C(0xFB80) + (character >> UNICHAR_C(15)));
            aaaa->secondary = (uint16_t)0x20;
            aaaa->tertiary = (uint16_t)0x2;
            bbbb->primary = (uint16_t)((character & UNICHAR_C(0x7FFF)) | UNICHAR_C(0x8000));
            break;
        default:
            // This code point is unassigned.
            // Deallocate the reserved collation elements.
            state->ces_count -= 2;
            break;
        }
    }
    else if ((collation_mapping_offset & INLINE_CE_FLAG) == INLINE_CE_FLAG)
    {
        CE *element = &state->ces[state->ces_count];
        const uint32_t weights = collation_mapping_offset & ~INLINE_CE_FLAG;
        element->primary = (uint16_t)((weights >> 15u) & 0xFFFFu);
        element->secondary = (uint16_t)((weights >> 6u) & 0x1FFu);
        element->tertiary = (uint16_t)(weights & 0x1Fu);
        state->ces_count += 1;
    }
    else if ((collation_mapping_offset & IN_WEIGHT_TABLE_FLAG) == IN_WEIGHT_TABLE_FLAG)
    {
        const uint32_t weight_table_index = collation_mapping_offset & ~IN_WEIGHT_TABLE_FLAG;
        state->ces_count = decode_collation_element_sequence(weight_table_index, state->ces);
    }
    else
    {
        assert((collation_mapping_offset & IN_TRIE_FLAG) == IN_TRIE_FLAG); // LCOV_EXCL_BR_LINE

        const struct CollationNode *node = &unicorn_collation_mappings_trie[collation_mapping_offset & ~IN_TRIE_FLAG];
        const struct CollationNode *longest_match = NULL;
        int32_t longest_match_depth = 0;
        assert(GET_NODE_CODE_POINT(node) == character); // LCOV_EXCL_BR_LINE

        // Check if the root node maps to a collation element.
        // If it does, then our "match" has started.
        if (UNICORN_IN_COLLATION_TABLE(node)) // LCOV_EXCL_BR_LINE: As of Unicode 16.0 this path is not taken.
        {
            longest_match = node;
        }

        // Consume the longest substring that has a match in the collation element table.
        for (unisize i = 0; i < udynbuf_length(charbuf); i++)
        {
            // Check if the code point is present in a sequence in the collation table.
            const uint32_t unsigned_child_count = GET_NODE_CHILD_COUNT(node);
            const int32_t child_count = (int32_t)unsigned_child_count;
            const struct CollationNode *child = find_child_collation_node(&unicorn_collation_mappings_trie[node->child_offset], 0, child_count - 1, charbuf->chars[i]);
            if (child == NULL)
            {
                break;
            }

            // Remember if this sequence of code points has a match in the collation table.    
            if (UNICORN_IN_COLLATION_TABLE(child))
            {
                longest_match = child;
                longest_match_depth = i + 1;
            }

            // Continue trying to expand the sequence.
            node = child;
        }

        // The depth reached into the trie is equal to the number of leading characters to remove.
        while (longest_match_depth > 0)
        {
            udynbuf_remove(charbuf, 0);
            longest_match_depth -= 1;
        }

        // Find any unblocked non-starters with a match in the trie.
        int32_t prev_ccc = 0;
        int32_t index = 0;
        while (index < udynbuf_length(charbuf))
        {
            // Stop processing if a non-starter was found.
            const unichar codepoint = charbuf->chars[index];
            const int32_t ccc = (int32_t)get_codepoint_data(codepoint)->canonical_combining_class;
            if ((ccc == 0) || (prev_ccc >= ccc))
            {
                break;
            }

            // Updated the previous CCC for next time.
            prev_ccc = ccc;

            // Check if the string + non-starter is present in the collation mappings table.
            const uint32_t unsigned_child_count = GET_NODE_CHILD_COUNT(node);
            const int32_t child_count = (int32_t)unsigned_child_count;
            const struct CollationNode *child = find_child_collation_node(&unicorn_collation_mappings_trie[node->child_offset], 0, child_count - 1, codepoint);
            if (child == NULL)
            {
                index += 1;
                continue;
            }

            // Remember if this sequence of code points has a match in the collation table.
            if (UNICORN_IN_COLLATION_TABLE(child))
            {
                // Found a non-starter with a correlation mapping.
                longest_match = child;
                node = child;

                // Shift down the entire string, removing the non-stater.
                udynbuf_remove(charbuf, index);
            }
            else
            {
                index += 1;
            }
        }

        if (longest_match != NULL) // LCOV_EXCL_BR_LINE
        {
            assert(UNICORN_IN_COLLATION_TABLE(longest_match)); // LCOV_EXCL_BR_LINE
            state->ces_count = decode_collation_element_sequence(longest_match->collation_mappings_offset, state->ces);
        }
    }

    // If no collation elements were accumulated, then an unknown/unassigned code point was encountered.
    // See tr10, 10.1.3 Implicit Weights, Table 16.
    if (state->ces_count == 0)
    {
        CE *aaaa = &state->ces[state->ces_count];
        CE *bbbb = &state->ces[state->ces_count + 1];
        aaaa->primary = (uint16_t)(UNICHAR_C(0xFBC0) + (character >> UNICHAR_C(15)));
        aaaa->secondary = (uint16_t)0x20;
        aaaa->tertiary = (uint16_t)0x2;
        bbbb->primary = (uint16_t)((character & UNICHAR_C(0x7FFF)) | UNICHAR_C(0x8000));
        state->ces_count += 2;
    }
}

void cebuf_init(struct CEDecoder *state)
{
    (void)memset(state, 0, sizeof(state[0]));
    nstate_init(&state->normbuf, &unicorn_is_stable_nfd);
    udynbuf_init(&state->charbuf);
}

void cebuf_free(struct CEDecoder *state)
{
    assert(state != NULL); // LCOV_EXCL_BR_LINE
    nstate_free(&state->normbuf);
    udynbuf_free(&state->charbuf);
}

bool cebuf_is_empty(const struct CEDecoder *state)
{
    return state->ces_index == state->ces_count;
}

CE cebuf_pop(struct CEDecoder *state)
{
    assert(!cebuf_is_empty(state)); // LCOV_EXCL_BR_LINE
    const CE ce = state->ces[state->ces_index];
    state->ces_index += 1;
    return ce;
}

unistat cebuf_append_run(struct CEDecoder *state, struct unitext *text)
{
    // LCOV_EXCL_START
    assert(state != NULL);
    assert(text != NULL);
    assert(cebuf_is_empty(state));
    // LCOV_EXCL_STOP

    unistat status = UNI_OK;
    struct UDynamicBuffer *charbuf = &state->charbuf;
    struct UNormalizeState *normbuf = &state->normbuf;

    // Reset the state.
    state->ces_count = 0;
    state->ces_index = 0;

    // Normalize a run of characters.
    while ((status == UNI_OK) && (udynbuf_length(charbuf) < LONGEST_INITIAL_SUBSTRING))
    {
        status = unorm_append_run(normbuf, text);
        if (status != UNI_OK)
        {
            if (status == UNI_DONE)
            {
                status = UNI_OK;
            }
            break;
        }
        status = udynbuf_append(charbuf, normbuf->decomp.chars, normbuf->decomp.length);
        ustate_reset(normbuf);
    }

    // Find the collation elements for the run of normalized characters.
    if ((status == UNI_OK) && (udynbuf_length(charbuf) > 0))
    {
        collect_collation_elements(charbuf, state);
    }

    return status;
}

#endif
