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
    // This code example case converts text to upper case, e.g. it would
    // convert the text "hello world" to "HELLO WORLD". Unicorn also
    // supports lower and title case conversion.

    const char *src = u8"Straße";

    char dest[64];
    unisize dest_len = 64;

    // The following function case converts a string to uppercase.
    // The resulting string can be presented to an end-user.
    // The signature for the function is documented online at:
    // <https://railgunlabs.com/unicorn/manual/api/case-mapping/uni-caseconv/>.
    if (uni_caseconv(UNI_UPPER, src, -1, UNI_UTF8, dest, &dest_len, UNI_UTF8 | UNI_NULIFY) == UNI_OK)
    {
        printf("Case converted: %s\n", dest);
    }

    // If you don't know how big the destination buffer will be, you can first
    // call uni_caseconv() with a null and zero-sized destination buffer.
    // When called this way, the implementation will write to 'dest_len' the
    // number of code units needed to accommodate the output. You can then
    // allocate a buffer with the appropriate size and call the function again.
    dest_len = 0;
    if (uni_caseconv(UNI_UPPER, src, -1, UNI_UTF8, NULL, &dest_len, UNI_UTF8 | UNI_NULIFY) == UNI_OK)
    {
        printf("Code units needed: %d\n", dest_len);
    }
    return 0;
}
