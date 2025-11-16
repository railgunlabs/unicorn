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
    // This code example compresses text using the BOCU-1 compression algorithm.
    // This algorithm is specifically designed for the compression of short
    // Unicode text. Once strings become longer, then a general purpose
    // compressor is preferable. 

    const char src[] = u8"素早い茶色のキツネが怠け者の犬を飛び越えます";

    char buf[64];
    size_t buf_len = 64;

    // The following function compresses the input text 'src' and writes the
    // result to 'buf'. It's function signature is documented online at:
    // <http://localhost:8080/unicorn/manual/api/compression/uni-compress/>.
    if (uni_compress(src, -1, UNI_UTF8, buf, &buf_len) == UNI_OK)
    {
        printf("Uncompressed : %ld bytes\n", sizeof(src));
        printf("Compressed   : %ld bytes\n", buf_len);
    }
    return 0;
}
