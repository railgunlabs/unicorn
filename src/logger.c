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

#include "common.h"

static unierrfunc unicorn_logger_cb;
static void *unicorn_logger_ud;

void uni_seterrfunc(void *user_data, unierrfunc callback) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
    unicorn_logger_ud = user_data;
    unicorn_logger_cb = callback;
}

void uni_message(const char *msg)
{
    if (unicorn_logger_cb != NULL)
    {
        unicorn_logger_cb(unicorn_logger_ud, msg);
    }
}
