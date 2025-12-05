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

#include "unidata.h"
#include "common.h"

#if defined(UNICORN_FEATURE_SEGMENTATION)

typedef uint32_t UOpcode;

#define DECODE_OP(OPCODE) ((OPCODE) & (UOpcode)0xFF)

#define DECODE_I_TYPE(OPCODE, IM) \
    do { \
        const UOpcode tmp_op = (OPCODE); \
        (IM) = (UOpcode)tmp_op >> (UOpcode)8; \
    } while (false)

#define DECODE_R_TYPE(OPCODE, RS, RD) \
    do { \
        const UOpcode tmp_op = (OPCODE); \
        (RS) = tmp_op >> (UOpcode)16; \
        (RD) = (tmp_op >> (UOpcode)8) & (UOpcode)0xFF; \
    } while (false)

struct RuleState
{
    struct RuleState *next;
    int32_t ip;
};

struct RuleStateList
{
    struct RuleState *head;
    struct RuleState *tail;
    uint64_t pending; // Based on number of OPCODES!
};

struct MatchContext
{
    const uint32_t *opcodes;
    struct RuleState *free_list;
    struct RuleStateList current_states;
    struct RuleStateList next_states;
    struct RuleState states[MAX_BREAK_STATES];
};

static inline struct RuleState *state_alloc(struct MatchContext *ctx)
{
    // LCOV_EXCL_START
    assert(ctx != NULL);
    assert(ctx->free_list != NULL);
    // LCOV_EXCL_STOP
    struct RuleState *state = ctx->free_list;
    ctx->free_list = state->next;
    (void)memset(state, 0, sizeof(state[0]));
    return state;
}

static inline void state_free(struct MatchContext *ctx, struct RuleState *state)
{
    // LCOV_EXCL_START
    assert(ctx != NULL);
    assert(state != NULL);
    // LCOV_EXCL_STOP
    state->next = ctx->free_list;
    ctx->free_list = state;
}

static bool at_end(struct unitext text)
{
    unichar cp;
    return uni_next(text.data, text.length, text.encoding, &text.index, &cp) == UNI_DONE;
}

static void add_state(struct MatchContext *ctx, struct RuleStateList *list, int32_t ip, struct unitext *text)
{
    struct RuleState *rule_state = NULL;
    UOpcode rs = 0;
    UOpcode rd = 0;

    const uint64_t bit = UINT64_C(1) << (uint64_t)ip;
    if ((list->pending & bit) != bit)
    {
        list->pending |= bit;
        const UOpcode pcode = ctx->opcodes[ip];
        switch (DECODE_OP(pcode)) // LCOV_EXCL_BR_LINE: The default case is unreachable.
        {
        case OP_SOT:
            if (text->index <= 0)
            {
                add_state(ctx, list, ip + 1, text); // cppcheck-suppress misra-c2012-17.2
            }
            break;

        case OP_EOT:
            if (at_end(*text))
            {
                add_state(ctx, list, ip + 1, text); // cppcheck-suppress misra-c2012-17.2
            }
            break;

        case OP_JMP:
            DECODE_I_TYPE(pcode, rs);
            add_state(ctx, list, (int32_t)rs, text); // cppcheck-suppress misra-c2012-17.2
            break;

        case OP_FORK:
            DECODE_R_TYPE(pcode, rs, rd);
            add_state(ctx, list, (int32_t)rs, text); // cppcheck-suppress misra-c2012-17.2
            add_state(ctx, list, (int32_t)rd, text); // cppcheck-suppress misra-c2012-17.2
            break;

        case OP_NOP:
        case OP_ANY:
        case OP_CHAR:
        case OP_NOTCHAR:
        case OP_CHAR_CLASS:
        case OP_INV_CHAR_CLASS:
        case OP_HALT:
            rule_state = state_alloc(ctx);
            rule_state->ip = ip;
            if (list->head == NULL)
            {
                list->head = rule_state;
                list->tail = rule_state;
            }
            else
            {
                list->tail->next = rule_state;
                list->tail = rule_state;
            }
            break;

        // LCOV_EXCL_START
        default:
            UNREACHABLE; // cppcheck-suppress premium-misra-c-2012-17.3
            break;
        // LCOV_EXCL_STOP
        }
    }
}

