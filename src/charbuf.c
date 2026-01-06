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

#include "charbuf.h"
#include "byteswap.h"
#include "unidata.h"

#if defined(UNICORN_FEATURE_ENCODING_UTF8)
static void uni_charbuf_append_u8(struct CharBuf *buf, const unichar *chars, unisize chars_count)
{
    UChar8 *buffer = buf->storage; // cppcheck-suppress misra-c2012-11.5
    if (buffer != NULL)
    {
        buffer = &buffer[buf->length];
    }

    for (unisize i = 0; i < chars_count; i++)
    {
        UChar8 bytes[4];
        const unisize bytes_count = unichar_to_u8(chars[i], bytes);
        if (buffer != NULL)
        {
            if ((buf->length + bytes_count) <= buf->capacity)
            {
                (void)memcpy(buffer, bytes, (size_t)bytes_count);
                buffer = &buffer[bytes_count];
                buf->written += bytes_count;
            }
        }
        buf->length += bytes_count;
    }
}

static void uni_charbuf_nullterminate_u8(struct CharBuf *buf)
{
    UChar8 *buffer = buf->storage; // cppcheck-suppress misra-c2012-11.5
    if (buffer != NULL)
    {
        bool needs_null = true;

        // Check if the last character written is a null terminator. If it's not, then delete it
        // and append the null terminator in its place. If there's room for the null terminator
        // without deleting the last character, then do that instead.
        if (buf->written > 0)
        {
            const unisize len = uni_prev_UTF8_seqlen(buffer, buf->written);
            assert(len > 0); // LCOV_EXCL_BR_LINE
            assert(buf->written <= buf->capacity); // LCOV_EXCL_BR_LINE

            if (buffer[buf->written - len] == (UChar8)0)
            {
                needs_null = false;
            }
            else
            {
                if (buf->written == buf->capacity)
                {
                    buf->written -= len;
                }
            }
        }

        if (needs_null)
        {
            if (buf->written < buf->capacity)
            {
                buffer[buf->written] = (UChar8)0;
                buf->written += 1;
            }
        }
    }

    if (!buf->is_null_terminated)
    {
        buf->length += 1;
    }
}
#endif

#if defined(UNICORN_FEATURE_ENCODING_UTF16)
// This assumes the UTF-16 is well-formed (which it should be since it was generated programmatically).
static unisize previous_UTF16_sequence_length(const UChar16 *words, ByteSwap16 swap)
{
    unisize length = 1;
    const unichar cp = swap(words[-1]);
    if (is_low_surrogate(cp))
    {
        length = 2;
    }
    return length;
}

static void uni_charbuf_append_u16(struct CharBuf *buf, const unichar *chars, unisize chars_count, ByteSwap16 swap)
{
    UChar16 *buffer = buf->storage; // cppcheck-suppress misra-c2012-11.5
    if (buffer != NULL)
    {
        buffer = &buffer[buf->length];
    }

    for (unisize i = 0; i < chars_count; i++)
    {
        UChar16 words[2];
        const unisize words_count = unichar_to_u16(chars[i], words, swap);
        if (buffer != NULL)
        {
            if ((buf->length + words_count) <= buf->capacity)
            {
                (void)memcpy(buffer, words, sizeof(UChar16) * (size_t)words_count);
                buffer = &buffer[words_count];
                buf->written += words_count;
            }
        }
        buf->length += words_count;
    }
}

static void uni_charbuf_nullterminate_u16(struct CharBuf *buf, ByteSwap16 swap)
{
    UChar16 *buffer = buf->storage; // cppcheck-suppress misra-c2012-11.5
    if (buffer != NULL)
    {
        bool needs_null = true;

        // Check if the last character written is a null terminator. If it's not, then delete it
        // and append the null terminator in its place. If there's room for the null terminator
        // without deleting the last character, then do that instead.
        if (buf->written > 0)
        {
            const unisize len = previous_UTF16_sequence_length(&buffer[buf->written], swap);
            assert(len > 0); // LCOV_EXCL_BR_LINE
            assert(buf->written <= buf->capacity); // LCOV_EXCL_BR_LINE

            if (buffer[buf->written - len] == (UChar16)0)
            {
                needs_null = false;
            }
            else
            {
                if (buf->written == buf->capacity)
                {
                    buf->written -= len;
                }
            }
        }

        if (needs_null)
        {
            if (buf->written < buf->capacity)
            {
                buffer[buf->written] = (UChar16)0;
                buf->written += 1;
            }
        }
    }

    if (!buf->is_null_terminated)
    {
        buf->length += 1;
    }
}

