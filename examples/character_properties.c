/*
 *  This file is public domain.
 *  You may use, modify, and distribute it without restriction.
 */

// Examples are also documented online at:
// <https://railgunlabs.com/unicorn/manual/code-examples/>.

#include <unicorn.h>
#include <stdio.h>

static const char *const general_categories[] = {
    [UNI_UPPERCASE_LETTER] = "Uppercase_Letter",
    [UNI_LOWERCASE_LETTER] = "Lowercase_Letter",
    [UNI_TITLECASE_LETTER] = "Titlecase_Letter",
    [UNI_MODIFIER_LETTER] = "Modifier_Letter",
    [UNI_OTHER_LETTER] = "Other_Letter",
    [UNI_NONSPACING_MARK] = "Nonspacing_Mark",
    [UNI_SPACING_MARK] = "Spacing_Mark",
    [UNI_ENCLOSING_MARK] = "Enclosing_Mark",
    [UNI_DECIMAL_NUMBER] = "Decimal_Number",
    [UNI_LETTER_NUMBER] = "Letter_Number",
    [UNI_OTHER_NUMBER] = "Other_Number",
    [UNI_CONNECTOR_PUNCTUATION] = "Connector_Punctuation",
    [UNI_DASH_PUNCTUATION] = "Dash_Punctuation",
    [UNI_OPEN_PUNCTUATION] = "Open_Punctuation",
    [UNI_CLOSE_PUNCTUATION] = "Close_Punctuation",
    [UNI_INITIAL_PUNCTUATION] = "Initial_Punctuation",
    [UNI_FINAL_PUNCTUATION] = "Final_Punctuation",
    [UNI_OTHER_PUNCTUATION] = "Other_Punctuation",
    [UNI_MATH_SYMBOL] = "Math_Symbol",
    [UNI_CURRENCY_SYMBOL] = "Currency_Symbol",
    [UNI_MODIFIER_SYMBOL] = "Modifier_Symbol",
    [UNI_OTHER_SYMBOL] = "Other_Symbol",
    [UNI_SPACE_SEPARATOR] = "Space_Separator",
    [UNI_LINE_SEPARATOR] = "Line_Separator",
    [UNI_PARAGRAPH_SEPARATOR] = "Paragraph_Separator",
    [UNI_CONTROL] = "Control",
    [UNI_FORMAT] = "Format",
    [UNI_SURROGATE] = "Surrogate",
    [UNI_PRIVATE_USE] = "Private_Use",
    [UNI_UNASSIGNED] = "Unassigned",
};

static const char *const binary_properties[] = {
    [UNI_NONCHARACTER_CODE_POINT] = "Noncharacter_Code_Point",
    [UNI_ALPHABETIC] = "Alphabetic",
    [UNI_LOWERCASE] = "Lowercase",
    [UNI_UPPERCASE] = "Uppercase",
    [UNI_HEX_DIGIT] = "Hex_Digit",
    [UNI_WHITE_SPACE] = "White_Space",
    [UNI_MATH] = "Math",
    [UNI_DASH] = "Dash",
    [UNI_DIACRITIC] = "Diacritic",
    [UNI_EXTENDER] = "Extender",
    [UNI_IDEOGRAPHIC] = "Ideographic",
    [UNI_QUOTATION_MARK] = "Quotation_Mark",
    [UNI_UNIFIED_IDEOGRAPH] = "Unified_Ideograph",
    [UNI_TERMINAL_PUNCTUATION] = "Terminal_Punctuation",
};

int main(int argc, char *argv[])
{
    // This code example prints character properties associated with
    // a Unicode code point. The meaning of each support character
    // property is documented here:
    // <https://railgunlabs.com/unicorn/manual/api/character-properties/>.

    // The following variable defines the Unicode character whose
    // properties will be queried. In this example, it's Unicode
    // character U+0300. Change this value to a different character
    // and notice how the printed properties change.
    unichar cp = 0x300;

    unichar tmp_cp;
    uint8_t ccc;
    const char *numeric_value;

    printf("General category: %s\n", general_categories[uni_gc(cp)]);

    ccc = uni_ccc(cp);
    if (ccc != 0)
    {
        printf("Canonical combining class: %d\n", ccc);
    }

    for (int i = 0; i < sizeof(binary_properties) / sizeof(binary_properties[0]); i++)
    {
        if (uni_is(cp, i))
        {
            printf("Binary property: %s\n", binary_properties[i]);
        }
    }

    numeric_value = uni_numval(cp);
    if (numeric_value != NULL)
    {
        printf("Numeric value: %s\n", numeric_value);
    }

    tmp_cp = uni_tolower(cp);
    if (tmp_cp != cp)
    {
        printf("Simple lower case mapping: U+%04X\n", tmp_cp);
    }

    tmp_cp = uni_toupper(cp);
    if (tmp_cp != cp)
    {
        printf("Simple upper case mapping: U+%04X\n", tmp_cp);
    }

    tmp_cp = uni_totitle(cp);
    if (tmp_cp != cp)
    {
        printf("Simple title case mapping: U+%04X\n", tmp_cp);
    }

    return 0;
}
