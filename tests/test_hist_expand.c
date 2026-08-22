/*
 * test_hist_expand.c — TDD for !! / !n / !$ / !^ (NEW-26 + FEAT-BANG)
 */

#include "acutest.h"
#include "petrush/hist_expand.h"
#include "linenoise.h"

#include <string.h>
#include <stdlib.h>

void test_bang_bang_last(void)
{
    linenoiseHistorySetMaxLen(50);
    linenoiseHistoryAdd("echo first");
    linenoiseHistoryAdd("echo second");
    char *exp = hist_expand_line("!!");
    TEST_CHECK(exp != NULL);
    if (exp) {
        TEST_CHECK(strcmp(exp, "echo second") == 0);
        free(exp);
    }
}

void test_bang_number(void)
{
    linenoiseHistorySetMaxLen(50);
    linenoiseHistoryAdd("cmd_one");
    linenoiseHistoryAdd("cmd_two");
    char *exp = hist_expand_line("!1");
    TEST_CHECK(exp != NULL);
    if (exp) {
        TEST_CHECK(strcmp(exp, "cmd_one") == 0);
        free(exp);
    }
}

void test_no_expand(void)
{
    char *exp = hist_expand_line("echo hi");
    TEST_CHECK(exp == NULL);
}

/* FEAT-BANG: !$ = último arg do último evento */
void test_bang_dollar_last_arg(void)
{
    linenoiseHistorySetMaxLen(50);
    linenoiseHistoryAdd("echo hello world");
    char *exp = hist_expand_line("!$");
    TEST_CHECK(exp != NULL);
    if (exp) {
        TEST_CHECK(strcmp(exp, "world") == 0);
        free(exp);
    }
}

/* FEAT-BANG: !^ = primeiro arg (word 1) do último evento */
void test_bang_caret_first_arg(void)
{
    linenoiseHistorySetMaxLen(50);
    linenoiseHistoryAdd("cp src dst");
    char *exp = hist_expand_line("!^");
    TEST_CHECK(exp != NULL);
    if (exp) {
        TEST_CHECK(strcmp(exp, "src") == 0);
        free(exp);
    }
}

/* FEAT-BANG: !$ com uma só palavra = essa palavra (bash) */
void test_bang_dollar_single_word(void)
{
    linenoiseHistorySetMaxLen(50);
    linenoiseHistoryAdd("ls");
    char *exp = hist_expand_line("!$");
    TEST_CHECK(exp != NULL);
    if (exp) {
        TEST_CHECK(strcmp(exp, "ls") == 0);
        free(exp);
    }
}

/* FEAT-BANG: !^ sem args = falha (sem word 1) */
void test_bang_caret_no_arg(void)
{
    linenoiseHistorySetMaxLen(50);
    linenoiseHistoryAdd("pwd");
    char *exp = hist_expand_line("!^");
    TEST_CHECK(exp == NULL);
}

TEST_LIST = {
    { "bang_bang_last", test_bang_bang_last },
    { "bang_number", test_bang_number },
    { "no_expand", test_no_expand },
    { "bang_dollar_last_arg", test_bang_dollar_last_arg },
    { "bang_caret_first_arg", test_bang_caret_first_arg },
    { "bang_dollar_single_word", test_bang_dollar_single_word },
    { "bang_caret_no_arg", test_bang_caret_no_arg },
    { NULL, NULL }
};
