/*
 *  This file is public domain.
 *  You may use, modify, and distribute it without restriction.
 */

// Examples are also documented online at:
// <https://railgunlabs.com/unicorn/manual/code-examples/>.

#include <unicorn.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

int main(int argc, char *argv[])
{
    // This code example demonstrates comparing strings for canonical equivalence.
    // Canonical equivalence means the graphemes are compared, not the code points
    // or code units. This twp of comparison is necessary when strings contain
    // precomposed and decomposed characters.

    const char *src1 = u8"ma\u0301scara"; // 'a' + U+0301 = á  (decomposed)
    const char *src2 = u8"m\u00E1scara";  //       U+00E1 = á  (precomposed)

    char dst1[16];
    char dst2[16];

    unisize dst1_len = 16;
    unisize dst2_len = 16;

    bool is_equal = false;
    
    // The following function normalizes two strings, performs the comparison, and 
    // reports the result. This approach is recommended when two strings are being
    // compared in a one-off comparison check. If you'll be repeatedly comparing
    // the same strings over and over again, then the approach below is preferred.
    if (uni_normcmp(src1, -1, UNI_UTF8, src2, -1, UNI_UTF8, &is_equal) == UNI_OK)
    {
        if (is_equal)
        {
            puts("equal");
        }
        else
        {
            puts("not equal");
        }
    }

    // The following lines normalizes two strings and then compares the results.
    // This more manual approach is recommended when you'll be comparing the same
    // strings over and over again because you can normalize them _once_ and then
    // re-use the results for each subsequent comparison.
    if (uni_norm(UNI_NFD, src1, -1, UNI_UTF8, dst1, &dst1_len, UNI_UTF8 | UNI_NULIFY) == UNI_OK)
    {
        if (uni_norm(UNI_NFD, src2, -1, UNI_UTF8, dst2, &dst2_len, UNI_UTF8 | UNI_NULIFY) == UNI_OK)
        {
            if (strcmp(dst1, dst2) == 0)
            {
                puts("equal");
            }
            else
            {
                puts("not equal");
            }
        }
    }
    return 0;
}