static bool match_character_class(unibreak type, unichar cp, UOpcode klass)
{
    bool match = false;
    const struct CodepointData *data = unicorn_get_codepoint_data(cp);
    
    if (type == UNI_GRAPHEME)
    {
#if defined(UNICORN_FEATURE_GCB)
        if (((UOpcode)data->gcb == klass) || ((UOpcode)data->incb == klass))
        {
            match = true;
        }
#endif
    }
    else if (type == UNI_WORD)
    {
#if defined(UNICORN_FEATURE_WB)
        if (((UOpcode)data->wb == klass) || ((UOpcode)data->wbx == klass))
        {
            match = true;
        }
#endif
    }
    else
    {
        assert(type == UNI_SENTENCE); // LCOV_EXCL_BR_LINE
#if defined(UNICORN_FEATURE_SB)
        if ((UOpcode)data->sb == klass)
        {
            match = true;
        }
#endif
    }
    return match;
}

static void prepare(struct MatchContext *ctx)
{
    for (int32_t i = 0; i < (MAX_BREAK_STATES - 1); i++)
    {
        ctx->states[i].next = &ctx->states[i + 1];
    }
    ctx->free_list = ctx->states;
}

static unistat match(unibreak type, const uint32_t *opcodes, const uint32_t *data, struct unitext text, int32_t direction, bool *is_match)
{
    // LCOV_EXCL_START
    assert(opcodes != NULL);
    assert(data != NULL);
    assert(is_match != NULL);
    assert((direction == 1) || (direction == -1));
    assert(MAX_BREAK_STATES > 0);
    // LCOV_EXCL_STOP

    *is_match = false;
    bool overflow = false;
    unistat status = UNI_OK;

    struct MatchContext ctx = {.opcodes = opcodes};
    prepare(&ctx);

    struct RuleStateList *current_states = &ctx.current_states;
    struct RuleStateList *next_states = &ctx.next_states;
    add_state(&ctx, current_states, 0, &text); // Allocate initial states.

    while (current_states->head != NULL)
    {
        unichar cp = UNICORN_LARGEST_CODE_POINT;
        if (direction > 0)
        {
            status = uni_next(text.data, text.length, text.encoding, &text.index, &cp);
            if (status == UNI_DONE)
            {
                overflow = true;
                status = UNI_OK;
            }
        }
        else
        {
            status = uni_prev(text.data, text.length, text.encoding, &text.index, &cp);
            if (status == UNI_DONE)
            {
                overflow = true;
                status = UNI_OK;
            }
        }

        while (current_states->head != NULL)
        {
            // Grab the newest state from the queue.
            struct RuleState *next = current_states->head->next;
            const int32_t ip = current_states->head->ip;
            state_free(&ctx, current_states->head);
            current_states->head = next;
            current_states->pending &= ~(UINT64_C(1) << (uint64_t)ip);

            UOpcode rs = 0;
            UOpcode rd = 0;
            UOpcode im = 0;
            const UOpcode pcode = opcodes[ip];
            switch (DECODE_OP(pcode)) // LCOV_EXCL_BR_LINE: The default case is unreachable.
            {
            case OP_CHAR:
                DECODE_I_TYPE(pcode, im);
                if (!overflow)
                {
                    if (match_character_class(type, cp, im))
                    {
                        add_state(&ctx, next_states, ip + 1, &text);
                    }
                }
                break;

            case OP_NOTCHAR:
                DECODE_I_TYPE(pcode, im);
                if (!overflow)
                {
                    if (!match_character_class(type, cp, im))
                    {
                        add_state(&ctx, next_states, ip + 1, &text);
                    }
                }
                break;

            case OP_CHAR_CLASS:
                if (!overflow)
                {
                    DECODE_R_TYPE(pcode, rs, rd);
                    bool class_match = false;
                    for (UOpcode i = rs; i < rd; i++)
                    {
                        if (match_character_class(type, cp, data[i]))
                        {
                            class_match = true;
                            break;
                        }
                    }
                    if (class_match)
                    {
                        add_state(&ctx, next_states, ip + 1, &text);
                    }
                }
                break;

            case OP_INV_CHAR_CLASS:
                if (!overflow)
                {
                    DECODE_R_TYPE(pcode, rs, rd);
                    bool class_match = true;
                    for (UOpcode i = rs; i < rd; i++)
                    {
                        if (match_character_class(type, cp, data[i]))
                        {
                            class_match = false;
                            break;
                        }
                    }
                    if (class_match)
                    {
                        add_state(&ctx, next_states, ip + 1, &text);
                    }
                }
                break;

            case OP_ANY:
                // All segmentation rules are written in a way such that this condition will alaways be true,
                // however, to guard against future assumptions, we'll leave the check in place, even though
                // it cannot be tested right now.
                if (!overflow) // LCOV_EXCL_BR_LINE
                {
                    add_state(&ctx, next_states, ip + 1, &text);
                }
                break;

            case OP_HALT: {
                *is_match = true;
                current_states->head = NULL;
                current_states->tail = NULL;
                break;
            }

            // LCOV_EXCL_START
            default:
                UNREACHABLE; // cppcheck-suppress premium-misra-c-2012-17.3
                break;
            // LCOV_EXCL_STOP
            }
        }

        if ((*is_match) || (status != UNI_OK))
        {
            break;
        }

        struct RuleStateList *tmp = current_states;
        current_states = next_states;
        next_states = tmp;
        (void)memset(next_states, 0, sizeof(next_states[0]));
    }

    return status;
}

