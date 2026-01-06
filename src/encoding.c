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

/*
 *  Fast lookup table for determining how many bytes are in a UTF-8 encoded sequence.
 *  It is based on RFC 3629.
 * 
 *  Using branches, written in pseudocode, it would look like this:
 * 
 *      if (c >= 0) and (c <= 127) return 1
 *      elif (c >= 194) and (c <= 223) return 2
 *      elif (c >= 224) and (c <= 239) return 3
 *      elif (c >= 240) and (c <= 244) return 4
 *      else return 0
 * 
 *  This lookup table will return '0' for continuation bytes, overlong bytes,
 *  and bytes which do not appear in a valid UTF-8 sequence.
 */
static const uint8_t bytes_needed_for_UTF8_sequence[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // Defines bit patterns for masking the leading byte of a UTF-8 sequence.
    0,
    0xFFu, // Single byte (i.e. fits in ASCII).
    0x1F,  // Two byte sequence: 110xxxxx 10xxxxxx.
    0x0F,  // Three byte sequence: 1110xxxx 10xxxxxx 10xxxxxx.
    0x07,  // Four byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx.
};

// See "next_utf8_byte.dot" for a visualization of the DFA.
static const uint8_t unicorn_next_UTF8_DFA[] = {
    0, 12, 24, 36, 60, 96, 84, 12, 12, 12, 48, 72,  // state 0
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, // state 1
    12, 0, 12, 12, 12, 12, 12, 0, 12, 0, 12, 12,    // state 2
    12, 24, 12, 12, 12, 12, 12, 24, 12, 24, 12, 12, // state 3
    12, 12, 12, 12, 12, 12, 12, 24, 12, 12, 12, 12, // state 4
    12, 24, 12, 12, 12, 12, 12, 12, 12, 24, 12, 12, // state 5
    12, 12, 12, 12, 12, 12, 12, 36, 12, 36, 12, 12, // state 6
    12, 36, 12, 12, 12, 12, 12, 36, 12, 36, 12, 12, // state 7
    12, 36, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, // state 8
};

// Character classes for the UTF-8 DFA.
static const uint8_t unicorn_byte_to_character_class[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    8, 8, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    10, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 3, 3,
    11, 6, 6, 6, 5, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
};

#define DFA_ACCEPTANCE_STATE ((uint8_t)0) // The acceptance state for the UTF-8 validator DFA.

static bool is_non_surrogate_bmp(unichar word)
{
    bool is_bmp;
    assert(word <= UNICHAR_C(0xFFFF)); // LCOV_EXCL_BR_LINE
    if (is_low_surrogate(word))
    {
        is_bmp = false;
    }
    else if (is_high_surrogate(word))
    {
        is_bmp = false;
    }
    else
    {
        is_bmp = true;
    }
    return is_bmp;
}

static bool is_valid_scalar(unichar c)
{
    bool is_scalar;
    if (c > UNICORN_LARGEST_CODE_POINT)
    {
        is_scalar = false;
    }
    else if (is_low_surrogate(c))
    {
        is_scalar = false;
    }
    else if (is_high_surrogate(c))
    {
        is_scalar = false;
    }
    else
    {
        is_scalar = true;
    }
    return is_scalar;
}

unisize uni_prev_UTF8_seqlen(const UChar8 *start, unisize offset)
{
    // Move back by one byte so it's pointing to the last byte of the previous sequence.
    const UChar8 *bytes = &start[offset];
    unisize length = 0;
    assert(bytes > start); // LCOV_EXCL_BR_LINE

    // Check if this UTF-8 sequence is only one byte.
    // Code points that fit in one byte have the bit pattern: 0xxxxxxx.
    bytes = &bytes[-1];
    if ((*bytes & (uint8_t)0x80) == (uint8_t)0)
    {
        length = 1;
    }
    // Keep going back a byte until the bit pattern 11xxxxx is found.
    // This indicates the beginning of a UTF-8 sequence.
    else if (bytes > start)
    {
        // Check for a two byte sequence: 110xxxxx 10xxxxxx.
        bytes = &bytes[-1];
        if ((*bytes & (uint8_t)0xE0) == (uint8_t)0xC0)
        {
            length = 2;
        }
        else if (bytes > start)
        {
            // Check for a three byte sequence: 1110xxxx 10xxxxxx 10xxxxxx.
            bytes = &bytes[-1];
            if ((*bytes & (uint8_t)0xF0) == (uint8_t)0xE0)
            {
                length = 3;
            }
            else if (bytes > start)
            {
                // Check for a four byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx.
                bytes = &bytes[-1];
                if ((*bytes & (uint8_t)0xF8) == (uint8_t)0xF0)
                {
                    length = 4;
                }
            }
            else
            {
                // No Action.
            }
        }
        else
        {
            // No Action.
        }
    }
    else
    {
        // No Action.
    }

    return length;
}

