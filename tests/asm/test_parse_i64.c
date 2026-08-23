/*
 * test_parse_i64.c - ASM-I64: petrush_parse_i64 (decimal signed 64, sem overflow UB).
 * Contrato: 0 sucesso (*out escrito); -1 vazio/invalido/overflow (*out intacto).
 */

#include "acutest.h"
#include "petrush/asm.h"

#include <stdint.h>
#include <string.h>

static int parse_cstr(const char *s, int64_t *out)
{
    return petrush_parse_i64(s, s ? strlen(s) : 0, out);
}

void test_i64_zero(void)
{
    int64_t out = 42;

    TEST_CHECK(parse_cstr("0", &out) == 0);
    TEST_CHECK(out == 0);
}

void test_i64_positive_simple(void)
{
    int64_t out = -1;

    TEST_CHECK(parse_cstr("42", &out) == 0);
    TEST_CHECK(out == 42);
}

void test_i64_negative_simple(void)
{
    int64_t out = 1;

    TEST_CHECK(parse_cstr("-7", &out) == 0);
    TEST_CHECK(out == -7);
}

void test_i64_plus_sign(void)
{
    int64_t out = 0;

    TEST_CHECK(parse_cstr("+99", &out) == 0);
    TEST_CHECK(out == 99);
}

void test_i64_leading_zeros(void)
{
    int64_t out = -1;

    TEST_CHECK(parse_cstr("00042", &out) == 0);
    TEST_CHECK(out == 42);
    TEST_CHECK(parse_cstr("-000", &out) == 0);
    TEST_CHECK(out == 0);
    TEST_CHECK(parse_cstr("+0001", &out) == 0);
    TEST_CHECK(out == 1);
}

void test_i64_int64_max(void)
{
    int64_t out = 0;

    TEST_CHECK(parse_cstr("9223372036854775807", &out) == 0);
    TEST_CHECK(out == INT64_MAX);
}

void test_i64_int64_min(void)
{
    int64_t out = 0;

    TEST_CHECK(parse_cstr("-9223372036854775808", &out) == 0);
    TEST_CHECK(out == INT64_MIN);
}

void test_i64_overflow_max_plus_one(void)
{
    int64_t out = 12345;

    TEST_CHECK(parse_cstr("9223372036854775808", &out) == -1);
    TEST_CHECK(out == 12345);
}

void test_i64_overflow_min_minus_one(void)
{
    int64_t out = 99;

    TEST_CHECK(parse_cstr("-9223372036854775809", &out) == -1);
    TEST_CHECK(out == 99);
}

void test_i64_overflow_long(void)
{
    int64_t out = 7;

    TEST_CHECK(parse_cstr("99999999999999999999", &out) == -1);
    TEST_CHECK(out == 7);
}

void test_i64_empty(void)
{
    int64_t out = 5;

    TEST_CHECK(petrush_parse_i64("", 0, &out) == -1);
    TEST_CHECK(out == 5);
    TEST_CHECK(petrush_parse_i64("x", 0, &out) == -1);
    TEST_CHECK(out == 5);
}

void test_i64_sign_only(void)
{
    int64_t out = 3;

    TEST_CHECK(parse_cstr("+", &out) == -1);
    TEST_CHECK(out == 3);
    TEST_CHECK(parse_cstr("-", &out) == -1);
    TEST_CHECK(out == 3);
}

void test_i64_invalid_chars(void)
{
    int64_t out = 11;

    TEST_CHECK(parse_cstr("12x", &out) == -1);
    TEST_CHECK(out == 11);
    TEST_CHECK(parse_cstr(" 1", &out) == -1);
    TEST_CHECK(out == 11);
    TEST_CHECK(parse_cstr("1 ", &out) == -1);
    TEST_CHECK(out == 11);
    TEST_CHECK(parse_cstr("1.0", &out) == -1);
    TEST_CHECK(out == 11);
    TEST_CHECK(parse_cstr("0x10", &out) == -1);
    TEST_CHECK(out == 11);
}

void test_i64_null_out(void)
{
    TEST_CHECK(petrush_parse_i64("1", 1, NULL) == -1);
}

void test_i64_null_s_nonzero_len(void)
{
    int64_t out = 8;

    TEST_CHECK(petrush_parse_i64(NULL, 1, &out) == -1);
    TEST_CHECK(out == 8);
}

void test_i64_explicit_len_prefix(void)
{
    int64_t out = -1;
    static const char buf[] = "12399";

    TEST_CHECK(petrush_parse_i64(buf, 3, &out) == 0);
    TEST_CHECK(out == 123);
}

TEST_LIST = {
    { "i64_zero", test_i64_zero },
    { "i64_positive_simple", test_i64_positive_simple },
    { "i64_negative_simple", test_i64_negative_simple },
    { "i64_plus_sign", test_i64_plus_sign },
    { "i64_leading_zeros", test_i64_leading_zeros },
    { "i64_int64_max", test_i64_int64_max },
    { "i64_int64_min", test_i64_int64_min },
    { "i64_overflow_max_plus_one", test_i64_overflow_max_plus_one },
    { "i64_overflow_min_minus_one", test_i64_overflow_min_minus_one },
    { "i64_overflow_long", test_i64_overflow_long },
    { "i64_empty", test_i64_empty },
    { "i64_sign_only", test_i64_sign_only },
    { "i64_invalid_chars", test_i64_invalid_chars },
    { "i64_null_out", test_i64_null_out },
    { "i64_null_s_nonzero_len", test_i64_null_s_nonzero_len },
    { "i64_explicit_len_prefix", test_i64_explicit_len_prefix },
    { NULL, NULL }
};
