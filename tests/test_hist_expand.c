/*
 * test_hist_expand.c — TDD for !! and !n (NEW-26)
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

TEST_LIST = {
    { "bang_bang_last", test_bang_bang_last },
    { "bang_number", test_bang_number },
    { "no_expand", test_no_expand },
    { NULL, NULL }
};
