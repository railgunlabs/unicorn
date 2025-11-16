/*
 *  This file is public domain.
 *  You may use, modify, and distribute it without restriction.
 */

// Examples are also documented online at:
// <https://railgunlabs.com/unicorn/manual/code-examples/>.

#include <unicorn.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    // This example demonstrates how to encode a code point as UTF-8, UTF-16, and UTF-32.
    // After encoding, the code units are printed as hexadecimal.

    // The following variable defines the Unicode code point that will be encoded.
    // In this example, it's Unicode character U+0300. Change this value to a
    // different character and notice how the printed code units change.
    unichar cp = 0x1F600;

    printf("Code point: U+%04X\n", cp);

    //
    // Encode the code point as UTF-8. UTF-8 is a variable length encoding and will encode
    // to either 1, 2, 3, or 4 code units.
    //

    uint8_t u8[4];
    unisize u8_len = 4;

    if (uni_encode(cp, u8, &u8_len, UNI_UTF8) == UNI_OK)
    {
        printf("UTF-8  :");
        for (unisize i = 0; i < u8_len; i++)
        {
            printf(" 0x%02X", u8[i]);
        }
        putchar('\n');
    }

    //
    // Encode the code point as UTF-16. UTF-16 is a variable length encoding and will encode
    // to either 1 or 2 code units.
    //

    uint16_t u16[2];
    unisize u16_len = 2;

    if (uni_encode(cp, u16, &u16_len, UNI_UTF16) == UNI_OK)
    {
        printf("UTF-16 :");
        for (unisize i = 0; i < u16_len; i++)
        {
            printf(" 0x%04X", u16[i]);
        }
        putchar('\n');
    }

    //
    // Encode the code point as UTF-32. Code points are 21-bit integers and UTF-32 stores
    // them in a 32-bit integer. UTF-32 is not variable length, unlike UTF-8 and UTF-16.
    //

    uint32_t u32[1];
    unisize u32_len = 1;

    if (uni_encode(cp, u32, &u32_len, UNI_UTF32) == UNI_OK)
    {
        printf("UTF-32 :");
        for (unisize i = 0; i < u32_len; i++)
        {
            printf(" 0x%08X", u32[i]);
        }
        putchar('\n');
    }

    return 0;
}
