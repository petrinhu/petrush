/*
 * test_expand.c — TDD UX-12 tilde + UX-13 $VAR + FEAT-PARAM
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

/* FEAT-PARAM: ${VAR:-word} ${VAR:+word} ${#VAR} */
void test_param_default_unset(void)
{
    petrush_unsetenv("FEAT_PARAM_A");
    char *e = expand_word("${FEAT_PARAM_A:-fallback}");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "fallback") == 0);
    free(e);
}

void test_param_default_empty(void)
{
    petrush_setenv("FEAT_PARAM_A", "", 1);
    char *e = expand_word("${FEAT_PARAM_A:-fallback}");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "fallback") == 0);
    free(e);
}

void test_param_default_set(void)
{
    petrush_setenv("FEAT_PARAM_A", "real", 1);
    char *e = expand_word("${FEAT_PARAM_A:-fallback}");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "real") == 0);
    free(e);
}

void test_param_default_empty_word(void)
{
    petrush_unsetenv("FEAT_PARAM_A");
    char *e = expand_word("${FEAT_PARAM_A:-}");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "") == 0);
    free(e);
}

void test_param_alternate_set(void)
{
    petrush_setenv("FEAT_PARAM_B", "yes", 1);
    char *e = expand_word("${FEAT_PARAM_B:+alt}");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "alt") == 0);
    free(e);
}

void test_param_alternate_unset(void)
{
    petrush_unsetenv("FEAT_PARAM_B");
    char *e = expand_word("${FEAT_PARAM_B:+alt}");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "") == 0);
    free(e);
}

void test_param_alternate_empty(void)
{
    petrush_setenv("FEAT_PARAM_B", "", 1);
    char *e = expand_word("${FEAT_PARAM_B:+alt}");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "") == 0);
    free(e);
}

void test_param_length_set(void)
{
    petrush_setenv("FEAT_PARAM_C", "abcd", 1);
    char *e = expand_word("${#FEAT_PARAM_C}");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "4") == 0);
    free(e);
}

void test_param_length_unset(void)
{
    petrush_unsetenv("FEAT_PARAM_C");
    char *e = expand_word("${#FEAT_PARAM_C}");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "0") == 0);
    free(e);
}

void test_param_embedded(void)
{
    petrush_unsetenv("FEAT_PARAM_D");
    char *e = expand_word("pre_${FEAT_PARAM_D:-x}_post");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "pre_x_post") == 0);
    free(e);
}

TEST_LIST = {
    { "tilde_alone", test_tilde_alone },
    { "tilde_slash", test_tilde_slash },
    { "var_simple", test_var_simple },
    { "var_braces", test_var_braces },
    { "no_expand_literal", test_no_expand_literal },
    { "expand_redir_err", test_expand_redir_err },
    { "param_default_unset", test_param_default_unset },
    { "param_default_empty", test_param_default_empty },
    { "param_default_set", test_param_default_set },
    { "param_default_empty_word", test_param_default_empty_word },
    { "param_alternate_set", test_param_alternate_set },
    { "param_alternate_unset", test_param_alternate_unset },
    { "param_alternate_empty", test_param_alternate_empty },
    { "param_length_set", test_param_length_set },
    { "param_length_unset", test_param_length_unset },
    { "param_embedded", test_param_embedded },
    { NULL, NULL }
};
