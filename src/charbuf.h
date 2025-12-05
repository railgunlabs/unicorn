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

#ifndef CHARBUF_H
#define CHARBUF_H

#include "common.h"

struct CharBufImpl;

struct CharBuf
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
    const struct CharBufImpl *impl;
};

struct CharBufImpl
{
    void (*append)(struct CharBuf *buf, const unichar *chars, unisize chars_count);
    void (*nullterminate)(struct CharBuf *buf);
};

unistat uni_charbuf_init(struct CharBuf *buf, void *text, unisize *capacity, uniattr attributes);
unistat uni_charbuf_finalize(struct CharBuf *buf);

void uni_charbuf_append(struct CharBuf *buf, const unichar *chars, unisize chars_count);
void uni_charbuf_appendchar(struct CharBuf *buf, unichar ch);

#endif // CHARBUF_H
