/*
 * test_expand.c — TDD UX-12 tilde + UX-13 $VAR (NEW expand)
 */

#include "acutest.h"
#include "petrush/expand.h"
#include "petrush/env.h"
#include "petrush/parser.h"

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

/* UX-16: expandir redir_err como in/out */
void test_expand_redir_err(void)
{
    petrush_setenv("FOO_PETRUSH", "barval", 1);
    petrush_setenv("HOME", "/home/tester", 1);

    petrush_cmd_t cmd = {0};
    cmd.argv = malloc(sizeof(char *) * 2);
    TEST_CHECK(cmd.argv != NULL);
    cmd.argv[0] = strdup("echo");
    cmd.argv[1] = NULL;
    cmd.argc = 1;
    cmd.redir_err = strdup("$FOO_PETRUSH");
    TEST_CHECK(cmd.redir_err != NULL);
    expand_cmd_argv(&cmd);
    TEST_CHECK(cmd.redir_err && strcmp(cmd.redir_err, "barval") == 0);
    free(cmd.argv[0]);
    free(cmd.argv);
    free(cmd.redir_err);

    memset(&cmd, 0, sizeof(cmd));
    cmd.argv = malloc(sizeof(char *) * 2);
    TEST_CHECK(cmd.argv != NULL);
    cmd.argv[0] = strdup("echo");
    cmd.argv[1] = NULL;
    cmd.argc = 1;
    cmd.redir_err = strdup("~/e");
    TEST_CHECK(cmd.redir_err != NULL);
    expand_cmd_argv(&cmd);
    TEST_CHECK(cmd.redir_err && strcmp(cmd.redir_err, "/home/tester/e") == 0);
    free(cmd.argv[0]);
    free(cmd.argv);
    free(cmd.redir_err);
}

TEST_LIST = {
    { "tilde_alone", test_tilde_alone },
    { "tilde_slash", test_tilde_slash },
    { "var_simple", test_var_simple },
    { "var_braces", test_var_braces },
    { "no_expand_literal", test_no_expand_literal },
    { "expand_redir_err", test_expand_redir_err },
    { NULL, NULL }
};
