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
    struct UNormalizeState normbuf;
    struct UDynamicBuffer charbuf;
    CE ces[UNICORN_MAX_COLLATION];
};

void cebuf_init(struct CEDecoder *state);
void cebuf_free(struct CEDecoder *state);
bool cebuf_is_empty(const struct CEDecoder *state);
CE cebuf_pop(struct CEDecoder *state);
unistat cebuf_append_run(struct CEDecoder *state, struct unitext *text);

#endif

#endif // DUCET_H