unisize unichar_to_u8(unichar codepoint, UChar8 bytes[4])
{
    unisize length;
    if (codepoint <= UNICHAR_C(0x7F))
    {
        bytes[0] = (UChar8)codepoint;
        length = 1;
    }
    else if (codepoint <= UNICHAR_C(0x7FF))
    {
        bytes[0] = (UChar8)(codepoint >> UNICHAR_C(6)) | (uint8_t)0xC0;
        bytes[1] = (UChar8)(codepoint & UNICHAR_C(0x3F)) | (uint8_t)0x80;
        length = 2;
    }
    else if (codepoint <= UNICHAR_C(0xFFFF))
    {
        bytes[0] = (UChar8)(codepoint >> UNICHAR_C(12)) | (uint8_t)0xE0;
        bytes[1] = (UChar8)((codepoint >> UNICHAR_C(6)) & UNICHAR_C(0x3F)) | (uint8_t)0x80;
        bytes[2] = (UChar8)(codepoint & UNICHAR_C(0x3F)) | (uint8_t)0x80;
        length = 3;
    }
    else
    {
        assert(codepoint <= UNICHAR_C(0x10FFFF)); // LCOV_EXCL_BR_LINE
        bytes[0] = (UChar8)(codepoint >> UNICHAR_C(18)) | (uint8_t)0xF0;
        bytes[1] = (UChar8)((codepoint >> UNICHAR_C(12)) & UNICHAR_C(0x3F)) | (uint8_t)0x80;
        bytes[2] = (UChar8)((codepoint >> UNICHAR_C(6)) & UNICHAR_C(0x3F)) | (uint8_t)0x80;
        bytes[3] = (UChar8)(codepoint & UNICHAR_C(0x3F)) | (uint8_t)0x80;
        length = 4;
    }
    return length;
}

unisize unichar_to_u16(unichar codepoint, UChar16 words[2], ByteSwap16 swap)
{
    unisize length = 1;
    if (codepoint <= (unichar)0xFFFF)
    {
        words[0] = swap((UChar16)codepoint);
    }
    else
    {
        const unichar LEAD_OFFSET = (unichar)0xD800 - ((unichar)0x10000 >> (unichar)10);
        words[0] = swap((UChar16)(LEAD_OFFSET + (codepoint >> (unichar)10)));
        words[1] = swap((UChar16)((unichar)0xDC00 + (codepoint & (unichar)0x3FF)));
        length = 2;
    }
    return length;
}

#if defined(UNICORN_FEATURE_ENCODING_UTF8)

static unisize u8_decode_unsafe(const UChar8 *bytes, unichar *scalar)
{
    // Fast but unsafe lookup table for determining how many bytes are in a UTF-8 encoded sequence.
    // The table stores a sequence length of '1' for illegal starter bytes which ensures iterators
    // always advance to the next byte. An alternative design would be to store a sequence length
    // of '0' but that will cause iteration code to enter an infinite loop since there would be
    // no advancement.
    static const int32_t unsafe_utf8_sequence_lengths[] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        // All bytes for the next four rows are continuation bytes.
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        // End of continuation bytes.
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // The first two bytes of this row encode overlong sequences.
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
        4, 4, 4, 4, 4,
                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // These bytes do not appear in valid UTF-8 sequences.
    };
    const unisize bytes_needed = (unisize)unsafe_utf8_sequence_lengths[bytes[0]];
    unichar cp;
    switch (bytes_needed)
    {
    case 1:
        // Single byte (i.e. fits in ASCII).
        cp = (unichar)bytes[0];
        break;
    case 2:
        // Two byte sequence: 110xxxxx 10xxxxxx.
        cp = (unichar)bytes[0] & UNICHAR_C(0x1F);
        cp = (cp << UNICHAR_C(6)) | ((unichar)bytes[1] & UNICHAR_C(0x3F));
        break;
    case 3:
        // Three byte sequence: 1110xxxx 10xxxxxx 10xxxxxx.
        cp = (unichar)bytes[0] & UNICHAR_C(0x0F);
        cp = (cp << UNICHAR_C(6)) | ((unichar)bytes[1] & UNICHAR_C(0x3F));
        cp = (cp << UNICHAR_C(6)) | ((unichar)bytes[2] & UNICHAR_C(0x3F));
        break;
    default:
        assert(bytes_needed == 4); // LCOV_EXCL_BR_LINE
        // Four byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx.
        cp = (unichar)bytes[0] & UNICHAR_C(0x07);
        cp = (cp << UNICHAR_C(6)) | ((unichar)bytes[1] & UNICHAR_C(0x3F));
        cp = (cp << UNICHAR_C(6)) | ((unichar)bytes[2] & UNICHAR_C(0x3F));
        cp = (cp << UNICHAR_C(6)) | ((unichar)bytes[3] & UNICHAR_C(0x3F));
        break;
    }
    *scalar = cp;
    return bytes_needed;
}

