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

// This code is based on the C reference implementation linked from 
// Unicode Technical Note #6: MIME-Compatible Unicode Compression.

// The BOCU-1 compression algorithm encodes the code points of a Unicode string as
// a sequence of byte-encoded differences (slope detection).

#include "common.h"
#include "charbuf.h"
#include "unidata.h"

#if defined(UNICORN_FEATURE_COMPRESSION)

// The initial value for "prev" is the middle of the ASCII range.
#define BOCU1_ASCII_PREV 0x40

// Bounding byte values for differences.
#define BOCU1_MIN 0x21
#define BOCU1_MIDDLE 0x90
#define BOCU1_MAX_TRAIL 0xFF
#define BOCU1_RESET 0xFF

// Adjust trail byte counts for the use of some C0 control byte values.
#define BOCU1_TRAIL_CONTROLS_COUNT 20
#define BOCU1_TRAIL_BYTE_OFFSET (BOCU1_MIN - BOCU1_TRAIL_CONTROLS_COUNT)

// number of trail bytes.
#define BOCU1_TRAIL_COUNT (((BOCU1_MAX_TRAIL - BOCU1_MIN) + 1) + BOCU1_TRAIL_CONTROLS_COUNT)

// Number of positive and negative single-byte codes
// (counting 0==BOCU1_MIDDLE among the positive ones).
#define BOCU1_SINGLE 64

// Number of lead bytes for positive and negative 2/3/4-byte sequences.
#define BOCU1_LEAD_2 43
#define BOCU1_LEAD_3 3

// The difference value range for single-bytes.
#define BOCU1_REACH_POS_1 (BOCU1_SINGLE - 1)
#define BOCU1_REACH_NEG_1 (-BOCU1_SINGLE)

// The difference value range for double-bytes.
#define BOCU1_REACH_POS_2 (BOCU1_REACH_POS_1 + (BOCU1_LEAD_2 * BOCU1_TRAIL_COUNT))
#define BOCU1_REACH_NEG_2 (BOCU1_REACH_NEG_1 - (BOCU1_LEAD_2 * BOCU1_TRAIL_COUNT))

// The difference value range for 3-bytes.
#define BOCU1_REACH_POS_3 (BOCU1_REACH_POS_2 + ((BOCU1_LEAD_3 * BOCU1_TRAIL_COUNT) * BOCU1_TRAIL_COUNT))
#define BOCU1_REACH_NEG_3 (BOCU1_REACH_NEG_2 - ((BOCU1_LEAD_3 * BOCU1_TRAIL_COUNT) * BOCU1_TRAIL_COUNT))

// The lead byte start values.
#define BOCU1_START_POS_2 ((BOCU1_MIDDLE + BOCU1_REACH_POS_1) + 1)
#define BOCU1_START_POS_3 (BOCU1_START_POS_2 + BOCU1_LEAD_2)
#define BOCU1_START_POS_4 (BOCU1_START_POS_3 + BOCU1_LEAD_3)

#define BOCU1_START_NEG_2 (BOCU1_MIDDLE + BOCU1_REACH_NEG_1)
#define BOCU1_START_NEG_3 (BOCU1_START_NEG_2 - BOCU1_LEAD_2)
#define BOCU1_START_NEG_4 (BOCU1_START_NEG_3 - BOCU1_LEAD_3)

enum decoderstate
{
    DECODER_OK,
    DECODER_FATAL_ERROR, // Indicates a decoding error.
    DECODER_CONTINUE, // Indicates more bytes must be read to decode the code point.
};

struct bocudecoder
{
    int32_t prev;
    int32_t count;
    int32_t diff;
};

// The length of a byte sequence, according to its packed form.
static inline int32_t bocu1_length_from_packed(int32_t package)
{
    uint32_t p = (uint32_t)package;
    uint32_t length;
    if (p < 0x04000000u)
    {
        assert(package >= 0); // LCOV_EXCL_BR_LINE
        length = p >> 24u;
    }
    else
    {
        length = 4u;
    }
    return (int32_t)length;
}

/*
 *  12 commonly used C0 control codes (and space) encode themselves directly,
 *  which makes BOCU-1 MIME-usable and reasonably safe for ASCII-oriented software.
 * 
 *  These control characters are:
 *   0   NUL
 *   7   BEL
 *   8   BS
 *   9   TAB
 *   a   LF
 *   b   VT
 *   c   FF
 *   d   CR
 *   e   SO
 *   f   SI
 *   1a  SUB
 *   1b  ESC
 *
 *  The other 20 C0 controls are also encoded directly (to preserve order)
 *  but are also used as trail bytes in difference encoding (for better compression).
 */
