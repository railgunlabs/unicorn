/*
 *  Unicorn - Embeddable Unicode Algorithms
 *  Copyright (c) 2021-2026 Railgun Labs
 * 
 *  THE CONTENTS OF THIS PROJECT ARE PROPRIETARY AND CONFIDENTIAL.
 *  UNAUTHORIZED COPYING, TRANSFERRING OR REPRODUCTION OF THE CONTENTS OF THIS
 *  PROJECT, VIA ANY MEDIUM IS STRICTLY PROHIBITED.
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

// Documentation is available at <https://RailgunLabs.com/unicorn/manual>.

#ifndef UNICORN_H
#define UNICORN_H

#include "_unicorn.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DOXYGEN
typedef uint32_t unichar;
#endif

/*
 *  The "UNICORN_STATIC" preprocessor directive must be defined
 *  before including <unicorn.h> when it's statically linked.
 */
#ifndef DOXYGEN
  #if defined(_WIN32) || defined(__CYGWIN__)
    #if defined(DLL_EXPORT)
      #define UNICORN_API __declspec(dllexport)
    #elif defined(UNICORN_STATIC)
      #define UNICORN_API
    #else
      #define UNICORN_API __declspec(dllimport)
    #endif
  #else
    #define UNICORN_API
  #endif
#endif

typedef int32_t unisize;

typedef enum unistat
{
    UNI_OK,
    UNI_DONE,
    UNI_NO_MEMORY,
    UNI_NO_SPACE,
    UNI_BAD_ENCODING,
    UNI_BAD_OPERATION,
    UNI_FEATURE_DISABLED,
    UNI_MALFUNCTION,
} unistat;

typedef uint32_t uniattr;

#define UNI_SCALAR 0x1u
#define UNI_UTF8 0x2u
#define UNI_UTF16 0x4u
#define UNI_UTF32 0x8u
#define UNI_BIG 0x10u
#define UNI_LITTLE 0x20u

#ifdef DOXYGEN
    #define UNI_NATIVE
#else
    #if defined(UNICORN_LITTLE_ENDIAN)
        #define UNI_NATIVE UNI_LITTLE
    #elif defined(UNICORN_BIG_ENDIAN)
        #define UNI_NATIVE UNI_BIG
    #else
        #error undefined endianness
    #endif
#endif

#define UNI_TRUST 0x40u
#define UNI_NULIFY 0x80u

//
// Unicode and Library Version
//

UNICORN_API void uni_getversion(int32_t *major, int32_t *minor, int32_t *patch);
UNICORN_API void uni_getucdversion(int32_t *major, int32_t *minor, int32_t *patch);

//
// Custom Memory Allocator
//

typedef void *(*unimemfunc)(void *user_data, void *ptr, size_t old_size, size_t new_size);

UNICORN_API unistat uni_setmemfunc(void *user_data, unimemfunc allocf);

//
// Logging
//

typedef void(*unierrfunc)(void *user_data, const char *message);

UNICORN_API void uni_seterrfunc(void *user_data, unierrfunc callback);

//
// Case Conversion
//

typedef enum unicaseconv
{
    UNI_LOWER,
    UNI_TITLE,
    UNI_UPPER,
} unicaseconv;

UNICORN_API unistat uni_caseconv(unicaseconv casing, const void *src, unisize src_len, uniattr src_attr, void *dst, unisize *dst_len, uniattr dst_attr);
UNICORN_API unistat uni_caseconvchk(unicaseconv casing, const void *text, unisize text_len, uniattr text_attr, bool *result);

//
// Case Folding
//

typedef enum unicasefold
{
    UNI_DEFAULT,
    UNI_CANONICAL,
} unicasefold;

UNICORN_API unistat uni_casefold(unicasefold casing, const void *src, unisize src_len, uniattr src_attr, void *dst, unisize *dst_len, uniattr dst_attr);
UNICORN_API unistat uni_casefoldcmp(unicasefold casing, const void *s1, unisize s1_len, uniattr s1_attr, const void *s2, unisize s2_len, uniattr s2_attr, bool *result);
UNICORN_API unistat uni_casefoldchk(unicasefold casing, const void *text, unisize text_len, uniattr text_attr, bool *result);

//
// Text Compression
//

UNICORN_API unistat uni_compress(const void *text, unisize text_len, uniattr text_attr, uint8_t *buffer, size_t *buffer_length);
UNICORN_API unistat uni_decompress(const uint8_t *buffer, size_t buffer_length, void *text, unisize *text_len, uniattr text_attr);

//
// Collation
//

typedef enum unistrength
{
    UNI_PRIMARY = 1,
    UNI_SECONDARY = 2,
    UNI_TERTIARY = 3,
    UNI_QUATERNARY = 4,
} unistrength;

typedef enum uniweighting
{
    UNI_NON_IGNORABLE,
    UNI_SHIFTED,
} uniweighting;

