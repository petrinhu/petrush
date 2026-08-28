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

/* OSH-1: posicionais $0 $1 $# $@ $* ; OSH-2: shift (sem ${1:-}) */
static void osh1_setup_abc(void)
{
    char *args[] = { "a", "b", "c" };
    TEST_CHECK(petrush_positional_set("/tmp/script.sh", 3, args) == 0);
}

void test_osh1_dollar_0(void)
{
    osh1_setup_abc();
    char *e = expand_word("$0");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "/tmp/script.sh") == 0);
    free(e);
    petrush_positional_clear();
}

void test_osh1_dollar_1_2(void)
{
    osh1_setup_abc();
    char *e1 = expand_word("$1");
    char *e2 = expand_word("$2");
    TEST_CHECK(e1 && strcmp(e1, "a") == 0);
    TEST_CHECK(e2 && strcmp(e2, "b") == 0);
    free(e1);
    free(e2);
    petrush_positional_clear();
}

void test_osh1_dollar_hash(void)
{
    osh1_setup_abc();
    char *e = expand_word("$#");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "3") == 0);
    free(e);
    petrush_positional_clear();
}

void test_osh1_dollar_1_unset_empty(void)
{
    TEST_CHECK(petrush_positional_set("petrush", 0, NULL) == 0);
    char *e = expand_word("$1");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "") == 0);
    free(e);
    petrush_positional_clear();
}

void test_osh1_dollar_10_is_1_then_0(void)
{
    /* POSIX sem braces: $10 = $1 + "0" */
    char *args[] = { "X" };
    TEST_CHECK(petrush_positional_set("sh", 1, args) == 0);
    char *e = expand_word("$10");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "X0") == 0);
    free(e);
    petrush_positional_clear();
}

void test_osh1_at_quoted_splice(void)
{
    osh1_setup_abc();
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo \"$@\"", &cmd) == 0);
    expand_cmd_argv(&cmd);
    TEST_CHECK(cmd.argc == 4);
    if (cmd.argc == 4) {
        TEST_CHECK(strcmp(cmd.argv[0], "echo") == 0);
        TEST_CHECK(strcmp(cmd.argv[1], "a") == 0);
        TEST_CHECK(strcmp(cmd.argv[2], "b") == 0);
        TEST_CHECK(strcmp(cmd.argv[3], "c") == 0);
    }
    petrush_cmd_free(&cmd);
    petrush_positional_clear();
}

void test_osh1_star_quoted_one_word(void)
{
    osh1_setup_abc();
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo \"$*\"", &cmd) == 0);
    expand_cmd_argv(&cmd);
    TEST_CHECK(cmd.argc == 2);
    if (cmd.argc == 2) {
        TEST_CHECK(strcmp(cmd.argv[1], "a b c") == 0);
    }
    petrush_cmd_free(&cmd);
    petrush_positional_clear();
}

void test_osh1_at_unquoted_words(void)
{
    osh1_setup_abc();
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo $@", &cmd) == 0);
    expand_cmd_argv(&cmd);
    TEST_CHECK(cmd.argc == 4);
    if (cmd.argc == 4) {
        TEST_CHECK(strcmp(cmd.argv[1], "a") == 0);
        TEST_CHECK(strcmp(cmd.argv[2], "b") == 0);
        TEST_CHECK(strcmp(cmd.argv[3], "c") == 0);
    }
    petrush_cmd_free(&cmd);
    petrush_positional_clear();
}

void test_osh1_at_quoted_empty_removes(void)
{
    TEST_CHECK(petrush_positional_set("sh", 0, NULL) == 0);
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo \"$@\"", &cmd) == 0);
    expand_cmd_argv(&cmd);
    TEST_CHECK(cmd.argc == 1);
    if (cmd.argc == 1) {
        TEST_CHECK(strcmp(cmd.argv[0], "echo") == 0);
    }
    petrush_cmd_free(&cmd);
    petrush_positional_clear();
}

void test_osh1_star_ifs_first_char(void)
{
    osh1_setup_abc();
    petrush_setenv("IFS", ":|", 1);
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo \"$*\"", &cmd) == 0);
    expand_cmd_argv(&cmd);
    TEST_CHECK(cmd.argc == 2);
    if (cmd.argc == 2) {
        TEST_CHECK(strcmp(cmd.argv[1], "a:b:c") == 0);
    }
    petrush_cmd_free(&cmd);
    petrush_unsetenv("IFS");
    petrush_positional_clear();
}

/* OSH-2: petrush_positional_shift */
void test_osh2_shift_default_one(void)
{
    osh1_setup_abc();
    TEST_CHECK(petrush_positional_shift(1) == 0);
    TEST_CHECK(petrush_positional_count() == 2);
    TEST_CHECK(strcmp(petrush_positional_get(0), "/tmp/script.sh") == 0);
    TEST_CHECK(strcmp(petrush_positional_get(1), "b") == 0);
    TEST_CHECK(strcmp(petrush_positional_get(2), "c") == 0);
    TEST_CHECK(petrush_positional_get(3) == NULL);
    petrush_positional_clear();
}

void test_osh2_shift_two(void)
{
    osh1_setup_abc();
    TEST_CHECK(petrush_positional_shift(2) == 0);
    TEST_CHECK(petrush_positional_count() == 1);
    TEST_CHECK(strcmp(petrush_positional_get(0), "/tmp/script.sh") == 0);
    TEST_CHECK(strcmp(petrush_positional_get(1), "c") == 0);
    TEST_CHECK(petrush_positional_get(2) == NULL);
    petrush_positional_clear();
}

