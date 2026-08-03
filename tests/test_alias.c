/*
 * test_alias.c — TDD for alias set/get/expand (NEW-22)
 * RED first: these tests fail until src/mid/alias.c exists.
 */

#include "acutest.h"
#include "petrush/alias.h"

#include <string.h>
#include <stdlib.h>

void test_alias_set_get(void)
{
    alias_clear_all();
    TEST_CHECK(alias_set("ll", "ls -la") == 0);
    const char *v = alias_get("ll");
    TEST_CHECK(v != NULL);
    TEST_CHECK(strcmp(v, "ls -la") == 0);
    alias_clear_all();
}

void test_alias_unset(void)
{
    alias_clear_all();
    TEST_CHECK(alias_set("g", "git") == 0);
    TEST_CHECK(alias_unset("g") == 0);
    TEST_CHECK(alias_get("g") == NULL);
    alias_clear_all();
}

void test_alias_expand_first_word(void)
{
    alias_clear_all();
    TEST_CHECK(alias_set("ll", "ls -la") == 0);
    char *exp = alias_expand_line("ll /tmp");
    TEST_CHECK(exp != NULL);
    TEST_CHECK(strcmp(exp, "ls -la /tmp") == 0);
    free(exp);
    alias_clear_all();
}

void test_alias_expand_no_match(void)
{
    alias_clear_all();
    char *exp = alias_expand_line("echo hi");
    TEST_CHECK(exp != NULL);
    TEST_CHECK(strcmp(exp, "echo hi") == 0);
    free(exp);
}

void test_alias_reject_empty_name(void)
{
    alias_clear_all();
    TEST_CHECK(alias_set("", "x") != 0);
    TEST_CHECK(alias_set(NULL, "x") != 0);
}

TEST_LIST = {
    { "alias_set_get", test_alias_set_get },
    { "alias_unset", test_alias_unset },
    { "alias_expand_first_word", test_alias_expand_first_word },
    { "alias_expand_no_match", test_alias_expand_no_match },
    { "alias_reject_empty_name", test_alias_reject_empty_name },
    { NULL, NULL }
};
