/*
 * test_expand.c — TDD UX-12 tilde + UX-13 $VAR (NEW expand)
 */

#include "acutest.h"
#include "petrush/expand.h"
#include "petrush/env.h"

#include <stdlib.h>
#include <string.h>

void test_tilde_alone(void)
{
    petrush_setenv("HOME", "/home/tester", 1);
    char *e = expand_word("~");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "/home/tester") == 0);
    free(e);
}

void test_tilde_slash(void)
{
    petrush_setenv("HOME", "/home/tester", 1);
    char *e = expand_word("~/docs");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "/home/tester/docs") == 0);
    free(e);
}

void test_var_simple(void)
{
    petrush_setenv("FOO_PETRUSH", "barval", 1);
    char *e = expand_word("$FOO_PETRUSH");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "barval") == 0);
    free(e);
}

void test_var_braces(void)
{
    petrush_setenv("FOO_PETRUSH", "barval", 1);
    char *e = expand_word("pre_${FOO_PETRUSH}_post");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "pre_barval_post") == 0);
    free(e);
}

void test_no_expand_literal(void)
{
    char *e = expand_word("hello");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "hello") == 0);
    free(e);
}

TEST_LIST = {
    { "tilde_alone", test_tilde_alone },
    { "tilde_slash", test_tilde_slash },
    { "var_simple", test_var_simple },
    { "var_braces", test_var_braces },
    { "no_expand_literal", test_no_expand_literal },
    { NULL, NULL }
};