void test_osh2_shift_zero_noop(void)
{
    osh1_setup_abc();
    TEST_CHECK(petrush_positional_shift(0) == 0);
    TEST_CHECK(petrush_positional_count() == 3);
    TEST_CHECK(strcmp(petrush_positional_get(1), "a") == 0);
    TEST_CHECK(strcmp(petrush_positional_get(2), "b") == 0);
    TEST_CHECK(strcmp(petrush_positional_get(3), "c") == 0);
    petrush_positional_clear();
}

void test_osh2_shift_too_many_intact(void)
{
    osh1_setup_abc();
    TEST_CHECK(petrush_positional_shift(4) != 0);
    TEST_CHECK(petrush_positional_count() == 3);
    TEST_CHECK(strcmp(petrush_positional_get(1), "a") == 0);
    TEST_CHECK(strcmp(petrush_positional_get(2), "b") == 0);
    TEST_CHECK(strcmp(petrush_positional_get(3), "c") == 0);
    TEST_CHECK(strcmp(petrush_positional_get(0), "/tmp/script.sh") == 0);
    petrush_positional_clear();
}

/* OSH-9: stub DIP (test_expand NAO liga dispatcher) */
static int g_osh9_stub_calls;
static char *osh9_stub_cmdsubst(const char *inner)
{
    g_osh9_stub_calls++;
    if (!inner) {
        return strdup("");
    }
    if (strcmp(inner, "echo hi") == 0) {
        return strdup("hi\n\n");
    }
    if (strcmp(inner, "echo X") == 0) {
        return strdup("X\n");
    }
    if (strcmp(inner, "printf hi") == 0) {
        return strdup("hi");
    }
    return strdup("STUB?");
}

void test_osh9_cmdsubst_strip_trailing_newlines(void)
{
    g_osh9_stub_calls = 0;
    petrush_set_cmdsubst_hook(osh9_stub_cmdsubst);
    char *e = expand_word("$(echo hi)");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "hi") == 0);
    TEST_CHECK(g_osh9_stub_calls == 1);
    free(e);
    petrush_set_cmdsubst_hook(NULL);
}

void test_osh9_cmdsubst_concat(void)
{
    g_osh9_stub_calls = 0;
    petrush_set_cmdsubst_hook(osh9_stub_cmdsubst);
    char *e = expand_word("pre$(echo X)post");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "preXpost") == 0);
    free(e);
    petrush_set_cmdsubst_hook(NULL);
}

void test_osh9_cmdsubst_hook_null_literal_dollar(void)
{
    petrush_set_cmdsubst_hook(NULL);
    char *e = expand_word("$(echo hi)");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "$(echo hi)") == 0);
    free(e);
}

void test_osh9_arith_not_cmdsubst(void)
{
    g_osh9_stub_calls = 0;
    petrush_set_cmdsubst_hook(osh9_stub_cmdsubst);
    char *e = expand_word("$((1+1))");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "$((1+1))") == 0);
    TEST_CHECK(g_osh9_stub_calls == 0);
    free(e);
    petrush_set_cmdsubst_hook(NULL);
}

void test_osh9_backticks_not_expanded(void)
{
    g_osh9_stub_calls = 0;
    petrush_set_cmdsubst_hook(osh9_stub_cmdsubst);
    char *e = expand_word("`echo hi`");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "`echo hi`") == 0);
    TEST_CHECK(g_osh9_stub_calls == 0);
    free(e);
    petrush_set_cmdsubst_hook(NULL);
}

void test_osh2_shift_all_leaves_empty(void)
{
    osh1_setup_abc();
    TEST_CHECK(petrush_positional_shift(3) == 0);
    TEST_CHECK(petrush_positional_count() == 0);
    TEST_CHECK(strcmp(petrush_positional_get(0), "/tmp/script.sh") == 0);
    TEST_CHECK(petrush_positional_get(1) == NULL);
    petrush_positional_clear();
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
    { "osh1_dollar_0", test_osh1_dollar_0 },
    { "osh1_dollar_1_2", test_osh1_dollar_1_2 },
    { "osh1_dollar_hash", test_osh1_dollar_hash },
    { "osh1_dollar_1_unset_empty", test_osh1_dollar_1_unset_empty },
    { "osh1_dollar_10_is_1_then_0", test_osh1_dollar_10_is_1_then_0 },
    { "osh1_at_quoted_splice", test_osh1_at_quoted_splice },
    { "osh1_star_quoted_one_word", test_osh1_star_quoted_one_word },
    { "osh1_at_unquoted_words", test_osh1_at_unquoted_words },
    { "osh1_at_quoted_empty_removes", test_osh1_at_quoted_empty_removes },
    { "osh1_star_ifs_first_char", test_osh1_star_ifs_first_char },
    { "osh2_shift_default_one", test_osh2_shift_default_one },
    { "osh2_shift_two", test_osh2_shift_two },
    { "osh2_shift_zero_noop", test_osh2_shift_zero_noop },
    { "osh2_shift_too_many_intact", test_osh2_shift_too_many_intact },
    { "osh2_shift_all_leaves_empty", test_osh2_shift_all_leaves_empty },
    { "osh9_cmdsubst_strip_trailing_newlines", test_osh9_cmdsubst_strip_trailing_newlines },
    { "osh9_cmdsubst_concat", test_osh9_cmdsubst_concat },
    { "osh9_cmdsubst_hook_null_literal_dollar", test_osh9_cmdsubst_hook_null_literal_dollar },
    { "osh9_arith_not_cmdsubst", test_osh9_arith_not_cmdsubst },
    { "osh9_backticks_not_expanded", test_osh9_backticks_not_expanded },
    { NULL, NULL }
};
