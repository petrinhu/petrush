/*
 * test_utf8.c - ASM-UTF8: petrush_utf8_width (UAX#11 subset).
 * Contrato: ASCII=1, combining=0, CJK/wide=2, UTF-8 invalido=-1.
 * len==0 -> 0; s==NULL com len>0 -> -1.
 */

#include "acutest.h"
#include "petrush/asm.h"

#include <string.h>

static int width_cstr(const char *s)
{
    return petrush_utf8_width(s, s ? strlen(s) : 0);
}

void test_utf8_empty(void)
{
    TEST_CHECK(petrush_utf8_width("", 0) == 0);
    TEST_CHECK(petrush_utf8_width("x", 0) == 0);
}

void test_utf8_ascii(void)
{
    TEST_CHECK(width_cstr("a") == 1);
    TEST_CHECK(width_cstr("abc") == 3);
    TEST_CHECK(width_cstr("Hello, world!") == 13);
}

void test_utf8_ascii_controls_count_as_one(void)
{
    /* Subset do brief: ASCII=1 (inclusive C0). */
    static const char ctrl[] = { 0x01, 0x1F, 0x7F, 0 };
    TEST_CHECK(petrush_utf8_width(ctrl, 3) == 3);
}

void test_utf8_latin1_narrow(void)
{
    /* U+00F1 n tilde = C3 B1 -> 1 coluna */
    TEST_CHECK(width_cstr("\xC3\xB1") == 1);
    /* cafe com e acute precomposed U+00E9 */
    TEST_CHECK(width_cstr("caf\xC3\xA9") == 4);
}

void test_utf8_combining_zero(void)
{
    /* 'a' + U+0301 COMBINING ACUTE = 1 + 0 */
    TEST_CHECK(width_cstr("a\xCC\x81") == 1);
    /* so combining -> 0 */
    TEST_CHECK(width_cstr("\xCC\x81") == 0);
    /* U+20D0 combining left harpoon (E2 83 90) */
    TEST_CHECK(width_cstr("x\xE2\x83\x90") == 1);
}

void test_utf8_cjk_wide(void)
{
    /* U+5B57 ideograph = E5 AD 97 -> 2 */
    TEST_CHECK(width_cstr("\xE5\xAD\x97") == 2);
    /* "日本語" = 2+2+2 */
    TEST_CHECK(width_cstr("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E") == 6);
}

void test_utf8_hangul_wide(void)
{
    /* U+D55C hangul syllable = ED 95 9C -> 2 */
    TEST_CHECK(width_cstr("\xED\x95\x9C") == 2);
    /* Hangul Jamo U+1100 = E1 84 80 -> 2 */
    TEST_CHECK(width_cstr("\xE1\x84\x80") == 2);
}

void test_utf8_fullwidth(void)
{
    /* U+FF21 FULLWIDTH LATIN A = EF BC A1 -> 2 */
    TEST_CHECK(width_cstr("\xEF\xBC\xA1") == 2);
}

void test_utf8_mixed(void)
{
    /* a + ideograph + b = 1+2+1 */
    TEST_CHECK(width_cstr("a\xE5\xAD\x97""b") == 4);
}

void test_utf8_cjk_ext_b(void)
{
    /* U+20000 CJK Ext B = F0 A0 80 80 -> 2 */
    TEST_CHECK(width_cstr("\xF0\xA0\x80\x80") == 2);
}

void test_utf8_invalid_truncated(void)
{
    static const char trunc[] = { (char)0xC2 };
    TEST_CHECK(petrush_utf8_width(trunc, 1) == -1);
}

void test_utf8_invalid_cont_alone(void)
{
    static const char cont[] = { (char)0x80 };
    TEST_CHECK(petrush_utf8_width(cont, 1) == -1);
}

void test_utf8_invalid_overlong(void)
{
    /* overlong NUL C0 80 */
    static const char ol[] = { (char)0xC0, (char)0x80 };
    TEST_CHECK(petrush_utf8_width(ol, 2) == -1);
}

void test_utf8_invalid_surrogate(void)
{
    /* U+D800 encoded as ED A0 80 */
    static const char sur[] = { (char)0xED, (char)0xA0, (char)0x80 };
    TEST_CHECK(petrush_utf8_width(sur, 3) == -1);
}

void test_utf8_invalid_too_large(void)
{
    /* lead F5 ... acima de U+10FFFF */
    static const char big[] = { (char)0xF5, (char)0x80, (char)0x80, (char)0x80 };
    TEST_CHECK(petrush_utf8_width(big, 4) == -1);
}

void test_utf8_null_s_nonzero_len(void)
{
    TEST_CHECK(petrush_utf8_width(NULL, 1) == -1);
}

void test_utf8_explicit_len_prefix(void)
{
    static const char buf[] = "abcXX";
    TEST_CHECK(petrush_utf8_width(buf, 3) == 3);
}

TEST_LIST = {
    { "utf8_empty", test_utf8_empty },
    { "utf8_ascii", test_utf8_ascii },
    { "utf8_ascii_controls_count_as_one", test_utf8_ascii_controls_count_as_one },
    { "utf8_latin1_narrow", test_utf8_latin1_narrow },
    { "utf8_combining_zero", test_utf8_combining_zero },
    { "utf8_cjk_wide", test_utf8_cjk_wide },
    { "utf8_hangul_wide", test_utf8_hangul_wide },
    { "utf8_fullwidth", test_utf8_fullwidth },
    { "utf8_mixed", test_utf8_mixed },
    { "utf8_cjk_ext_b", test_utf8_cjk_ext_b },
    { "utf8_invalid_truncated", test_utf8_invalid_truncated },
    { "utf8_invalid_cont_alone", test_utf8_invalid_cont_alone },
    { "utf8_invalid_overlong", test_utf8_invalid_overlong },
    { "utf8_invalid_surrogate", test_utf8_invalid_surrogate },
    { "utf8_invalid_too_large", test_utf8_invalid_too_large },
    { "utf8_null_s_nonzero_len", test_utf8_null_s_nonzero_len },
    { "utf8_explicit_len_prefix", test_utf8_explicit_len_prefix },
    { NULL, NULL }
};
