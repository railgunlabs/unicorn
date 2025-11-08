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

// Implements a dynamic character buffer.

#ifndef DYNAMIC_BUFFER_H
#define DYNAMIC_BUFFER_H

#include "common.h"

struct UDynamicBuffer
{
    unisize length;
    unisize capacity;
    unichar *chars;
    unichar scratch[UNICORN_STACK_BUFFER_SIZE];
};

void udynbuf_init(struct UDynamicBuffer *buffer);
void udynbuf_free(struct UDynamicBuffer *buffer);
void udynbuf_reset(struct UDynamicBuffer *buffer);
unistat udynbuf_append(struct UDynamicBuffer *buffer, const unichar *chars, unisize chars_count);
unistat udynbuf_reserve(struct UDynamicBuffer *buffer, unisize new_capacity);
void udynbuf_remove(struct UDynamicBuffer *buf, unisize i);
void udynbuf_append_unsafe(struct UDynamicBuffer *cb, const unichar *chars, unisize chars_count);

static inline unisize udynbuf_length(const struct UDynamicBuffer *buffer)
{
    return buffer->length;
}

static inline unisize udynbuf_capacity(const struct UDynamicBuffer *buffer)
{
    return buffer->capacity;
}

#endif // DYNAMIC_BUFFER_H