static inline uint32_t bocu1_trail_to_byte(uint32_t m)
{
    // Byte value map for control codes,
    // from trail byte values 0..19 (0..0x13) as used in the difference calculation
    // to external byte values 0x00..0x20.
    static const int8_t trail_to_byte[] = {
    /*  0     1     2     3     4     5     6     7    */
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x10, 0x11,

    /*  8     9     a     b     c     d     e     f    */
        0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,

    /*  10    11    12    13   */
        0x1c, 0x1d, 0x1e, 0x1f
    };
    assert(COUNT_OF(trail_to_byte) == (size_t)BOCU1_TRAIL_CONTROLS_COUNT); // LCOV_EXCL_BR_LINE

    uint32_t controL_code = 0;
    if (m >= (uint32_t)BOCU1_TRAIL_CONTROLS_COUNT)
    {
        assert(BOCU1_TRAIL_BYTE_OFFSET >= 0); // LCOV_EXCL_BR_LINE
        controL_code = m + (uint32_t)BOCU1_TRAIL_BYTE_OFFSET;
    }
    else
    {
        controL_code = (uint32_t)trail_to_byte[m];
    }
    return controL_code;
}

// Compute the next "previous" value for differencing from the current code point.
// The return value is a "previous code point" state value.
static inline int32_t bocu1_prev(int32_t cp)
{
    int32_t value = 0;
    if ((cp >= 0x3040) && (cp <= 0x309F))
    {
        // Hiragana is not 128-aligned
        value = 0x3070;
    }
    else if ((cp >= 0x4E00) && (cp <= 0x9FA5))
    {
        // CJK Unihan
        value = 0x4E00 - BOCU1_REACH_NEG_2;
    }
    else if ((cp >= 0xAC00) && (cp <= 0xD7A3))
    {
        // Korean Hangul
        value = (0xD7A3 + 0xAC00) / 2;
    }
    else
    {
        // Mostly small scripts
        const uint32_t ucp = (uint32_t)cp;
        const uint32_t uval = (ucp & ~0x7Fu) + (uint32_t)BOCU1_ASCII_PREV;
        value = (int32_t)uval;
    }
    return value;
}

