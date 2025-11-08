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

#ifndef BUFFER_H
#define BUFFER_H

#include "common.h"

struct UBufferDelegate;

struct UBuffer
{
    void *storage;

    // This is where the next character will be written.
    // When finished this is the number of characters that needed to be
    // written, but not necessarily how many were actually written.
    unisize length;

    // Number of characters actually written.
    unisize written;

    unisize capacity;
    unisize *result;
    bool null_terminate;
    bool is_null_terminated;
    const struct UBufferDelegate *delegate;
};

struct UBufferDelegate
{
    void (*append)(struct UBuffer *buf, const unichar *chars, unisize chars_count);
    void (*nullterminate)(struct UBuffer *buf);
};

unistat ubuffer_init(struct UBuffer *buf, void *text, unisize *capacity, uniattr attributes);
unistat ubuffer_finalize(struct UBuffer *buf);

void ubuffer_append(struct UBuffer *buf, const unichar *chars, unisize chars_count);
void ubuffer_appendchar(struct UBuffer *buf, unichar ch);

#endif // BUFFER_H
