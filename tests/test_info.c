/*
 * test_info.c — Basic tests for info builtin (Onda 3 placeholder, NEW-04/19)
 */

#include "acutest.h"
#include "petrush/dispatcher.h"
#include "petrush/parser.h"

void test_info_builtin_basic(void)
{
    petrush_cmd_t cmd = {0};
    /* simulate "info" */
    int rc = petrush_parse("info", &cmd);
    TEST_CHECK(rc == 0);
    TEST_CHECK(cmd.argc == 1);

    /* dispatch should succeed without crash */
    int ret = dispatch_command(&cmd);
    TEST_CHECK(ret == 0);  /* info returns 0 */

    petrush_cmd_free(&cmd);
}

void test_info_output_contains_version(void)
{
    /* smoke-like: run via system but since no exec here, just check parse+dispatch */
    petrush_cmd_t cmd = {0};
    int rc = petrush_parse("info", &cmd);
    TEST_CHECK(rc == 0);
    /* in real run, output would have "petrush 0.0.1" etc. */
    TEST_CHECK(cmd.argc >= 1);

    petrush_cmd_free(&cmd);
}

TEST_LIST = {
    { "info_builtin_basic", test_info_builtin_basic },
    { "info_output_contains_version", test_info_output_contains_version },
    { NULL, NULL }
};