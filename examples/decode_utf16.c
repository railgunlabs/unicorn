/*
 *  This file is public domain.
 *  You may use, modify, and distribute it without restriction.
 */

// Examples are also documented online at:
// <https://railgunlabs.com/unicorn/manual/code-examples/>.

#include <unicorn.h>
#include <stdio.h>
#include <uchar.h>

int main(int argc, char *argv[])
{
    // This example demonstrates decoding the code points of a UTF-16 encoded string.
    // It prints each code point on its own line.

    const char16_t str[] = u"I 🕵️."; // I spy
    unisize i = 0;
    for (;;)
    {
        unichar cp = 0x0;
        unistat s = uni_next(str, -1, UNI_UTF16, &i, &cp);
        if (s == UNI_OK)
        {
            printf("U+%04X\n", cp);
        }
        else if (s == UNI_DONE)
        {
            break;
        }
        else if (s == UNI_BAD_ENCODING)
        {
            printf("malformed character at code unit %d\n", i);
            return 1;
        }
        else
        {
            puts("an internal error occured");
            return 2; // Something else went wrong (check the status code).
        }
    }
    return 0;
}
