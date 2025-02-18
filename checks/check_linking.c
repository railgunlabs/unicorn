/*
 *  Unicorn - Embeddable Unicode Library
 *  Copyright (c) 2021-2025 Railgun Labs
 *
 *  This file is part of Unicorn, distributed under a non-commercial use software
 *  license. For the full terms see the included LICENSE file. If you did not
 *  receive a LICENSE file or you would like to purchase a commercial license,
 *  contact us at <https://RailgunLabs.com/contact/>.
 */

#include "unicorn.h"
#include <stddef.h>

int main(int argc, char *argv[])
{
    uni_getversion(NULL, NULL, NULL);
    uni_getucdversion(NULL, NULL, NULL);
    uni_setmemfunc(NULL, NULL);
    uni_seterrfunc(NULL, NULL);

    uni_caseconv(UNI_UPPER, NULL, -1, UNI_UTF8, NULL, NULL, UNI_UTF8);
    uni_caseconvchk(UNI_UPPER, NULL, -1, UNI_UTF8, NULL);

    uni_casefold(UNI_DEFAULT, NULL, -1, UNI_UTF8, NULL, NULL, UNI_UTF8);
    uni_casefoldcmp(UNI_DEFAULT, NULL, -1, UNI_UTF8, NULL, -1, UNI_UTF8, NULL);
    uni_casefoldchk(UNI_DEFAULT, NULL, -1, UNI_UTF8, NULL);

    uni_norm(UNI_NFC, NULL, -1, UNI_UTF8, NULL, NULL, UNI_UTF8);
    uni_normcmp(NULL, -1, UNI_UTF8, NULL, -1, UNI_UTF8, NULL);
    uni_normchk(UNI_NFD, NULL, -1, UNI_UTF8, NULL);
    uni_normqchk(UNI_NFD, NULL, -1, UNI_UTF8, NULL);

    uni_compress(NULL, -1, UNI_UTF8, NULL, NULL);
    uni_decompress(NULL, -1, NULL, NULL, UNI_UTF8);

    uni_sortkeymk(NULL, -1, UNI_UTF8, UNI_NON_IGNORABLE, UNI_TERTIARY, NULL, NULL);
    uni_sortkeycmp(NULL, -1, NULL, -1, NULL);
    uni_collate(NULL, -1, UNI_UTF8, NULL, -1, UNI_UTF8, UNI_NON_IGNORABLE, UNI_PRIMARY, NULL);

    uni_next(NULL, -1, UNI_UTF16 | UNI_TRUST | UNI_NATIVE, NULL, NULL);
    uni_prev(NULL, -1, UNI_UTF8, NULL, NULL);
    
    uni_convert(NULL, -1, UNI_UTF8, NULL, NULL, UNI_UTF8);

    uni_nextbrk(UNI_GRAPHEME, NULL, -1, UNI_UTF16 | UNI_LITTLE, NULL);
    uni_nextbrk(UNI_GRAPHEME, NULL, -1, UNI_UTF32 | UNI_BIG, NULL);

    uni_gc(0);
    uni_ccc(0);
    uni_is(0, UNI_NONCHARACTER_CODE_POINT);
    uni_numval(0);
    uni_tolower(0);
    uni_toupper(0);
    return 0;
}
