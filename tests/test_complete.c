/*
 * test_complete.c — TDD for completion count + history hints (NEW-23)
 */

#include "acutest.h"
#include "petrush/complete.h"
#include "linenoise.h"

#include <string.h>
#include <stdlib.h>

void test_complete_count_empty_has_builtins(void)
{
    int n = petrush_complete_count("");
    TEST_CHECK(n >= 5); /* cd, pwd, echo, ... */
}

void test_complete_count_cd_prefix(void)
{
    int n = petrush_complete_count("c");
    TEST_CHECK(n >= 1); /* cd, clear, ... */
}

void test_history_hint_suffix(void)
{
    linenoiseHistorySetMaxLen(100);
    linenoiseHistoryAdd("echo hello world");
    char *h = petrush_history_hint("echo he");
    TEST_CHECK(h != NULL);
    if (h) {
        TEST_CHECK(strcmp(h, "llo world") == 0);
        free(h);
    }
}

void test_history_hint_none(void)
{
    char *h = petrush_history_hint("zzz_no_such_prefix_xyz");
    TEST_CHECK(h == NULL);
}

TEST_LIST = {
    { "complete_count_empty", test_complete_count_empty_has_builtins },
    { "complete_count_cd_prefix", test_complete_count_cd_prefix },
    { "history_hint_suffix", test_history_hint_suffix },
    { "history_hint_none", test_history_hint_none },
    { NULL, NULL }
};
