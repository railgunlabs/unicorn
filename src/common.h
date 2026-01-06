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

#ifndef COMMON_H
#define COMMON_H

#include "unicorn.h"
#include <string.h>
#include <assert.h>

#define GET_ENCODING(FLAGS) ((FLAGS) & (UNI_SCALAR | UNI_UTF8 | UNI_UTF16 | UNI_UTF32))

#define UNICHAR_C(X) ((unichar)(X))
#define UNISIZE_C(X) ((unisize)(X))
#define UNIHASH_C(X) ((unihash)(X))

#define UNICORN_LARGEST_CODE_POINT UNICHAR_C(0x10FFFFL)
#define FIRST_ILLEGAL_CODE_POINT UNICHAR_C(0x110000L)

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

typedef uint8_t UChar8;
typedef uint16_t UChar16;
typedef uint32_t UChar32;

typedef uint32_t unihash;

typedef uint16_t(*ByteSwap16)(uint16_t val);
typedef uint32_t(*ByteSwap32)(uint32_t val);

struct unitext
{
    const void *data;
    unisize index;
    unisize length;
    uniattr encoding;
};

void *uni_malloc(size_t size);
void *uni_realloc(void *old_ptr, size_t old_size, size_t new_size);
void uni_free(void *ptr, size_t size);

unisize unichar_to_u8(unichar codepoint, UChar8 bytes[4]);
unisize unichar_to_u16(unichar codepoint, UChar16 words[2], ByteSwap16 swap); // cppcheck-suppress premium-misra-c-2012-17.3 ; This is a false positive.

unisize uni_prev_UTF8_seqlen(const UChar8 *start, unisize offset);

unistat uni_check_input_encoding(const void *text, unisize length, uniattr *encoding);
unistat uni_check_output_encoding(const void *buffer, const unisize *capacity, uniattr *encoding);

void uni_message(const char *msg);

static inline bool is_low_surrogate(unichar c)
{
    bool is_low;
    if (c < UNICHAR_C(0xDC00))
    {
        is_low = false;
    }
    else if (c > UNICHAR_C(0xDFFF))
    {
        is_low = false;
    }
    else
    {
        is_low = true;
    }
    return is_low;
}

static inline bool is_high_surrogate(unichar c)
{
    bool is_high;
    if (c < UNICHAR_C(0xD800))
    {
        is_high = false;
    }
    else if (c > UNICHAR_C(0xDBFF))
    {
        is_high = false;
    }
    else
    {
        is_high = true;
    }
    return is_high;
}

#if defined(_MSC_VER)
#define UNREACHABLE __assume(false)
#elif defined(__GNUC__) || defined(__clang__)
#define UNREACHABLE __builtin_unreachable()
#else
#define UNREACHABLE assert(0 && "unreachable code") // LCOV_EXCL_BR_LINE
#endif

#endif // COMMON_H
