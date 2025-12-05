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

#ifndef DUCET_H
#define DUCET_H

#include "normalize.h"
#include "unidata.h"

#if defined(UNICORN_FEATURE_COLLATION)

typedef struct CE
{
    uint16_t primary;
    uint16_t secondary;
    uint16_t tertiary;
} CE;

struct CEDecoder
{
    int32_t ces_index;
    int32_t ces_count;
    struct NormalizeState normbuf;
    struct CharVec charvec;
    CE ces[UNICORN_MAX_COLLATION];
};

void uni_cebuf_init(struct CEDecoder *state);
void uni_cebuf_free(struct CEDecoder *state);
bool uni_cebuf_is_empty(const struct CEDecoder *state);
CE uni_cebuf_pop(struct CEDecoder *state);
unistat uni_cebuf_append_run(struct CEDecoder *state, struct unitext *text);

#endif

#endif // DUCET_H
