/*
 * test_memeq.c - ASM-MEMEQ: petrush_memeq_ct (tempo constante).
 * Contrato: 0 iguais, 1 diferem; n==0 igual (NULL ok); sem early-out.
 */

#include "acutest.h"
#include "petrush/asm.h"

#include <stdint.h>
#include <string.h>

void test_memeq_n_zero_nulls(void)
{
    TEST_CHECK(petrush_memeq_ct(NULL, NULL, 0) == 0);
}

void test_memeq_n_zero_nonnull(void)
{
    static const unsigned char a[] = { 0x01 };
    static const unsigned char b[] = { 0x02 };

    TEST_CHECK(petrush_memeq_ct(a, b, 0) == 0);
    TEST_CHECK(petrush_memeq_ct(a, NULL, 0) == 0);
    TEST_CHECK(petrush_memeq_ct(NULL, b, 0) == 0);
}

void test_memeq_equal_one_byte(void)
{
    static const unsigned char a[] = { 0xA5 };
    static const unsigned char b[] = { 0xA5 };

    TEST_CHECK(petrush_memeq_ct(a, b, 1) == 0);
}

void test_memeq_differ_one_byte(void)
{
    static const unsigned char a[] = { 0xA5 };
    static const unsigned char b[] = { 0x5A };

    TEST_CHECK(petrush_memeq_ct(a, b, 1) == 1);
}

void test_memeq_equal_multi(void)
{
    static const unsigned char a[] = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
    };
    unsigned char b[sizeof a];

    memcpy(b, a, sizeof a);
    TEST_CHECK(petrush_memeq_ct(a, b, sizeof a) == 0);
}

void test_memeq_differ_first(void)
{
    static const unsigned char a[] = { 0x01, 0x02, 0x03, 0x04 };
    static const unsigned char b[] = { 0xFF, 0x02, 0x03, 0x04 };

    TEST_CHECK(petrush_memeq_ct(a, b, sizeof a) == 1);
}

void test_memeq_differ_last(void)
{
    static const unsigned char a[] = { 0x01, 0x02, 0x03, 0x04 };
    static const unsigned char b[] = { 0x01, 0x02, 0x03, 0xFF };

    TEST_CHECK(petrush_memeq_ct(a, b, sizeof a) == 1);
}

void test_memeq_differ_middle(void)
{
    static const unsigned char a[] = { 0x10, 0x20, 0x30, 0x40, 0x50 };
    static const unsigned char b[] = { 0x10, 0x20, 0xDE, 0x40, 0x50 };

    TEST_CHECK(petrush_memeq_ct(a, b, sizeof a) == 1);
}

void test_memeq_equal_zeros(void)
{
    static const unsigned char a[32];
    static const unsigned char b[32];

    TEST_CHECK(petrush_memeq_ct(a, b, sizeof a) == 0);
}

void test_memeq_all_bits_differ(void)
{
    static const unsigned char a[] = { 0x00, 0x00, 0x00, 0x00 };
    static const unsigned char b[] = { 0xFF, 0xFF, 0xFF, 0xFF };

    TEST_CHECK(petrush_memeq_ct(a, b, sizeof a) == 1);
}

void test_memeq_self_compare(void)
{
    static const char msg[] = "petrush ASM-MEMEQ";

    TEST_CHECK(petrush_memeq_ct(msg, msg, sizeof msg) == 0);
}

TEST_LIST = {
    { "memeq_n_zero_nulls", test_memeq_n_zero_nulls },
    { "memeq_n_zero_nonnull", test_memeq_n_zero_nonnull },
    { "memeq_equal_one_byte", test_memeq_equal_one_byte },
    { "memeq_differ_one_byte", test_memeq_differ_one_byte },
    { "memeq_equal_multi", test_memeq_equal_multi },
    { "memeq_differ_first", test_memeq_differ_first },
    { "memeq_differ_last", test_memeq_differ_last },
    { "memeq_differ_middle", test_memeq_differ_middle },
    { "memeq_equal_zeros", test_memeq_equal_zeros },
    { "memeq_all_bits_differ", test_memeq_all_bits_differ },
    { "memeq_self_compare", test_memeq_self_compare },
    { NULL, NULL }
};
