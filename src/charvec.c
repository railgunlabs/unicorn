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

#include "charvec.h"

unistat uni_charvec_reserve(struct CharVec *buffer, unisize new_capacity)
{
    unistat status = UNI_OK;

    // LCOV_EXCL_START
    assert(buffer != NULL);
    assert(new_capacity >= 0);
    // LCOV_EXCL_STOP

    if (new_capacity > buffer->capacity)
    {
        unichar *ptr;
        if (buffer->chars == buffer->scratch)
        {
            ptr = uni_malloc(sizeof(buffer->chars[0]) * (size_t)new_capacity); // cppcheck-suppress misra-c2012-11.5 ; MISRA 2012 Rule 11.5 - Pointer conversion required.
            if (ptr != NULL)
            {
                (void)memcpy(ptr, buffer->scratch, sizeof(buffer->chars[0]) * (size_t)buffer->capacity);
            }
        }
        else
        {
            ptr = uni_realloc(buffer->chars, // cppcheck-suppress misra-c2012-11.5 ; MISRA 2012 Rule 11.5 - Pointer conversion required.
                              sizeof(buffer->chars[0]) * (size_t)buffer->capacity,
                              sizeof(buffer->chars[0]) * (size_t)new_capacity);
        }

        if (ptr == NULL)
        {
            uni_message("memory allocation failed");
            status = UNI_NO_MEMORY;
        }
        else
        {
            buffer->chars = ptr;
            buffer->capacity = new_capacity;
        }
    }

    return status;
}

unistat uni_charvec_append(struct CharVec *buffer, const unichar *chars, unisize chars_count)
{
    // LCOV_EXCL_START
    assert(buffer);
    assert(chars);
    // LCOV_EXCL_STOP

    const unistat status = uni_charvec_reserve(buffer, buffer->length + chars_count);
    if (status == UNI_OK)
    {
        (void)memcpy(&buffer->chars[buffer->length], chars, sizeof(buffer->chars[0]) * (size_t)chars_count);
        buffer->length += chars_count;
    }

    return status;
}

void uni_charvec_append_unsafe(struct CharVec *cb, const unichar *chars, unisize chars_count)
{
    assert((cb->length + chars_count) <= cb->capacity);  // LCOV_EXCL_BR_LINE
    (void)memcpy(&cb->chars[cb->length], chars, sizeof(chars[0]) * (size_t)chars_count);
    cb->length += chars_count;
}

void uni_charvec_reset(struct CharVec *buffer)
{
    buffer->length = 0;
}

void uni_charvec_init(struct CharVec *buffer)
{
    const size_t scratch_buffer_size = COUNT_OF(buffer->scratch);
    buffer->length = 0;
    buffer->capacity = (unisize)scratch_buffer_size;
    buffer->chars = buffer->scratch;
}

void uni_charvec_free(struct CharVec *buffer)
{
    if (buffer->chars != buffer->scratch)
    {
        uni_free(buffer->chars, sizeof(buffer->chars[0]) * (size_t)buffer->capacity);
    }
}

void uni_charvec_remove(struct CharVec *buf, unisize i)
{
    assert(buf->length >= 1); // LCOV_EXCL_BR_LINE
    const unisize shift = buf->length - (i + UNISIZE_C(1));
    assert(shift >= 0); // LCOV_EXCL_BR_LINE: The math should never work out negative.
    (void)memmove(&buf->chars[i], &buf->chars[i + 1], sizeof(buf->chars[0]) * (size_t)shift);
    buf->length -= 1;
}
