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
    // This example prints the code unit indices of the extended grapheme clusters in the string.
    // A grapheme cluster is a user-perceived character. It correspond to a single graphical
    // character on the end-users display. A grapheme can consist of multiple code points.

    // Change 'UNI_GRAPHEME' to 'UNI_WORD' or 'UNI_SENTENCE' and observe how it affects the
    // printed break positions.

    const char str[] = u8"👨🏼‍🚀👨🏽‍🚀 landed on the 🌕";
    unisize break_at = 0;
    for (;;)
    {
        unistat s = uni_nextbrk(UNI_GRAPHEME, str, -1, UNI_UTF8, &break_at);
        if (s == UNI_OK)
        {
            printf("%d\n", break_at);
        }
        else if (s == UNI_DONE)
        {
            break;
        }
        else if (s == UNI_BAD_ENCODING)
        {
            printf("malformed character at %d\n", break_at);
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
