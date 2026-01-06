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

#ifndef BYTESWAP_H
#define BYTESWAP_H

#include "common.h"

static inline uint32_t uni_swap32(uint32_t val)
{
    uint32_t valm = (val << 8u) & 0xFF00FF00u;
    valm |= (val >> 8u) & 0xFF00FFu;
    return (valm << 16u) | (valm >> 16u);
}

static inline uint16_t uni_swap16(uint16_t val)
{
    const uint32_t byte1 = (uint32_t)val << 8u;
    const uint32_t byte2 = (uint32_t)val >> 8u;
    return (uint16_t)(byte1 | byte2);
}

static inline uint32_t uni_swap32_le(uint32_t val)
{
#if defined(UNICORN_BIG_ENDIAN)
    return uni_swap32(val);
#else
    return val;
#endif
}

static inline uint16_t uni_swap16_le(uint16_t val)
{
#if defined(UNICORN_BIG_ENDIAN)
    return uni_swap16(val);
#else
    return val;
#endif
}

static inline uint32_t uni_swap32_be(uint32_t val)
{
#if defined(UNICORN_LITTLE_ENDIAN)
    return uni_swap32(val);
#else
    return val;
#endif
}

static inline uint16_t uni_swap16_be(uint16_t val)
{
#if defined(UNICORN_LITTLE_ENDIAN)
    return uni_swap16(val);
#else
    return val;
#endif
}

#endif // BYTESWAP_H
