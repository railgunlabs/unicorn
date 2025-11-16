/*
 *  This file is public domain.
 *  You may use, modify, and distribute it without restriction.
 */

#include <unicorn.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// This code example demonstrates case folding strings. Case folding
// transform strings for the purposes of caseless string comparison.
// 
// You are not supposed to display case folded strings to the end-user.
// They for internal caseless string comparisons only.

int main(int argc, char *argv[])
{
    const char *src1 = u8"Straße";
    const char *src2 = u8"STrasse";

    char dst1[16];
    char dst2[16];

    unisize dst1_len = 16;
    unisize dst2_len = 16;

    bool is_equal = false;
    
    // The following lines case fold two strings and then compares the results.
    // This more manual approach is recommended when you'll be comparing the same
    // strings over and over again because you can case fold them _once_ and then
    // re-use the results for each subsequent comparison.
    if (uni_casefold(UNI_CANONICAL, src1, -1, UNI_UTF8, dst1, &dst1_len, UNI_UTF8 | UNI_NULIFY) == UNI_OK)
    {
        if (uni_casefold(UNI_CANONICAL, src2, -1, UNI_UTF8, dst2, &dst2_len, UNI_UTF8 | UNI_NULIFY) == UNI_OK)
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

    // The following function case folds two strings, performs the comparison, and 
    // reports the result. This approach is recommended when two strings are being
    // compared in a one-off comparison check. If you'll be repeatedly comparing
    // the same strings over and over again, then the approach above is preferred.
    if (uni_casefoldcmp(UNI_CANONICAL, src1, -1, UNI_UTF8, src2, -1, UNI_UTF8, &is_equal) == UNI_OK)
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
    return 0;
}
