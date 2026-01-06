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

#ifndef NORMALIZE_H
#define NORMALIZE_H

#include "common.h"
#include "charvec.h"

typedef bool (*IsStable)(unichar cp);

// Incremental normalization state book keeping.
struct NormalizeState
{
    int32_t offset;
    int32_t r;
    IsStable is_stable;

    // Tracks an unstable span of characters within unnormalized text.
    // This is the span of text that will be normalized.
    struct CharVec span;

    // Buffer with decomposed characters.
    struct CharVec decomp;
};

void uni_norm_init(struct NormalizeState *state, IsStable is_stable);
void uni_norm_free(struct NormalizeState *state);

void uni_norm_reset(struct NormalizeState *ns);

unistat uni_norm_append_run(struct NormalizeState *state, struct unitext *it);

bool uni_is_stable_nfd(unichar cp);

#endif // NORMALIZE_H
