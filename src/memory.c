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

#include "common.h"
#include "unidata.h"

#if defined(UNICORN_HAVE_C_MEMORY_ROUTINES)
#include <stdlib.h>
static void *default_allocf(void *ud, void *ptr, size_t osize, size_t nsize)
{
    (void)ud;
    (void)osize;
    void *mem = NULL;
    if (nsize == (size_t)0)
    {
        free(ptr); // cppcheck-suppress misra-c2012-21.3 ; This function is optional.
    }
    else
    {
        mem = realloc(ptr, nsize); // cppcheck-suppress misra-c2012-21.3 ; This function is optional.
    }
    return mem;
}
static unimemfunc unicorn_allocf = &default_allocf;
#else
static unimemfunc unicorn_allocf;
#endif

static void *unicorn_ud;

UNICORN_API unistat uni_setmemfunc(void *user_data, unimemfunc allocf) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
    unistat status = UNI_OK;
    if (allocf == NULL)
    {
        unicorn_ud = NULL;
#if defined(UNICORN_HAVE_C_MEMORY_ROUTINES)
        unicorn_allocf = &default_allocf;
#else
        unicorn_allocf = NULL;
        status = UNI_FEATURE_DISABLED;
#endif
    }
    else
    {
        unicorn_ud = user_data;
        unicorn_allocf = allocf;
    }
    return status;
}

void *uni_malloc(size_t size)
{
    void *ptr = NULL;
    if (unicorn_allocf != NULL) // LCOV_EXCL_BR_LINE: Branch taken only by the features.json combinations tests.
    {
        ptr = unicorn_allocf(unicorn_ud, NULL, 0, size);
    }
    return ptr;
}

void *uni_realloc(void *old_ptr, size_t old_size, size_t new_size)
{
    void *new_ptr = NULL;
    if (unicorn_allocf != NULL) // LCOV_EXCL_BR_LINE: Branch taken only by the features.json combinations tests.
    {
        new_ptr = unicorn_allocf(unicorn_ud, old_ptr, old_size, new_size);
    }
    return new_ptr;
}

void uni_free(void *ptr, size_t size)
{
    if (unicorn_allocf != NULL) // LCOV_EXCL_BR_LINE: Branch taken only by the features.json combinations tests.
    {
        (void)unicorn_allocf(unicorn_ud, ptr, size, 0);
    }
}