static unistat u8_next_unsafe(const void *text, unisize text_length, unisize *offset, unichar *scalar)
{
    // LCOV_EXCL_START
    assert(text != NULL);
    assert(offset != NULL);
    assert(scalar != NULL);
    // LCOV_EXCL_STOP

    unistat status = UNI_OK;

    if ((text_length >= 0) && (*offset >= text_length))
    {
        status = UNI_DONE;
    }
    else
    {
        const UChar8 *bytes = text; // cppcheck-suppress misra-c2012-11.5

        // Check for an empty null-terminated string.
        if ((text_length < 0) && (bytes[*offset] == (uint8_t)0))
        {
            status = UNI_DONE;
        }
        else
        {
            bytes = &bytes[*offset];
            *offset += u8_decode_unsafe(bytes, scalar);
        }
    }

    return status;
}

static unistat u8_next(const void *text, unisize text_length, unisize *offset, unichar *scalar)
{
    // LCOV_EXCL_START
    assert(text != NULL);
    assert(offset != NULL);
    assert(scalar != NULL);
    // LCOV_EXCL_STOP

    unistat status = UNI_OK;
    const UChar8 *start = text; // cppcheck-suppress misra-c2012-11.5
    
    // Check for an empty null-terminated string.
    if ((text_length >= 0) && (*offset >= text_length))
    {
        status = UNI_DONE;
    }
    else if ((text_length < 0) && (start[*offset] == (uint8_t)0))
    {
        status = UNI_DONE;
    }
    else
    {
        // Lookup the total bytes needed to represent this UTF-8 sequence.
        const UChar8 *curr = &start[*offset];
        const unisize byte_count = (unisize)bytes_needed_for_UTF8_sequence[curr[0]];
        if (byte_count == UNISIZE_C(0))
        {
            status = UNI_BAD_ENCODING; // Malformed leading byte.
        }
        else
        {
            // Check if the sequence would extend beyond the length of the string.
            if (text_length >= 0)
            {
                if ((*offset + byte_count) > text_length)
                {
                    status = UNI_BAD_ENCODING;
                }
            }
            else
            {
                for (unisize i = 1; i < byte_count; i++)
                {
                    if (curr[i] == (uint8_t)0)
                    {
                        status = UNI_BAD_ENCODING;
                        break;
                    }
                }
            }

            if (status == UNI_OK)
            {
                // Consume the first byte.
                unichar value = (unichar)curr[0] & (unichar)bytes_needed_for_UTF8_sequence[256 + byte_count];

                // Transition to the first state.
                uint8_t state = unicorn_next_UTF8_DFA[unicorn_byte_to_character_class[curr[0]]];

                // Consume the remaining bytes.
                for (unisize i = 1; i < byte_count; i++)
                {
                    // Mask off the next byte.
                    // It's of the form 10xxxxxx if valid UTF-8.
                    const uint8_t mask = curr[i] & (uint8_t)0x3F;
                    value = (value << UNICHAR_C(6)) | (unichar)mask;

                    // Transition to the next DFA state.
                    state = unicorn_next_UTF8_DFA[(const uint8_t)state + unicorn_byte_to_character_class[curr[i]]];
                }

                // Check for invalid UTF-8.
                if (state == DFA_ACCEPTANCE_STATE)
                {
                    *offset += byte_count;
                    *scalar = value;
                }
                else
                {
                    status = UNI_BAD_ENCODING; // Malformed UTF-8 sequence.
                }
            }
        }
    }

    return status;
}

static unistat u8_prev_unsafe(const void *text, unisize *offset, unichar *cp)
{
    unistat status = UNI_OK;

    // Calculate the byte offset of the current code point.
    const UChar8 *start = text; // cppcheck-suppress misra-c2012-11.5
    if (*offset <= 0)
    {
        status = UNI_DONE;
    }
    else
    {
        // Calculate where the previous code point starts.
        const unisize byte_count = uni_prev_UTF8_seqlen(start, *offset);
        *offset -= byte_count;
        (void)u8_decode_unsafe(&start[*offset], cp);
    }

    return status;
}

