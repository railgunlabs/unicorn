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
    // This code example demonstrates how to collate strings (compare strings for sorting).
    // The online documentation is available here:
    // <https://railgunlabs.com/unicorn/manual/api/collation/>.

    const char *s1 = u8"Hello World";
    const char *s2 = u8"こんにちは世界";

    uint16_t sk1[64] = {0};
    uint16_t sk2[64] = {0};

    size_t sk1_len = 64;
    size_t sk2_len = 64;

    int32_t result = 0;
    
    // The following snippet collates two strings and prints the result of the comparison.
    // This function, conceptually, builds sort keys for the input strings and compares them.
    // Building a sort key is expensive. If strings are collated multiple times, it is recommend
    // to build the sort keys once and use them for subsequent comparisons.
    if (uni_collate(s1, -1, UNI_UTF8, s2, -1, UNI_UTF8, UNI_NON_IGNORABLE, UNI_PRIMARY, &result) == UNI_OK)
    {
        if (result < 0)
        {
            puts("s1 < s2");
        }
        else if (result > 0)
        {
            puts("s1 > s2");
        }
        else
        {
            puts("s1 = s2");
        }
    }

    // The following snippet collates strings the manual way. The first step is to
    // generate a "sort key" for both strings. The sort key encodes the sorting
    // weights of the string.
    // 
    // Constructing a sort key is the computationally expensive part of the process.
    // Once the sort keys are constructed, they are compared which is inexpensive.
    if (uni_sortkeymk(s1, -1, UNI_UTF8, UNI_NON_IGNORABLE, UNI_PRIMARY, sk1, &sk1_len) == UNI_OK)
    {
        if (uni_sortkeymk(s2, -1, UNI_UTF8, UNI_NON_IGNORABLE, UNI_PRIMARY, sk2, &sk2_len) == UNI_OK)
        {
            if (uni_sortkeycmp(sk1, sk1_len, sk2, sk2_len, &result) == UNI_OK)
            {
                if (result < 0)
                {
                    puts("s1 < s2");
                }
                else if (result > 0)
                {
                    puts("s1 > s2");
                }
                else
                {
                    puts("s1 = s2");
                }
            }
        }
    }
    return 0;
}