UNICORN_API unistat uni_sortkeymk(const void *text, unisize text_len, uniattr text_attr, uniweighting weighting, unistrength strength, uint16_t *sortkey, size_t *sortkey_cap);
UNICORN_API unistat uni_sortkeycmp(const uint16_t *sk1, size_t sk1_len, const uint16_t *sk2, size_t sk2_len, int32_t *result);
UNICORN_API unistat uni_collate(const void *s1, unisize s1_len, uniattr s1_attr, const void *s2, unisize s2_len, uniattr s2_attr, uniweighting weighting, unistrength strength, int32_t *result);

//
// Normalization
//

typedef enum uninormform
{
    UNI_NFC,
    UNI_NFD,
} uninormform;

typedef enum uninormchk
{
    UNI_YES,
    UNI_MAYBE,
    UNI_NO,
} uninormchk;

UNICORN_API unistat uni_norm(uninormform form, const void *src, unisize src_len, uniattr src_attr, void *dst, unisize *dst_len, uniattr dst_attr);
UNICORN_API unistat uni_normcmp(const void *s1, unisize s1_len, uniattr s1_attr, const void *s2, unisize s2_len, uniattr s2_attr, bool *result);
UNICORN_API unistat uni_normchk(uninormform form, const void *text, unisize text_len, uniattr text_attr, bool *result);
UNICORN_API unistat uni_normqchk(uninormform form, const void *text, unisize text_len, uniattr text_attr, uninormchk *result);

//
// Text Iterators, Encoders, Decoders, and Validators
//

UNICORN_API unistat uni_next(const void *text, unisize text_len, uniattr text_attr, unisize *index, unichar *cp);
UNICORN_API unistat uni_prev(const void *text, unisize text_len, uniattr text_attr, unisize *index, unichar *cp);
UNICORN_API unistat uni_encode(unichar cp, void *dst, unisize *dst_len, uniattr dst_attr);
UNICORN_API unistat uni_convert(const void *src, unisize src_len, uniattr src_attr, void *dst, unisize *dst_len, uniattr dst_attr);
UNICORN_API unistat uni_validate(const void *text, unisize text_len, uniattr text_attr);

//
// Text Segmentation
//

typedef enum unibreak
{
    UNI_GRAPHEME,
    UNI_WORD,
    UNI_SENTENCE,
} unibreak;

UNICORN_API unistat uni_nextbrk(unibreak boundary, const void *text, unisize text_len, uniattr text_attr, unisize *index);
UNICORN_API unistat uni_prevbrk(unibreak boundary, const void *text, unisize text_len, uniattr text_attr, unisize *index);

//
// Character Properties
//

typedef enum unigc
{
    UNI_UPPERCASE_LETTER,
    UNI_LOWERCASE_LETTER,
    UNI_TITLECASE_LETTER,
    UNI_MODIFIER_LETTER,
    UNI_OTHER_LETTER,

    UNI_NONSPACING_MARK,
    UNI_SPACING_MARK,
    UNI_ENCLOSING_MARK,

    UNI_DECIMAL_NUMBER,
    UNI_LETTER_NUMBER,
    UNI_OTHER_NUMBER,

    UNI_CONNECTOR_PUNCTUATION,
    UNI_DASH_PUNCTUATION,
    UNI_OPEN_PUNCTUATION,
    UNI_CLOSE_PUNCTUATION,
    UNI_INITIAL_PUNCTUATION,
    UNI_FINAL_PUNCTUATION,
    UNI_OTHER_PUNCTUATION,

    UNI_MATH_SYMBOL,
    UNI_CURRENCY_SYMBOL,
    UNI_MODIFIER_SYMBOL,
    UNI_OTHER_SYMBOL,

    UNI_SPACE_SEPARATOR,
    UNI_LINE_SEPARATOR,
    UNI_PARAGRAPH_SEPARATOR,

    UNI_CONTROL,
    UNI_FORMAT,
    UNI_SURROGATE,
    UNI_PRIVATE_USE,
    UNI_UNASSIGNED,
} unigc;

typedef enum unibp
{
    UNI_NONCHARACTER_CODE_POINT,
    UNI_ALPHABETIC,
    UNI_LOWERCASE,
    UNI_UPPERCASE,
    UNI_HEX_DIGIT,
    UNI_WHITE_SPACE,
    UNI_MATH,
    UNI_DASH,
    UNI_DIACRITIC,
    UNI_EXTENDER,
    UNI_IDEOGRAPHIC,
    UNI_QUOTATION_MARK,
    UNI_UNIFIED_IDEOGRAPH,
    UNI_TERMINAL_PUNCTUATION,
} unibp;

UNICORN_API unigc uni_gc(unichar c);
UNICORN_API uint8_t uni_ccc(unichar c);
UNICORN_API bool uni_is(unichar c, unibp p);
UNICORN_API const char *uni_numval(unichar c);
UNICORN_API unichar uni_tolower(unichar c);
UNICORN_API unichar uni_totitle(unichar c);
UNICORN_API unichar uni_toupper(unichar c);

#ifdef __cplusplus
}
#endif

#endif // UNICORN_H