static unistat u8_prev(const void *text, unisize *offset, unichar *cp)
{
    unistat status = UNI_OK;

    // Calculate the byte offset of the current code point.
    const UChar8 *start = text; // cppcheck-suppress misra-c2012-11.5
    if (*offset <= 0)
    {
        status = UNI_DONE;
    }
    else
    {
        // Calculate where the previous code point starts.
        const UChar8 *curr = &start[*offset];
        const unisize byte_count = uni_prev_UTF8_seqlen(start, *offset);
        if (byte_count == UNISIZE_C(0))
        {
            status = UNI_BAD_ENCODING;
        }
        else
        {
            // Backup to the previous code point.
            curr = &curr[-byte_count];
            assert(curr >= start); // LCOV_EXCL_BR_LINE

            // Consume the first byte.
            unichar value = (unichar)curr[0] & (unichar)bytes_needed_for_UTF8_sequence[256 + byte_count];

            // Transition to the first state.
            uint8_t state = unicorn_next_UTF8_DFA[unicorn_byte_to_character_class[curr[0]]];

            // Consume the remaining bytes.
            for (unisize i = 1; i < byte_count; i++)
            {
                // Mask off the next byte.
                // It's of the form 10xxxxxx if valid UTF-8.
                const uint8_t mask = curr[i] & (uint8_t)0x3F;
                value = (value << UNICHAR_C(6)) | (unichar)mask;

                // Transition to the next DFA state.
                state = unicorn_next_UTF8_DFA[(const uint8_t)state + unicorn_byte_to_character_class[curr[i]]];
            }

            // Check for invalid UTF-8.
            if (state == DFA_ACCEPTANCE_STATE)
            {
                *offset -= (unisize)byte_count;
                *cp = value;
            }
            else
            {
                status = UNI_BAD_ENCODING; // Malformed UTF-8 sequence.
            }
        }
    }

    return status;
}

#endif

#if defined(UNICORN_FEATURE_ENCODING_UTF16)

// For converting UTF-16 to UTF-32.
static const unichar SURROGATE_OFFSET = (unichar)0x10000 - ((unichar)0xD800 << (unichar)10) - (unichar)0xDC00;

static unistat u16_next(const void *text, unisize text_length, unisize *offset, unichar *scalar, ByteSwap16 swap)
{
    // LCOV_EXCL_START
    assert(text != NULL);
    assert(offset != NULL);
    assert(scalar != NULL);
    // LCOV_EXCL_STOP

    unistat status = UNI_OK;
    const UChar16 *words = text; // cppcheck-suppress misra-c2012-11.5

    // Check for the end of the string.
    if ((text_length >= 0) && (*offset >= text_length))
    {
        status = UNI_DONE;
    }
    else if ((text_length < 0) && (words[*offset] == (uint16_t)0x0000))
    {
        status = UNI_DONE;
    }
    else
    {
        // Extract the current word.
        const unisize word_offset = *offset;
        const unichar word = (unichar)swap(words[word_offset]);

        // Check if this UTF-16 character is in the Basic Multilingual Plane.
        if (is_non_surrogate_bmp(word))
        {
            (*offset) += 1;
            (*scalar) = word;
        }
        else
        {
            // A High Surrogate must be followed by a Low Surrogate.
            // Check if there is room for a subsequent 16-bit word in the string.
            if ((text_length >= 0) && ((word_offset + 1) >= text_length))
            {
                status = UNI_BAD_ENCODING;
            }
            else if (words[word_offset + 1] == (uint16_t)0x0)
            {
                status = UNI_BAD_ENCODING;
            }
            else
            {
                // If the word is not in the Basic Multilingual Plane, then it must be a High Surrogate.
                // If it's not a High Surrogate, then this isn't valid UTF-16.
                const unichar next_word = (unichar)swap(words[word_offset + 1]);
                if (!is_high_surrogate(word))
                {
                    status = UNI_BAD_ENCODING;
                }
                // Verify the next word is a Low Surrogate.
                else if (!is_low_surrogate(next_word))
                {
                    status = UNI_BAD_ENCODING;
                }
                else
                {
                    (*offset) += 2;
                    (*scalar) = (word << 10) + next_word + SURROGATE_OFFSET;
                }
            }
        }
    }
    return status;
}