static void uni_charbuf_append_u16be(struct CharBuf *buf, const unichar *chars, unisize chars_count)
{
    uni_charbuf_append_u16(buf, chars, chars_count, &uni_swap16_be);
}

static void uni_charbuf_nullterminate_u16be(struct CharBuf *buf)
{
    uni_charbuf_nullterminate_u16(buf, &uni_swap16_be);
}

static void uni_charbuf_append_u16le(struct CharBuf *buf, const unichar *chars, unisize chars_count)
{
    uni_charbuf_append_u16(buf, chars, chars_count, &uni_swap16_le);
}

static void uni_charbuf_nullterminate_u16le(struct CharBuf *buf)
{
    uni_charbuf_nullterminate_u16(buf, &uni_swap16_le);
}
#endif

#if defined(UNICORN_FEATURE_ENCODING_UTF32)
static void uni_charbuf_append_u32(struct CharBuf *buf, const unichar *chars, unisize chars_count, ByteSwap32 swap)
{
    UChar32 *buffer = buf->storage; // cppcheck-suppress misra-c2012-11.5
    if (buffer != NULL)
    {
        buffer = &buffer[buf->length];
    }

    for (unisize i = 0; i < chars_count; i++)
    {
        if ((buffer != NULL) && (buf->length < buf->capacity))
        {
            buffer[i] = swap((UChar32)chars[i]);
            buf->written += 1;
        }
    }
    buf->length += chars_count;
}

static void uni_charbuf_append_u32be(struct CharBuf *buf, const unichar *chars, unisize chars_count)
{
    uni_charbuf_append_u32(buf, chars, chars_count, &uni_swap32_be);
}

static void uni_charbuf_append_u32le(struct CharBuf *buf, const unichar *chars, unisize chars_count)
{
    uni_charbuf_append_u32(buf, chars, chars_count, &uni_swap32_le);
}

static void uni_charbuf_nullterminate_u32(struct CharBuf *buf)
{
    UChar32 *buffer = buf->storage; // cppcheck-suppress misra-c2012-11.5
    if (buffer != NULL)
    {
        bool needs_null = true;

        // Check if the last character written is a null terminator. If it's not, then delete it
        // and append the null terminator in its place. If there's room for the null terminator
        // without deleting the last character, then do that instead.
        if (buf->written > 0)
        {
            assert(buf->written <= buf->capacity); // LCOV_EXCL_BR_LINE
            if (buffer[buf->written - 1] == (UChar32)0)
            {
                needs_null = false;
            }
            else
            {
                if (buf->written == buf->capacity)
                {
                    buf->written -= 1;
                }
            }
        }

        if (needs_null)
        {
            if (buf->written < buf->capacity)
            {
                buffer[buf->written] = (UChar32)0;
                buf->written += 1;
            }
        }
    }

    if (!buf->is_null_terminated)
    {
        buf->length += 1;
    }
}
#endif

static void uni_charbuf_append_scalar(struct CharBuf *buf, const unichar *chars, unisize chars_count)
{
    unichar *buffer = buf->storage; // cppcheck-suppress misra-c2012-11.5
    if (buffer != NULL)
    {
        buffer = &buffer[buf->length];
    }

    for (unisize i = 0; i < chars_count; i++)
    {
        if ((buffer != NULL) && (buf->length < buf->capacity))
        {
            buffer[i] = chars[i];
            buf->written += 1;
        }
    }
    buf->length += chars_count;
}

static void uni_charbuf_nullterminate_scalar(struct CharBuf *buf)
{
    unichar *buffer = buf->storage; // cppcheck-suppress misra-c2012-11.5
    if (buffer != NULL)
    {
        bool needs_null = true;

        // Check if the last character written is a null terminator. If it's not, then delete it
        // and append the null terminator in its place. If there's room for the null terminator
        // without deleting the last character, then do that instead.
        if (buf->written > 0)
        {
            assert(buf->written <= buf->capacity); // LCOV_EXCL_BR_LINE
            if (buffer[buf->written - 1] == UNICHAR_C(0))
            {
                needs_null = false;
            }
            else
            {
                if (buf->written == buf->capacity)
                {
                    buf->written -= 1;
                }
            }
        }

        if (needs_null)
        {
            if (buf->written < buf->capacity)
            {
                buffer[buf->written] = UNICHAR_C(0);
                buf->written += 1;
            }
        }
    }

    if (!buf->is_null_terminated)
    {
        buf->length += 1;
    }
}

