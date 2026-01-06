/*
 *  Unicorn - Embeddable Unicode Algorithms
 *  Copyright (c) 2021-2026 Railgun Labs
 *  
 *  THE CONTENTS OF THIS PROJECT ARE PROPRIETARY AND CONFIDENTIAL. UNAUTHORIZED
 *  COPYING, TRANSFERRING OR REPRODUCTION OF THE CONTENTS OF THIS PROJECT, VIA
 *  ANY MEDIUM IS STRICTLY PROHIBITED.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#include "charbuf.h"
#include "unidata.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

static inline int32_t uni_popcnt(uint32_t n)
{
#if defined(_MSC_VER)
    return (int32_t)__popcnt(n); // cppcheck-suppress premium-misra-c-2012-17.3
#elif defined(__GNUC__) || defined(__clang__)
    return (int32_t)__builtin_popcount(n); // cppcheck-suppress premium-misra-c-2012-17.3
#else
    int32_t popcnt = 0;
    for (uint32_t v = (uint32_t)n; v != 0u; v >>= 1u)
    {
        if ((v & 1u) == 1u)
        {
            popcnt += 1;
        }
    }
    return popcnt;
#endif
}

static unistat check_encoding(uniattr *encoding)
{
    unistat status = UNI_OK;
    const uniattr enc = *encoding;
    uint32_t flags;

    // Only one or none of the endian flags can be provided.
    uniattr mask = enc & (UNI_LITTLE | UNI_BIG);
    flags = (uint32_t)mask;
    switch (uni_popcnt(flags))
    {
    case 0: // No endian flags was provided; default to native byte order.
        if ((GET_ENCODING(enc) & (UNI_UTF16 | UNI_UTF32)) != (uniattr)0)
        {
#if defined(UNICORN_BIG_ENDIAN)
            (*encoding) |= UNI_BIG;
#else
            (*encoding) |= UNI_LITTLE;
#endif
        }
        break;
    case 1: // Exactly one endian flag was provided (good).
        break;
    default: // Too many endian flags were provided (bad).
        uni_message("too many endian flags (must specify exactly one)");
        status = UNI_BAD_OPERATION;
        break;
    }

    if (status == UNI_OK)
    {
        // Make sure exactly ONE text encoding flag was provided.
        mask = GET_ENCODING(enc);
        flags = (uint32_t)mask;
        if (uni_popcnt(flags) != 1)
        {
            uni_message("expected exactly one encoding form");
            status = UNI_BAD_OPERATION;
        }
    }

    return status;
}

unistat uni_check_input_encoding(const void *text, unisize length, uniattr *encoding)
{
    unistat status = UNI_OK;
    (void)length;

    // Only destination buffers can use the null terminated flag.
    if (((*encoding) & UNI_NULIFY) == UNI_NULIFY)
    {
        uni_message("input buffer incompatible with 'UNI_NULIFY' flag");
        status = UNI_BAD_OPERATION;
    }
    else if (text == NULL)
    {
        uni_message("input buffer is null");
        status = UNI_BAD_OPERATION;
    }
    else
    {
        status = check_encoding(encoding);
    }

    return status;
}

unistat uni_check_output_encoding(const void *buffer, const unisize *capacity, uniattr *encoding)
{
    unistat status = UNI_OK;

    // Only input buffers can use the trusted.
    if (((*encoding) & UNI_TRUST) == UNI_TRUST)
    {
        uni_message("output buffer incompatible with 'UNI_TRUST' flag");
        status = UNI_BAD_OPERATION;
    }
    else if (capacity == NULL)
    {
        uni_message("output buffer capacity is null");
        status = UNI_BAD_OPERATION;
    }
    else if (*capacity < 0)
    {
        uni_message("output buffer capacity is negative");
        status = UNI_BAD_OPERATION;
    }
    else if ((buffer == NULL) && (*capacity > 0))
    {
        uni_message("output buffer cannot be null if its capacity is non-zero");
        status = UNI_BAD_OPERATION;
    }
    else
    {
        status = check_encoding(encoding);
    }

    return status;
}

UNICORN_API void uni_getversion(int32_t *major, int32_t *minor, int32_t *patch) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
    if (major != NULL)
    {
        *major = 1;
    }

    if (minor != NULL)
    {
        *minor = 0;
    }

    if (patch != NULL)
    {
        *patch = 0;
    }
}

UNICORN_API void uni_getucdversion(int32_t *major, int32_t *minor, int32_t *patch) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
    if (major != NULL)
    {
        *major = UNICODE_VERSION_MAJOR;
    }

    if (minor != NULL)
    {
        *minor = UNICODE_VERSION_MINOR;
    }

    if (patch != NULL)
    {
        *patch = UNICODE_VERSION_PATCH;
    }
}