static unistat u16_next_unsafe(const void *text, unisize text_length, unisize *offset, unichar *scalar, ByteSwap16 swap)
{
    // LCOV_EXCL_START
    assert(text != NULL);
    assert(offset != NULL);
    assert(scalar != NULL);
    assert(swap != NULL);
    // LCOV_EXCL_STOP

    unistat status = UNI_OK;
    const UChar16 *words = text; // cppcheck-suppress misra-c2012-11.5
    words = &words[*offset];

    // Check for the end of the string.
    if ((text_length >= 0) && (*offset >= text_length))
    {
        status = UNI_DONE;
    }
    else if ((text_length < 0) && (words[0] == (uint16_t)0x0))
    {
        status = UNI_DONE;
    }
    else
    {
        const unichar hi = (unichar)swap(words[0]);
        if (is_high_surrogate(hi))
        {
            const unichar lo = (unichar)swap(words[1]);
            *scalar = (hi << 10) + (unichar)lo + SURROGATE_OFFSET;
            *offset += 2;
        }
        else
        {
            *scalar = hi;
            *offset += 1;
        }
    }

    return status;
}

static unistat u16_prev_unsafe(const void *text, unisize *offset, unichar *scalar, ByteSwap16 swap)
{
    unistat status = UNI_OK;
    const UChar16 *words = text; // cppcheck-suppress misra-c2012-11.5
    words = &words[*offset];

    if (*offset == 0)
    {
        status = UNI_DONE;
    }
    else
    {
        const unichar lo = (unichar)swap(words[-1]);
        if (is_low_surrogate(lo))
        {
            const unichar hi = (unichar)swap(words[-2]);
            *scalar = (hi << 10) + (unichar)lo + SURROGATE_OFFSET;
            *offset -= 2;
        }
        else
        {
            *scalar = lo;
            *offset -= 1;
        }
    }

    return status;
}

static unistat u16_prev(const void *text, unisize *offset, unichar *cp, ByteSwap16 swap)
{
    unistat status = UNI_OK;

    // Check if the end of iteration has been reached.
    if (*offset <= 0)
    {
        status = UNI_DONE;
    }
    else
    {
        // Extract the current word.
        const UChar16 *words = text; // cppcheck-suppress misra-c2012-11.5
        const unisize word_end = *offset;
        unisize word_start = word_end - 1;
        const unichar word = (unichar)swap(words[word_start]);

        // If it's in the Basic Multilingual Plane, then it's a valid code point.
        if (is_non_surrogate_bmp(word))
        {
            *offset -= 1;
            *cp = word;
        }
        else
        {
            // A Low Surrogate must be succeed a High Surrogate.
            // Check if there is room for a precede 16-bit word in the string.
            if (word_start == 0)
            {
                status = UNI_BAD_ENCODING;
            }
            else
            {
                const unichar prev_word = (unichar)swap(words[word_start - 1]);

                // If the word is not in the Basic Multilingual Plane, then it must be a Low Surrogate
                // otherwise this isn't valid UTF-16.
                if (!is_low_surrogate(word))
                {
                    status = UNI_BAD_ENCODING;
                }
                // Verify the preceding word is a High Surrogate.
                else if (!is_high_surrogate(prev_word))
                {
                    status = UNI_BAD_ENCODING;
                }
                else
                {
                    *offset -= 2;
                    *cp = (prev_word << 10) + word + SURROGATE_OFFSET;
                }
            }
        }
    }

    return status;
}

#endif

#if defined(UNICORN_FEATURE_ENCODING_UTF32)

static unistat u32_next(const void *text, unisize text_length, unisize *offset, unichar *scalar, ByteSwap32 swap)
{
    // LCOV_EXCL_START
    assert(text != NULL);
    assert(offset != NULL);
    assert(scalar != NULL);
    assert(swap != NULL);
    // LCOV_EXCL_STOP

    unistat status = UNI_OK;

    if ((text_length >= 0) && (*offset >= text_length))
    {
        status = UNI_DONE;
    }
    else
    {
        const UChar32 *chars = text; // cppcheck-suppress misra-c2012-11.5
        const unichar cp = (unichar)swap(chars[*offset]);
        if ((text_length < 0) && (cp == UNICHAR_C(0x0)))
        {
            status = UNI_DONE;
        }
        else if (!is_valid_scalar(cp))
        {
            status = UNI_BAD_ENCODING;
        }
        else
        {
            *offset += 1;
            *scalar = cp;
        }
    }

    return status;
}

