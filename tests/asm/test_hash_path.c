/*
 * test_hash_path.c - ASM-HASH: petrush_hash_path FNV-1a 64-bit.
 * Semente oficial: 14695981039346656037 (0xcbf29ce484222325).
 * Prime: 1099511628211 (0x100000001b3). len bytes; nao exige NUL.
 */

#include "acutest.h"
#include "petrush/asm.h"

#include <stdint.h>
#include <string.h>

#define FNV1A64_OFFSET UINT64_C(14695981039346656037)

void test_hash_path_empty_is_offset(void)
{
    TEST_CHECK(petrush_hash_path("", 0) == FNV1A64_OFFSET);
    TEST_CHECK(petrush_hash_path(NULL, 0) == FNV1A64_OFFSET);
}

void test_hash_path_single_a(void)
{
    /* FNV-1a64("a") = 0xaf63dc4c8601ec8c */
    TEST_CHECK(petrush_hash_path("a", 1) == UINT64_C(0xaf63dc4c8601ec8c));
}

void test_hash_path_foobar(void)
{
    static const char s[] = "foobar";

    TEST_CHECK(petrush_hash_path(s, 6) == UINT64_C(0x85944171f73967e8));
}

void test_hash_path_hello(void)
{
    static const char s[] = "hello";

    TEST_CHECK(petrush_hash_path(s, 5) == UINT64_C(0xa430d84680aabd0b));
}

void test_hash_path_abc(void)
{
    static const char s[] = "abc";

    TEST_CHECK(petrush_hash_path(s, 3) == UINT64_C(0xe71fa2190541574b));
}

void test_hash_path_len_limits(void)
{
    /* len limita; bytes apos len nao entram */
    static const char s[] = "abXXXX";

    TEST_CHECK(petrush_hash_path(s, 2) == petrush_hash_path("ab", 2));
    TEST_CHECK(petrush_hash_path(s, 2) != petrush_hash_path(s, 6));
}

void test_hash_path_embedded_nul(void)
{
    static const unsigned char buf[] = { 0x00 };
    static const unsigned char buf2[] = { 0x00, 0x00 };

    TEST_CHECK(petrush_hash_path((const char *)buf, 1)
               == UINT64_C(0xaf63bd4c8601b7df));
    TEST_CHECK(petrush_hash_path((const char *)buf2, 2)
               == UINT64_C(0x08328807b4eb6fed));
}

void test_hash_path_usr_bin_petrush(void)
{
    static const char s[] = "/usr/bin/petrush";

    TEST_CHECK(petrush_hash_path(s, sizeof s - 1)
               == UINT64_C(0xa308077d7e3814e6));
}

void test_hash_path_prefix_differs(void)
{
    /* caminhos proximos nao colidem trivialmente */
    uint64_t a = petrush_hash_path("/usr/bin", 8);
    uint64_t b = petrush_hash_path("/usr/bin/", 9);

    TEST_CHECK(a != b);
    TEST_CHECK(a != FNV1A64_OFFSET);
}

TEST_LIST = {
    { "hash_path_empty_is_offset", test_hash_path_empty_is_offset },
    { "hash_path_single_a", test_hash_path_single_a },
    { "hash_path_foobar", test_hash_path_foobar },
    { "hash_path_hello", test_hash_path_hello },
    { "hash_path_abc", test_hash_path_abc },
    { "hash_path_len_limits", test_hash_path_len_limits },
    { "hash_path_embedded_nul", test_hash_path_embedded_nul },
    { "hash_path_usr_bin_petrush", test_hash_path_usr_bin_petrush },
    { "hash_path_prefix_differs", test_hash_path_prefix_differs },
    { NULL, NULL }
};