static unistat match_rule(unibreak type, const struct BreakRule *rule, const uint32_t *data, const struct unitext *it, bool *is_match)
{
    unistat status = match(type, rule->left, data, *it, -1, is_match);
    if ((status == UNI_OK) && *is_match)
    {
        status = match(type, rule->right, data, *it, 1, is_match);
    }
    return status;
}

static unistat next_break(unibreak type, const struct Segmentation *seg, const void *text, unisize length, uniattr encoding, unisize *index)
{
    unistat status = UNI_OK;

    if (index == NULL)
    {
        uni_message("required argument is null");
        status = UNI_BAD_OPERATION;
    }
    else
    {
        status = uni_check_input_encoding(text, length, &encoding);
    }

    if (status == UNI_OK)
    {
        unichar cp;
        struct unitext it = {text, *index, length, encoding};
        status = uni_next(it.data, it.length, it.encoding, &it.index, &cp);
        while (status == UNI_OK)
        {
            const struct BreakRule *rule = NULL;
            bool found_match = false;
            for (int32_t i = 0; i < seg->rules_count; i++) // LCOV_EXCL_BR_LINE: There is always a matching rule (therefore this is never false).
            {
                rule = &seg->rules[i];
                status = match_rule(type, rule, seg->data, &it, &found_match);
                if ((status != UNI_OK) || found_match)
                {
                    break;
                }
            }

            // All segmentation types have a wildcard match rule therefore there
            // must always be a matching rule. This check is left in place to
            // guard against future assumptions.
             // LCOV_EXCL_START
            if (rule == NULL)
            {
                status = UNI_MALFUNCTION;
            }
            // LCOV_EXCL_STOP

            if (status == UNI_OK)
            {
                assert(found_match); // LCOV_EXCL_BR_LINE
                if (!rule->mandatory_break)
                {
                    status = uni_next(text, length, encoding, &it.index, &cp);
                }
            }

            if ((status != UNI_OK) || rule->mandatory_break)
            {
                break;
            }
        }

        if (status == UNI_OK)
        {
            *index = it.index;
        }
    }

    return status;
}