static unistat u32_next_unsafe(const void *text, unisize text_length, unisize *offset, unichar *scalar, ByteSwap32 swap)
{
    // LCOV_EXCL_START
    assert(text != NULL);
    assert(offset != NULL);
    assert(scalar != NULL);
    assert(swap != NULL);
    // LCOV_EXCL_STOP

    unistat status = UNI_OK;

    if ((text_length >= 0) && (*offset >= text_length))
    {
        status = UNI_DONE;
    }
    else
    {
        const UChar32 *chars = text; // cppcheck-suppress misra-c2012-11.5
        const unichar cp = (unichar)swap(chars[*offset]);
        if ((text_length < 0) && (cp == UNICHAR_C(0x0)))
        {
            status = UNI_DONE;
        }
        else
        {
            *offset += 1;
            *scalar = cp;
        }
    }
    return status;
}

static unistat u32_prev(const void *text, unisize *offset, unichar *scalar, ByteSwap32 swap)
{
    unistat status = UNI_OK;
    if (*offset == 0)
    {
        status = UNI_DONE;
    }
    else
    {
        const UChar32 *chars = text; // cppcheck-suppress misra-c2012-11.5
        const unichar cp = (unichar)swap(chars[(*offset) - 1]);
        if (!is_valid_scalar(cp))
        {
            status = UNI_BAD_ENCODING;
        }
        else
        {
            *offset -= 1;
            *scalar = cp;
        }
    }
    return status;
}

static unistat u32_prev_unsafe(const void *text, unisize *offset, unichar *scalar, ByteSwap32 swap)
{
    unistat status = UNI_OK;
    if (*offset == 0)
    {
        status = UNI_DONE;
    }
    else
    {
        const UChar32 *chars = text; // cppcheck-suppress misra-c2012-11.5
        *scalar = (unichar)swap(chars[(*offset) - 1]);
        *offset -= 1;
    }
    return status;
}

#endif

static unistat scalar_next_unsafe(const void *text, unisize text_length, unisize *offset, unichar *scalar)
{
    // LCOV_EXCL_START
    assert(text != NULL);
    assert(offset != NULL);
    assert(scalar != NULL);
    // LCOV_EXCL_STOP

    unistat status = UNI_OK;

    if ((text_length >= 0) && (*offset >= text_length))
    {
        status = UNI_DONE;
    }
    else
    {
        const unichar *chars = text; // cppcheck-suppress misra-c2012-11.5
        const unichar cp = chars[*offset];
        if ((text_length < 0) && (cp == UNICHAR_C(0x0)))
        {
            status = UNI_DONE;
        }
        else
        {
            *offset += 1;
            *scalar = cp;
        }
    }
    return status;
}

static unistat scalar_prev_unsafe(const void *text, unisize *offset, unichar *scalar)
{
    unistat status = UNI_OK;
    if (*offset == 0)
    {
        status = UNI_DONE;
    }
    else
    {
        const unichar *chars = text; // cppcheck-suppress misra-c2012-11.5
        const unichar cp = chars[(*offset) - 1];
        *offset -= 1;
        *scalar = cp;
    }
    return status;
}

static unistat scalar_next(const void *text, unisize text_length, unisize *offset, unichar *scalar)
{
    // LCOV_EXCL_START
    assert(text != NULL);
    assert(offset != NULL);
    assert(scalar != NULL);
    // LCOV_EXCL_STOP

    unistat status = UNI_OK;

    if ((text_length >= 0) && (*offset >= text_length))
    {
        status = UNI_DONE;
    }
    else
    {
        const unichar *chars = text; // cppcheck-suppress misra-c2012-11.5
        const unichar cp = chars[*offset];
        if ((text_length < 0) && (cp == UNICHAR_C(0x0)))
        {
            status = UNI_DONE;
        }
        else if (!is_valid_scalar(cp))
        {
            status = UNI_BAD_ENCODING;
        }
        else
        {
            *offset += 1;
            *scalar = cp;
        }
    }

    return status;
}

static unistat scalar_prev(const void *text, unisize *offset, unichar *scalar)
{
    unistat status = UNI_OK;
    if (*offset == 0)
    {
        status = UNI_DONE;
    }
    else
    {
        const unichar *chars = text; // cppcheck-suppress misra-c2012-11.5
        const unichar cp = chars[(*offset) - 1];
        if (!is_valid_scalar(cp))
        {
            status = UNI_BAD_ENCODING;
        }
        else
        {
            *offset -= 1;
            *scalar = cp;
        }
    }
    return status;
}

