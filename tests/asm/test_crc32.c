/*
 * test_crc32.c - ASM-CRC: petrush_crc32 IEEE 0xEDB88320.
 * Contrato: incremental; init tipico 0xFFFFFFFF; XOR final a cargo do caller.
 * Vetor canonico "123456789" -> digest 0xCBF43926.
 */

#include "acutest.h"
#include "petrush/asm.h"

#include <stdint.h>
#include <string.h>

static uint32_t crc32_digest(const void *buf, size_t len)
{
    uint32_t c = petrush_crc32(0xFFFFFFFFu, buf, len);
    return c ^ 0xFFFFFFFFu;
}

void test_crc32_len_zero_passthrough(void)
{
    static const unsigned char b[] = { 0x42 };

    TEST_CHECK(petrush_crc32(0xFFFFFFFFu, b, 0) == 0xFFFFFFFFu);
    TEST_CHECK(petrush_crc32(0x12345678u, NULL, 0) == 0x12345678u);
    TEST_CHECK(petrush_crc32(0u, b, 0) == 0u);
}

void test_crc32_empty_digest(void)
{
    /* digest de buffer vazio com init+final XOR = 0 */
    TEST_CHECK(crc32_digest("", 0) == 0u);
}

void test_crc32_check_vector_123456789(void)
{
    static const char msg[] = "123456789";

    TEST_CHECK(crc32_digest(msg, 9) == 0xCBF43926u);
}

void test_crc32_raw_intermediate_vector(void)
{
    /* sem XOR final: estado apos processar o vetor */
    static const char msg[] = "123456789";
    uint32_t raw = petrush_crc32(0xFFFFFFFFu, msg, 9);

    TEST_CHECK(raw == 0x340BC6D9u);
    TEST_CHECK((raw ^ 0xFFFFFFFFu) == 0xCBF43926u);
}

void test_crc32_incremental_matches_oneshot(void)
{
    static const char msg[] = "123456789";
    uint32_t one = petrush_crc32(0xFFFFFFFFu, msg, 9);
    uint32_t a = petrush_crc32(0xFFFFFFFFu, msg, 5);
    uint32_t b = petrush_crc32(a, msg + 5, 4);

    TEST_CHECK(a != one);
    TEST_CHECK(b == one);
    TEST_CHECK((b ^ 0xFFFFFFFFu) == 0xCBF43926u);
}

void test_crc32_single_byte_zero(void)
{
    static const unsigned char z[] = { 0x00 };
    /* CRC-32 IEEE de um unico 0x00 com init+final */
    TEST_CHECK(crc32_digest(z, 1) == 0xD202EF8Du);
}

void test_crc32_abc(void)
{
    static const char msg[] = "abc";

    TEST_CHECK(crc32_digest(msg, 3) == 0x352441C2u);
}

void test_crc32_null_bytes(void)
{
    static const unsigned char buf[] = { 0x00, 0x00, 0x00, 0x00 };

    TEST_CHECK(crc32_digest(buf, sizeof buf) == 0x2144DF1Cu);
}

TEST_LIST = {
    { "crc32_len_zero_passthrough", test_crc32_len_zero_passthrough },
    { "crc32_empty_digest", test_crc32_empty_digest },
    { "crc32_check_vector_123456789", test_crc32_check_vector_123456789 },
    { "crc32_raw_intermediate_vector", test_crc32_raw_intermediate_vector },
    { "crc32_incremental_matches_oneshot", test_crc32_incremental_matches_oneshot },
    { "crc32_single_byte_zero", test_crc32_single_byte_zero },
    { "crc32_abc", test_crc32_abc },
    { "crc32_null_bytes", test_crc32_null_bytes },
    { NULL, NULL }
};
