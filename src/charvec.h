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

// Implements a heap-allocated growable character buffer.

#ifndef CHARACTER_VECTOR_H
#define CHARACTER_VECTOR_H

#include "common.h"

struct CharVec
{
    unisize length;
    unisize capacity;
    unichar *chars;
    unichar scratch[UNICORN_STACK_BUFFER_SIZE];
};

void uni_charvec_init(struct CharVec *buffer);
void uni_charvec_free(struct CharVec *buffer);
void uni_charvec_reset(struct CharVec *buffer);
unistat uni_charvec_append(struct CharVec *buffer, const unichar *chars, unisize chars_count);
unistat uni_charvec_reserve(struct CharVec *buffer, unisize new_capacity);
void uni_charvec_remove(struct CharVec *buf, unisize i);
void uni_charvec_append_unsafe(struct CharVec *cb, const unichar *chars, unisize chars_count);

static inline unisize uni_charvec_length(const struct CharVec *buffer)
{
    return buffer->length;
}

static inline unisize uni_charvec_capacity(const struct CharVec *buffer)
{
    return buffer->capacity;
}

#endif // CHARACTER_VECTOR_H