UNICORN_API unistat uni_next(const void *text, unisize text_len, uniattr text_attr, unisize *index, unichar *cp)
{
    ByteSwap16 swap16 = NULL;
    ByteSwap32 swap32 = NULL;
    unistat status = UNI_OK;

    if (index == NULL)
    {
        uni_message("required argument 'index' is null");
        status = UNI_BAD_OPERATION;
    }
    else if (cp == NULL)
    {
        uni_message("required argument 'cp' is null");
        status = UNI_BAD_OPERATION;
    }
    else
    {
        status = uni_check_input_encoding(text, text_len, &text_attr);
    }

    if (status == UNI_OK)
    {
        switch (GET_ENCODING(text_attr)) // LCOV_EXCL_BR_LINE
        {
        case UNI_UTF8:
#if defined(UNICORN_FEATURE_ENCODING_UTF8)
            if ((text_attr & UNI_TRUST) == UNI_TRUST)
            {
                status = u8_next_unsafe(text, text_len, index, cp);
            }
            else
            {
                status = u8_next(text, text_len, index, cp);
            }
#else
            uni_message("UTF-8 encoding form disabled");
            status = UNI_FEATURE_DISABLED;
#endif
            break;

        case UNI_UTF16:
#if defined(UNICORN_FEATURE_ENCODING_UTF16)
            swap16 = ((text_attr & UNI_LITTLE) == UNI_LITTLE) ? &uni_swap16_le : &uni_swap16_be;
            if ((text_attr & UNI_TRUST) == UNI_TRUST)
            {
                status = u16_next_unsafe(text, text_len, index, cp, swap16);
            }
            else
            {
                status = u16_next(text, text_len, index, cp, swap16);
            }
#else
            uni_message("UTF-16 encoding form disabled");
            status = UNI_FEATURE_DISABLED;
#endif
            break;

        case UNI_UTF32:
#if defined(UNICORN_FEATURE_ENCODING_UTF32)
            swap32 = ((text_attr & UNI_LITTLE) == UNI_LITTLE) ? &uni_swap32_le : &uni_swap32_be;
            if ((text_attr & UNI_TRUST) == UNI_TRUST)
            {
                status = u32_next_unsafe(text, text_len, index, cp, swap32);
            }
            else
            {
                status = u32_next(text, text_len, index, cp, swap32);
            }
#else
            uni_message("UTF-32 encoding form disabled");
            status = UNI_FEATURE_DISABLED;
#endif
            break;

        case UNI_SCALAR:
            if ((text_attr & UNI_TRUST) == UNI_TRUST)
            {
                status = scalar_next_unsafe(text, text_len, index, cp);
            }
            else
            {
                status = scalar_next(text, text_len, index, cp);
            }
            break;

        // LCOV_EXCL_START: This code path is tested via feature configuration tests.
        default:
            UNREACHABLE; // cppcheck-suppress premium-misra-c-2012-17.3
            status = UNI_MALFUNCTION;
            break;
        // LCOV_EXCL_STOP
        }
    }
    return status;
}

