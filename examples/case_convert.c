/*
 *  This file is public domain.
 *  You may use, modify, and distribute it without restriction.
 */

#include <unicorn.h>
#include <stdio.h>

// This code example case converts text to upper case, e.g. it would
// convert the text "hello world" to "HELLO WORLD". Unicorn also
// supports lower and title case conversion.

// This example does NOT include error checking!

int main(int argc, char *argv[])
{
    const char *src = u8"Straße";

    char dest[64];
    unisize dest_len = 64;

    // The following function call performs the case conversion.
    // Here's what each argument means:
    //
    // UNI_UPPER :
    //   Specifies we want upper case conversion (as opposed to
    //   lower or upper case conversion).
    //
    // src :
    //   This text to be upper cased (i.e. the input text). 
    //
    // -1 :
    //   A negative value indicates the input text is null terminated
    //   which is true in this example. You can also explictly pass
    //   in the number of code units in the input text.
    //
    // UNI_UTF8 :
    //   The input is UTF-8 encoded.
    //
    // dest :
    //   The buffer the upper-cased text will be written to (i.e. the
    //   output or destination buffer).
    //
    // dest_len :
    //   This must be set to the length of the destination buffer when
    //   the function is called. The function, internally, update it
    //   to the number of code units written to the destination buffer.
    //   This is useful for when the output is NOT null terminated.
    //
    // UNI_UTF8 | UNI_NULIFY :
    //   Specifies the result (the case converted text) should be written
    //   to the destination buffer in the UTF-8 encoding and with a null
    //   terminator character.
    uni_caseconv(UNI_UPPER, src, -1, UNI_UTF8, dest, &dest_len, UNI_UTF8 | UNI_NULIFY);
    printf("Case converted: %s\n", dest);

    // If you don't know how big to make the destination buffer, you can
    // call uni_caseconv() with a null and zero-sized destination buffer.
    // The implementation will write to 'dest_len' the number of code
    // units needed to accommodate the output. You can allocate a
    // buffer with the appropriate size and call it again.
    dest_len = 0;
    uni_caseconv(UNI_UPPER, src, -1, UNI_UTF8, NULL, &dest_len, UNI_UTF8 | UNI_NULIFY);
    printf("Code units needed: %d\n", dest_len);
    return 0;
}