unistat uni_charbuf_init(struct CharBuf *buf, void *text, unisize *capacity, uniattr attributes)
{
#if defined(UNICORN_FEATURE_ENCODING_UTF8)
    static const struct CharBufImpl uni_charbuf_u8 = {
        .append = &uni_charbuf_append_u8,
        .nullterminate = &uni_charbuf_nullterminate_u8,
    };
#endif

#if defined(UNICORN_FEATURE_ENCODING_UTF16)
    static const struct CharBufImpl uni_charbuf_u16be = {
        .append = &uni_charbuf_append_u16be,
        .nullterminate = &uni_charbuf_nullterminate_u16be,
    };

    static const struct CharBufImpl uni_charbuf_u16le = {
        .append = &uni_charbuf_append_u16le,
        .nullterminate = &uni_charbuf_nullterminate_u16le,
    };
#endif

#if defined(UNICORN_FEATURE_ENCODING_UTF32)
    static const struct CharBufImpl uni_charbuf_u32be = {
        .append = &uni_charbuf_append_u32be,
        .nullterminate = &uni_charbuf_nullterminate_u32,
    };

    static const struct CharBufImpl uni_charbuf_u32le = {
        .append = &uni_charbuf_append_u32le,
        .nullterminate = &uni_charbuf_nullterminate_u32,
    };
#endif

    static const struct CharBufImpl uni_charbuf_scalar = {
        .append = &uni_charbuf_append_scalar,
        .nullterminate = &uni_charbuf_nullterminate_scalar,
    };

    unistat status = uni_check_output_encoding(text, capacity, &attributes);
    if (status == UNI_OK)
    {
        buf->storage = text;
        buf->capacity = *capacity;
        buf->length = 0;
        buf->written = 0;
        buf->result = capacity;
        buf->null_terminate = ((attributes & UNI_NULIFY) == UNI_NULIFY) ? true : false;
        buf->is_null_terminated = false;

        switch (GET_ENCODING(attributes)) // LCOV_EXCL_BR_LINE
        {
#if defined(UNICORN_FEATURE_ENCODING_UTF8)
        case UNI_UTF8:
            buf->impl = &uni_charbuf_u8;
            break;
#endif

#if defined(UNICORN_FEATURE_ENCODING_UTF16)
        case UNI_UTF16:
            if ((attributes & UNI_LITTLE) == UNI_LITTLE)
            {
                buf->impl = &uni_charbuf_u16le;
            }
            else
            {
                buf->impl = &uni_charbuf_u16be;
            }
            break;
#endif

#if defined(UNICORN_FEATURE_ENCODING_UTF32)
        case UNI_UTF32:
            if ((attributes & UNI_LITTLE) == UNI_LITTLE)
            {
                buf->impl = &uni_charbuf_u32le;
            }
            else
            {
                buf->impl = &uni_charbuf_u32be;
            }
            break;
#endif

        case UNI_SCALAR:
            buf->impl = &uni_charbuf_scalar;
            break;

        // LCOV_EXCL_START: This code path is tested via feature configuration tests.
        default:
            status = UNI_FEATURE_DISABLED;
            break;
        // LCOV_EXCL_STOP
        }
    }

    return status;
}

unistat uni_charbuf_finalize(struct CharBuf *buf)
{
    unistat status = UNI_OK;

    if (buf->null_terminate)
    {
        buf->impl->nullterminate(buf);
    }

    if (buf->storage != NULL)
    {
        if (buf->length > buf->capacity)
        {
            status = UNI_NO_SPACE;
        }
        *buf->result = buf->written;
    }
    else
    {
        *buf->result = buf->length;
    }

    return status;
}

void uni_charbuf_append(struct CharBuf *buf, const unichar *chars, unisize chars_count)
{
    buf->impl->append(buf, chars, chars_count);

    // If the last appended character _is_ a null terminator, then record that fact.
    // This detail is used to stop the implementation from appending a null terminator twice.
    if (chars_count > 0)
    {
        if (chars[chars_count - 1] == UNICHAR_C(0x0))
        {
            buf->is_null_terminated = true;
        }
        else
        {
            buf->is_null_terminated = false;
        }
    }
}

void uni_charbuf_appendchar(struct CharBuf *buf, unichar ch)
{
    uni_charbuf_append(buf, &ch, 1);
}