UNICORN_API unistat uni_prev(const void *text, unisize text_len, uniattr text_attr, unisize *index, unichar *cp)
{
    ByteSwap16 swap16 = NULL;
    ByteSwap32 swap32 = NULL;
    unistat status = UNI_OK;
    
    // The 'length' parameter is unused in the implementation of decode previous.
    // It is provided as part of the function signature so it matches the signature of decode next.
    (void)text_len;

    if (index == NULL)
    {
        uni_message("required argument 'index' is null");
        status = UNI_BAD_OPERATION;
    }
    else if (cp == NULL)
    {
        uni_message("required argument 'cp' is null");
        status = UNI_BAD_OPERATION;
    }
    else
    {
        status = uni_check_input_encoding(text, text_len, &text_attr);
    }

    if (status == UNI_OK)
    {
        switch (GET_ENCODING(text_attr)) // LCOV_EXCL_BR_LINE
        {
        case UNI_UTF8:
#if defined(UNICORN_FEATURE_ENCODING_UTF8)
            if ((text_attr & UNI_TRUST) == UNI_TRUST)
            {
                status = u8_prev_unsafe(text, index, cp);
        }
            else
            {
                status = u8_prev(text, index, cp);
            }
#else
            status = UNI_FEATURE_DISABLED;
            uni_message("UTF-8 encoding form disabled");
#endif
            break;

        case UNI_UTF16:
#if defined(UNICORN_FEATURE_ENCODING_UTF16)
            swap16 = ((text_attr & UNI_LITTLE) == UNI_LITTLE) ? &uni_swap16_le : &uni_swap16_be;
            if ((text_attr & UNI_TRUST) == UNI_TRUST)
            {
                status = u16_prev_unsafe(text, index, cp, swap16);
            }
            else
            {
                status = u16_prev(text, index, cp, swap16);
            }
#else
            status = UNI_FEATURE_DISABLED;
            uni_message("UTF-16 encoding form disabled");
#endif
            break;

        case UNI_UTF32:
#if defined(UNICORN_FEATURE_ENCODING_UTF32)
            swap32 = ((text_attr & UNI_LITTLE) == UNI_LITTLE) ? &uni_swap32_le : &uni_swap32_be;
            if ((text_attr & UNI_TRUST) == UNI_TRUST)
            {
                status = u32_prev_unsafe(text, index, cp, swap32);
            }
            else
            {
                status = u32_prev(text, index, cp, swap32);
            }
#else
            status = UNI_FEATURE_DISABLED;
            uni_message("UTF-32 encoding form disabled");
#endif
            break;

        case UNI_SCALAR:
            if ((text_attr & UNI_TRUST) == UNI_TRUST)
            {
                status = scalar_prev_unsafe(text, index, cp);
            }
            else
            {
                status = scalar_prev(text, index, cp);
            }
            break;

        // LCOV_EXCL_START: This code path is tested via feature configuration tests.
        default:
            UNREACHABLE; // cppcheck-suppress premium-misra-c-2012-17.3
            status = UNI_MALFUNCTION;
            break;
        // LCOV_EXCL_STOP
        }
    }
    return status;
}

UNICORN_API unistat uni_encode(unichar cp, void *dst, unisize *dst_len, uniattr dst_attr) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
    unistat status = UNI_OK;
    struct CharBuf buffer = {NULL};

    if (!is_valid_scalar(cp))
    {
        status = UNI_BAD_OPERATION;
    }

    if (status == UNI_OK)
    {
        status = uni_charbuf_init(&buffer, dst, dst_len, dst_attr);
        if (status == UNI_OK)
        {
            uni_charbuf_appendchar(&buffer, cp);
            status = uni_charbuf_finalize(&buffer);
        }
    }

    return status;
}

UNICORN_API unistat uni_convert(const void *src, unisize src_len, uniattr src_attr, void *dst, unisize *dst_len, uniattr dst_attr) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
    unistat status = UNI_OK;

    if (src == NULL)
    {
        uni_message("required argument is null");
        status = UNI_BAD_OPERATION;
    }

    if (status == UNI_OK)
    {
        status = uni_check_input_encoding(src, src_len, &src_attr);
    }

    if (status == UNI_OK)
    {
        struct CharBuf buffer = {NULL};
        status = uni_charbuf_init(&buffer, dst, dst_len, dst_attr);
        if (status == UNI_OK)
        {
            unisize i = 0;
            for (;;)
            {
                unichar cp;
                status = uni_next(src, src_len, src_attr, &i, &cp);
                if (status == UNI_OK)
                {
                    uni_charbuf_appendchar(&buffer, cp);
                }
                else
                {
                    break;
                }
            }

            if (status == UNI_DONE)
            {
                status = uni_charbuf_finalize(&buffer);
            }
        }
    }

    return status;
}

UNICORN_API unistat uni_validate(const void *text, unisize text_len, uniattr text_attr) // cppcheck-suppress misra-c2012-8.7 ; This is supposed to have external linkage.
{
    unistat status = UNI_OK;

    if (text == NULL)
    {
        uni_message("required argument is null");
        status = UNI_BAD_OPERATION;
    }

    if (status == UNI_OK)
    {
        status = uni_check_input_encoding(text, text_len, &text_attr);
    }

    if (status == UNI_OK)
    {
        // The 'trust' flag is useless for this function because its entire
        // purpose is to verify that the input text is well-formed therefore
        // assuming it is well formed via the 'trust' flag defeats this purpose.
        if ((text_attr & UNI_TRUST) == UNI_TRUST)
        {
            uni_message("trust flag is self-defeating");
            status = UNI_BAD_OPERATION;
        }
    }

    if (status == UNI_OK)
    {
        unisize i = 0;
        for (;;)
        {
            unichar cp;
            status = uni_next(text, text_len, text_attr, &i, &cp);
            if (status != UNI_OK)
            {
                break;
            }
        }

        if (status == UNI_DONE)
        {
            status = UNI_OK;
        }
    }
    return status;
}