static unistat previous_break(unibreak type, const struct Segmentation *seg, const void *text, unisize length, uniattr encoding, unisize *index)
{
    unistat status = UNI_OK;

    if (index == NULL)
    {
        uni_message("required argument is null");
        status = UNI_BAD_OPERATION;
    }
    else
    {
        status = uni_check_input_encoding(text, length, &encoding);
    }

    if (status == UNI_OK)
    {
        unichar cp;
        struct unitext it = {text, *index, length, encoding};
        status = uni_prev(it.data, it.length, it.encoding, &it.index, &cp);
        while (status == UNI_OK)
        {
            const struct BreakRule *rule = NULL;
            bool found_match = false;
            for (int32_t i = 0; i < seg->rules_count; i++) // LCOV_EXCL_BR_LINE: There is always a matching rule (therefore this is never false).
            {
                rule = &seg->rules[i];
                status = match_rule(type, rule, seg->data, &it, &found_match);
                if ((status != UNI_OK) || found_match)
                {
                    break;
                }
            }

            // All segmentation types have a wildcard match rule therefore there
            // must always be a matching rule. This check is left in place to
            // guard against future assumptions.
             // LCOV_EXCL_START
            if (rule == NULL)
            {
                status = UNI_MALFUNCTION;
            }
            // LCOV_EXCL_STOP

            if (status == UNI_OK)
            {
                assert(found_match); // LCOV_EXCL_BR_LINE
                if (!rule->mandatory_break)
                {
                    status = uni_prev(text, length, encoding, &it.index, &cp);
                }
            }

            if ((status != UNI_OK) || rule->mandatory_break)
            {
                break;
            }
        }

        if (status == UNI_OK)
        {
            *index = it.index;
        }
    }

    return status;
}

#endif

UNICORN_API unistat uni_nextbrk(unibreak boundary, const void *text, unisize text_len, uniattr text_attr, unisize *index)
{
    unistat status;
    switch (boundary)
    {
    case UNI_GRAPHEME:
#if defined(UNICORN_FEATURE_GCB)
        status = next_break(boundary, &uni_gcb, text, text_len, text_attr, index);
#else
        status = UNI_FEATURE_DISABLED;
        uni_message("extended grapheme cluster break disabled");
#endif
        break;

    case UNI_WORD:
#if defined(UNICORN_FEATURE_WB)
        status = next_break(boundary, &uni_wb, text, text_len, text_attr, index);
#else
        status = UNI_FEATURE_DISABLED;
        uni_message("word break disabled");
#endif
        break;

    case UNI_SENTENCE:
#if defined(UNICORN_FEATURE_SB)
        status = next_break(boundary, &uni_sb, text, text_len, text_attr, index);
#else
        status = UNI_FEATURE_DISABLED;
        uni_message("sentence break disabled");
#endif
        break;

    default:
        uni_message("illegal break argument");
        status = UNI_BAD_OPERATION;
        break;
    }
    return status;
}

UNICORN_API unistat uni_prevbrk(unibreak boundary, const void *text, unisize text_len, uniattr text_attr, unisize *index) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
    unistat status;
    switch (boundary)
    {
    case UNI_GRAPHEME:
#if defined(UNICORN_FEATURE_GCB)
        status = previous_break(boundary, &uni_gcb, text, text_len, text_attr, index);
#else
        status = UNI_FEATURE_DISABLED;
        uni_message("extended grapheme cluster break disabled");
#endif
        break;

    case UNI_WORD:
#if defined(UNICORN_FEATURE_WB)
        status = previous_break(boundary, &uni_wb, text, text_len, text_attr, index);
#else
        status = UNI_FEATURE_DISABLED;
        uni_message("word break disabled");
#endif
        break;

    case UNI_SENTENCE:
#if defined(UNICORN_FEATURE_SB)
        status = previous_break(boundary, &uni_sb, text, text_len, text_attr, index);
#else
        status = UNI_FEATURE_DISABLED;
        uni_message("sentence break disabled");
#endif
        break;

    default:
        uni_message("illegal break argument");
        status = UNI_BAD_OPERATION;
        break;
    }
    return status;
}