// Encode the difference -0x10FFFF..0x10FFFF in 1..4 bytes and return it in a packed integer.
// The encoding favors small differences with short encodings to compress runs of same-script characters.
// Returns:
//     0x010000zz for 1-byte sequence zz
//     0x0200yyzz for 2-byte sequence yy zz
//     0x03xxyyzz for 3-byte sequence xx yy zz
//     0xwwxxyyzz for 4-byte sequence ww xx yy zz (ww>0x03)
static int32_t pack_diff(int32_t c)
{
    int32_t diff = c;
    int32_t lead = 0;
    int32_t count = 0;
    int32_t result = (int32_t)FIRST_ILLEGAL_CODE_POINT;

    if (diff >= BOCU1_REACH_NEG_1)
    {
        // Mostly positive differences and single-byte negative ones.
        if (diff <= BOCU1_REACH_POS_1)
        {
            // Single byte
            const int32_t single = BOCU1_MIDDLE + diff;
            assert(single >= 0); // LCOV_EXCL_BR_LINE
            const uint32_t r = 0x01000000u | (uint32_t)single;
            result = (int32_t)r;
        }
        else if (diff <= BOCU1_REACH_POS_2)
        {
            // Two bytes
            diff -= BOCU1_REACH_POS_1 + 1;
            lead = BOCU1_START_POS_2;
            count = 1;
        }
        else if (diff <= BOCU1_REACH_POS_3)
        {
            // Three bytes
            diff -= BOCU1_REACH_POS_2 + 1;
            lead = BOCU1_START_POS_3;
            count = 2;
        }
        else
        {
            // Four bytes
            diff -= BOCU1_REACH_POS_3 + 1;
            lead = BOCU1_START_POS_4;
            count = 3;
        }
    }
    else
    {
        // Two and four byte negative differences.
        if (diff >= BOCU1_REACH_NEG_2)
        {
            // Two bytes
            diff -= BOCU1_REACH_NEG_1;
            lead = BOCU1_START_NEG_2;
            count = 1;
        }
        else if (diff >= BOCU1_REACH_NEG_3)
        {
            // Three bytes
            diff -= BOCU1_REACH_NEG_2;
            lead = BOCU1_START_NEG_3;
            count = 2;
        }
        else
        {
            // four bytes
            diff -= BOCU1_REACH_NEG_3;
            lead = BOCU1_START_NEG_4;
            count = 3;
        }
    }

    if (result == (int32_t)FIRST_ILLEGAL_CODE_POINT)
    {
        uint32_t r;

        // Encode the length of the packed result.
        if (count < 3)
        {
            const int32_t offset = count + 1;
            assert(offset >= 0); // LCOV_EXCL_BR_LINE
            r = (uint32_t)offset << 24u;
        }
        else
        {
            assert(count == 3); // LCOV_EXCL_BR_LINE: MSB used for the lead byte.
            r = 0u;
        }

        // Calculate trail bytes like digits in itoa().
        uint32_t shift = 0u;
        do
        {
            // Integer division and modulo with negative numerators yields negative modulo
            // results and quotients that are one more than what we need here.
            // Adjust the results so that the modulo-value m is always >=0.
            int32_t m = diff % BOCU1_TRAIL_COUNT;
            diff /= BOCU1_TRAIL_COUNT;
            if (m < 0)
            {
                diff -= 1;
                m += BOCU1_TRAIL_COUNT;
            }
            assert(m >= 0); // LCOV_EXCL_BR_LINE
            r |= bocu1_trail_to_byte((uint32_t)m) << shift;
            shift += 8u;
            count -= 1;
        } while (count > 0);

        // Add to lead byte.
        lead += diff;
        assert(lead >= 0); // LCOV_EXCL_BR_LINE

        // Prevent undefined behavior by casting to an unsigned integer before shifting.
        // This is a safe cast because the lead byte is guaranteed to be non-negative.
        r |= (uint32_t)lead << (uint32_t)shift;
        result = (int32_t)r;
    }

    return result;
}

static void decode_bocu1_lead_byte(struct bocudecoder *decoder, int32_t byte)
{
    int32_t c;
    int32_t count;
    if (byte >= BOCU1_START_NEG_2)
    {
        // Positive difference
        if (byte < BOCU1_START_POS_3)
        {
            // Two bytes
            c = (((byte - BOCU1_START_POS_2) * BOCU1_TRAIL_COUNT) + BOCU1_REACH_POS_1) + 1;
            count = 1;
        }
        else if (byte < BOCU1_START_POS_4)
        {
            // Three bytes
            c = ((((byte - BOCU1_START_POS_3) * BOCU1_TRAIL_COUNT) * BOCU1_TRAIL_COUNT) + BOCU1_REACH_POS_2) + 1;
            count = 2;
        }
        else
        {
            // Four bytes
            c = BOCU1_REACH_POS_3 + 1;
            count = 3;
        }
    }
    else
    {
        // Negative difference
        if (byte >= BOCU1_START_NEG_3)
        {
            // Two bytes
            c = ((byte - BOCU1_START_NEG_2) * BOCU1_TRAIL_COUNT) + BOCU1_REACH_NEG_1;
            count = 1;
        }
        else if (byte > BOCU1_MIN)
        {
            // Three bytes
            c = (((byte - BOCU1_START_NEG_3) * BOCU1_TRAIL_COUNT) * BOCU1_TRAIL_COUNT) + BOCU1_REACH_NEG_2;
            count = 2;
        }
        else
        {
            // Four bytes
            c = ((-BOCU1_TRAIL_COUNT * BOCU1_TRAIL_COUNT) * BOCU1_TRAIL_COUNT) + BOCU1_REACH_NEG_3;
            count = 3;
        }
    }

    // Set the state for decoding the trail byte(s).
    decoder->diff = c;
    decoder->count = count;
}

