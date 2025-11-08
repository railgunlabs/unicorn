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

#ifndef NORMALIZE_H
#define NORMALIZE_H

#include "common.h"
#include "charbuf.h"

typedef bool (*IsStable)(unichar cp);

// Incremental normalization state book keeping.
struct UNormalizeState
{
    int32_t offset;
    int32_t r;
    IsStable is_stable;

    // Tracks an unstable span of characters within unnormalized text.
    // This is the span of text that will be normalized.
    struct UDynamicBuffer span;

    // Buffer with decomposed characters.
    struct UDynamicBuffer decomp;
};

void nstate_init(struct UNormalizeState *state, IsStable is_stable);
void nstate_free(struct UNormalizeState *state);

void ustate_reset(struct UNormalizeState *ns);

unistat unorm_append_run(struct UNormalizeState *state, struct unitext *it);

bool unicorn_is_stable_nfd(unichar cp);

#endif // NORMALIZE_H
