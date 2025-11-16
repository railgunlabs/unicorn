/*
 *  This file is public domain.
 *  You may use, modify, and distribute it without restriction.
 */

#include <unicorn.h>
#include <stdio.h>

// This code example case converts text to upper case, e.g. it would
// convert the text "hello world" to "HELLO WORLD". Unicorn also
// supports lower and title case conversion.

int main(int argc, char *argv[])
{
    const char *src = u8"Straße";

    char dest[64];
    unisize dest_len = 64;

    // The following function call performs the case conversion.
    // It's documented online at:
    // <https://railgunlabs.com/unicorn/manual/api/case-mapping/uni-caseconv/>.
    //
    // In this example, here's what each argument means:
    //
    // UNI_UPPER :
    //   Specifies we want upper case conversion (as opposed to
    //   lower or title case conversion).
    //
    // src :
    //   The text to be upper cased (i.e. the input text). 
    //
    // -1 :
    //   A negative value indicates the input text is null terminated
    //   which is true for this example. You can explictly pass the
    //   number of code units if preferred (this is required when the
    //   input is not null terminated).
    //
    // UNI_UTF8 :
    //   The input is UTF-8 encoded.
    //
    // dest :
    //   This is output or destination buffer (i.e. the buffer the
    //   upper cased text will be written to).
    //
    // dest_len :
    //   This must be set to the length of the destination buffer when
    //   the function is called. The implementation writes to it the
    //   number of code units written to the destination buffer.
    //   It's neccessary to know how many code units were were
    //   written when the output is not null terminated.
    //
    // UNI_UTF8 | UNI_NULIFY :
    //   Bit flags that specify the resulting case converted text should
    //   be written as UTF-8 and with a null terminator.
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