static enum decoderstate decode_bocu1_trail_byte(struct bocudecoder *decoder, int32_t byte, unichar *codepoint)
{
    // Byte value map for control codes,
    // from external byte values 0x00..0x20 to trail byte values 0..19 (0..0x13) as used in the
    // difference calculation. External byte values that are illegal as trail bytes are mapped to -1.
    static const int8_t byte_to_trail[] = {
    /*  0     1     2     3     4     5     6     7    */
        -1,   0x00, 0x01, 0x02, 0x03, 0x04, 0x05, -1,

    /*  8     9     a     b     c     d     e     f    */
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,

    /*  10    11    12    13    14    15    16    17   */
        0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,

    /*  18    19    1a    1b    1c    1d    1e    1f   */
        0x0e, 0x0f, -1,   -1,   0x10, 0x11, 0x12, 0x13,

    /*  20   */
        -1
    };
    assert(COUNT_OF(byte_to_trail) == (size_t)BOCU1_MIN); // LCOV_EXCL_BR_LINE
    assert(byte >= 0); // LCOV_EXCL_BR_LINE

    int32_t trail;
    enum decoderstate result = DECODER_OK;

    if (byte <= 0x20)
    {
        // Skip some C0 controls and make the trail byte range contiguous.
        trail = (int32_t)byte_to_trail[byte];
        if (trail < 0)
        {
            // Illegal trail byte value.
            decoder->prev = BOCU1_ASCII_PREV;
            decoder->count = 0;
            result = DECODER_FATAL_ERROR;
        }
    }
    else
    {
        trail = byte - BOCU1_TRAIL_BYTE_OFFSET;
    }

    if (result == DECODER_OK)
    {
        // Add trail byte into difference and decrement count.
        const int32_t count = decoder->count;
        if (count == 1)
        {
            // Final trail byte, deliver a code point.
            const int32_t cp = decoder->prev + decoder->diff + trail;
            if ((cp >= 0) && (cp <= 0x10FFFF))
            {
                // Valid code point result.
                decoder->prev = bocu1_prev(cp);
                decoder->count = 0;
                *codepoint = (unichar)cp;
                result = DECODER_OK;
            }
            else
            {
                // Illegal code point result.
                decoder->prev = BOCU1_ASCII_PREV;
                decoder->count = 0;
                result = DECODER_FATAL_ERROR;
            }
        }
        else
        {
            // Intermediate trail byte.
            if (count == 2)
            {
                decoder->diff += trail * BOCU1_TRAIL_COUNT;
            }
            else
            {
                assert(count == 3); // LCOV_EXCL_BR_LINE
                decoder->diff += trail * BOCU1_TRAIL_COUNT * BOCU1_TRAIL_COUNT;
            }

            decoder->count = count - 1;
            result = DECODER_CONTINUE;
        }
    }

    return result;
}

#endif

UNICORN_API unistat uni_compress(const void *text, unisize text_len, uniattr text_attr, uint8_t *buffer, size_t *buffer_length) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
#if defined(UNICORN_FEATURE_COMPRESSION)
    int32_t buffer_index = 0;
    int32_t packed;
    int32_t prev_state = BOCU1_ASCII_PREV;
    unistat status;

    // Do the null check here to simplify the 'else' body below.
    if (buffer_length == NULL)
    {
        uni_message("output buffer capacity is null");
        status = UNI_BAD_OPERATION;
    }
    else
    {
        // The parameters for the destination buffer are identical to character buffers.
        // Reuse the existing error checking logic rather than repeating it here.
        unisize unused1 = (*buffer_length > (size_t)0) ? UNISIZE_C(1) : UNISIZE_C(0);
        uniattr unused2 = UNI_SCALAR;
        status = uni_check_output_encoding(buffer, &unused1, &unused2);
    }

    if (status == UNI_OK)
    {
        status = uni_check_input_encoding(text, text_len, &text_attr);
    }

    if (status == UNI_OK)
    {
        const int32_t buffer_capacity = (int32_t)*buffer_length;
        struct unitext it = {text, 0, text_len, text_attr};
        for (;;)
        {
            unichar cp;
            status = uni_next(it.data, it.length, it.encoding, &it.index, &cp);
            if (status != UNI_OK)
            {
                if (status == UNI_DONE)
                {
                    status = UNI_OK;
                }
                break;
            }

            if (cp <= UNICHAR_C(0x20))
            {
                // ISO C0 control & space: Encode directly for MIME compatibility,
                // and reset state except for space, to not disrupt compression.
                if (cp != UNICHAR_C(0x20))
                {
                    prev_state = BOCU1_ASCII_PREV;
                }
                const uint32_t p = 0x01000000u | (uint32_t)cp;
                packed = (int32_t)p;
            }
            else
            {
                // All other Unicode code points U+0021..U+10ffff are encoded with the difference.
                // A new prev is computed and placed in the middle of a 0x80-block (for most small scripts) or
                // in the middle of the Unihan and Hangul blocks to statistically minimize the following difference.
                const int32_t c = (int32_t)cp;
                const int32_t prev = prev_state;
                prev_state = bocu1_prev(c);
                packed = pack_diff(c - prev);
            }

            // Copy the packed bytes to the destination buffer.
            const int32_t packed_length = bocu1_length_from_packed(packed);
            if (buffer != NULL)
            {
                if ((buffer_index + packed_length) <= buffer_capacity)
                {
                    int32_t j = (packed_length - 1) * 8;
                    for (int32_t i = 0; i < packed_length; i++)
                    {
                        const uint32_t tmp = (uint32_t)packed >> (uint32_t)j;
                        buffer[buffer_index + i] = (uint8_t)tmp & (uint8_t)0xFF;
                        j -= 8;
                    }
                }
            }
            buffer_index += packed_length;
        }

        if (status == UNI_OK)
        {
            if (buffer_index > buffer_capacity)
            {
                *buffer_length = (size_t)buffer_index;
                status = UNI_NO_SPACE;
            }
            else
            {
                *buffer_length = (size_t)buffer_index;
            }
        }
    }

    return status;
#else
    uni_message("compression disabled");
    return UNI_FEATURE_DISABLED;
#endif
}

UNICORN_API unistat uni_decompress(const uint8_t *buffer, size_t buffer_length, void *text, unisize *text_len, uniattr text_attr) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
#if defined(UNICORN_FEATURE_COMPRESSION)
    unistat status = UNI_OK;
    struct CharBuf output = {NULL};

    if (buffer == NULL)
    {
        uni_message("required argument is null");
        status = UNI_BAD_OPERATION;
    }
    else
    {
        status = uni_charbuf_init(&output, text, text_len, text_attr);
    }
   
    if (status == UNI_OK)
    {
        struct bocudecoder decoder = {.prev = BOCU1_ASCII_PREV};
        for (size_t i = 0; i < buffer_length; i++)
        {
            unichar cp;
            const int32_t byte = (int32_t)buffer[i];
            enum decoderstate state = DECODER_OK;
            if (decoder.count == 0)
            {
                // Byte in lead position.
                if (byte <= 0x20)
                {
                    // Direct-encoded C0 control code or space.
                    // Reset prev for C0 control codes but not for space.
                    if (byte != 0x20)
                    {
                        decoder.prev = BOCU1_ASCII_PREV;
                    }
                    cp = (unichar)byte;
                }
                // The value of 'byte' is a difference lead byte.
                // Calculate a code point directly from a single-byte difference.
                //
                // For multi-byte difference lead bytes, set the decoder state
                // with the partial difference value from the lead byte and
                // with the number of trail bytes.
                //
                // For four-byte differences, the signedness also affects the
                // first trail byte, which has special handling farther below.
                else if ((byte >= BOCU1_START_NEG_2) && (byte < BOCU1_START_POS_2))
                {
                    // Single-byte difference.
                    const int32_t tmp = decoder.prev + (byte - BOCU1_MIDDLE);
                    cp = (unichar)tmp;
                    decoder.prev = bocu1_prev(tmp);
                }
                else if (byte == BOCU1_RESET)
                {
                    // No code point is available yet so only reset the state.
                    // Another byte must be read to decode the code point.
                    decoder.prev = BOCU1_ASCII_PREV;
                    state = DECODER_CONTINUE;
                }
                else
                {
                    decode_bocu1_lead_byte(&decoder, byte);
                    state = DECODER_CONTINUE;
                }
            }
            else
            {
                // Trail byte in any position.
                state = decode_bocu1_trail_byte(&decoder, byte, &cp);
            }

            // Check if a code point was decoded or an error occurred.
            if (state == DECODER_OK)
            {
                uni_charbuf_appendchar(&output, (unichar)cp);
            }
            else if (state == DECODER_FATAL_ERROR)
            {
                status = UNI_BAD_ENCODING;
                break;
            }
            else
            {
                // No Action.
            }
        }

        if (status == UNI_OK)
        {
            status = uni_charbuf_finalize(&output);
        }
    }

    return status;
#else
    uni_message("compression disabled");
    return UNI_FEATURE_DISABLED;
#endif
}
