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

/* OSH-11: $(( )) avalia; nao passa pelo hook de cmdsubst. */
void test_osh9_arith_not_cmdsubst(void)
{
    g_osh9_stub_calls = 0;
    petrush_set_cmdsubst_hook(osh9_stub_cmdsubst);
    char *e = expand_word("$((1+1))");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "2") == 0);
    TEST_CHECK(g_osh9_stub_calls == 0);
    free(e);
    petrush_set_cmdsubst_hook(NULL);
}

/* OSH-11 TDD: hook NULL prova que nao e caminho $( ). */
void test_osh11_arith_one_plus_one(void)
{
    petrush_set_cmdsubst_hook(NULL);
    char *e = expand_word("$((1+1))");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "2") == 0);
    free(e);
}

void test_osh11_arith_concat(void)
{
    petrush_set_cmdsubst_hook(NULL);
    char *e = expand_word("pre$((1))post");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "pre1post") == 0);
    free(e);
}

void test_osh11_arith_unary_parens(void)
{
    petrush_set_cmdsubst_hook(NULL);
    char *e = expand_word("$((-(2+3)*4))");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "-20") == 0);
    free(e);
}

void test_osh11_arith_var(void)
{
    petrush_setenv("OSH11_N", "7", 1);
    petrush_set_cmdsubst_hook(NULL);
    char *e = expand_word("$((OSH11_N+1))");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "8") == 0);
    free(e);
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

/* OSH-14: helper unquoted here-doc — $VAR, sem tilde, quoted path via expand_cmd */
static void osh14_free_cmd(petrush_cmd_t *cmd)
{
    if (!cmd) {
        return;
    }
    if (cmd->argv) {
        for (int i = 0; i < cmd->argc; i++) {
            free(cmd->argv[i]);
        }
        free(cmd->argv);
    }
    free(cmd->argv_quoted);
    free(cmd->redir_in);
    free(cmd->redir_out);
    free(cmd->redir_err);
    free(cmd->here_delim);
    free(cmd->here_body);
    memset(cmd, 0, sizeof(*cmd));
}

static int osh14_make_cat_cmd(petrush_cmd_t *cmd, const char *body, int quoted)
{
    memset(cmd, 0, sizeof(*cmd));
    cmd->argv = malloc(sizeof(char *) * 2);
    if (!cmd->argv) {
        return -1;
    }
    cmd->argv[0] = strdup("cat");
    cmd->argv[1] = NULL;
    cmd->argc = 1;
    if (!cmd->argv[0]) {
        return -1;
    }
    cmd->here_delim = strdup("EOF");
    cmd->here_body = strdup(body);
    cmd->here_quoted = quoted;
    if (!cmd->here_delim || !cmd->here_body) {
        return -1;
    }
    return 0;
}

void test_osh14_heredoc_body_var(void)
{
    petrush_setenv("OSH14_FOO", "bar", 1);
    char *e = expand_heredoc_body("$OSH14_FOO\n");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "bar\n") == 0);
    free(e);
}

void test_osh14_heredoc_body_no_tilde(void)
{
    petrush_setenv("HOME", "/home/tester", 1);
    char *e = expand_heredoc_body("~/x\n");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "~/x\n") == 0);
    free(e);
    /* regressao: expand_word ainda faz tilde */
    char *w = expand_word("~/x");
    TEST_CHECK(w != NULL);
    TEST_CHECK(strcmp(w, "/home/tester/x") == 0);
    free(w);
}

void test_osh14_heredoc_body_arith(void)
{
    petrush_set_cmdsubst_hook(NULL);
    char *e = expand_heredoc_body("$((1+1))\n");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "2\n") == 0);
    free(e);
}

void test_osh14_heredoc_body_cmdsubst(void)
{
    g_osh9_stub_calls = 0;
    petrush_set_cmdsubst_hook(osh9_stub_cmdsubst);
    char *e = expand_heredoc_body("$(echo X)\n");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "X\n") == 0);
    TEST_CHECK(g_osh9_stub_calls == 1);
    free(e);
    petrush_set_cmdsubst_hook(NULL);
}

void test_osh14_heredoc_body_backslash_dollar(void)
{
    petrush_setenv("OSH14_FOO", "bar", 1);
    char *e = expand_heredoc_body("\\$OSH14_FOO\n");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "$OSH14_FOO\n") == 0);
    free(e);
}

void test_osh14_heredoc_body_quotes_not_special(void)
{
    petrush_setenv("OSH14_FOO", "bar", 1);
    char *e = expand_heredoc_body("\"$OSH14_FOO\"\n");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "\"bar\"\n") == 0);
    free(e);
}

void test_osh14_expand_cmd_unquoted(void)
{
    petrush_setenv("OSH14_FOO", "bar", 1);
    petrush_cmd_t cmd;
    TEST_CHECK(osh14_make_cat_cmd(&cmd, "$OSH14_FOO\n", 0) == 0);
    expand_cmd_argv(&cmd);
    TEST_CHECK(cmd.here_body != NULL);
    TEST_CHECK(strcmp(cmd.here_body, "bar\n") == 0);
    osh14_free_cmd(&cmd);
}

void test_osh14_expand_cmd_quoted_literal(void)
{
    petrush_setenv("OSH14_FOO", "bar", 1);
    petrush_cmd_t cmd;
    TEST_CHECK(osh14_make_cat_cmd(&cmd, "$OSH14_FOO\n", 1) == 0);
    expand_cmd_argv(&cmd);
    TEST_CHECK(cmd.here_body != NULL);
    TEST_CHECK(strcmp(cmd.here_body, "$OSH14_FOO\n") == 0);
    osh14_free_cmd(&cmd);
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

/* OSH-16: $? / $- / shellopt (x); C always-on em $-. */
void test_osh16_dollar_question_default_zero(void)
{
    petrush_shellopt_reset_for_tests();
    char *e = expand_word("$?");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "0") == 0);
    free(e);
}

void test_osh16_dollar_question_after_set(void)
{
    petrush_shellopt_reset_for_tests();
    petrush_last_status_set(7);
    char *e = expand_word("$?");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "7") == 0);
    free(e);
    petrush_last_status_set(0);
    e = expand_word("$?");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strcmp(e, "0") == 0);
    free(e);
}

void test_osh16_dollar_minus_has_C(void)
{
    petrush_shellopt_reset_for_tests();
    char *e = expand_word("$-");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strchr(e, 'C') != NULL);
    TEST_CHECK(strchr(e, 'x') == NULL);
    free(e);
}

void test_osh16_dollar_minus_x_when_on(void)
{
    petrush_shellopt_reset_for_tests();
    TEST_CHECK(petrush_shellopt_set('x', 1) == 0);
    TEST_CHECK(petrush_shellopt_get('x') == 1);
    char *e = expand_word("$-");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strchr(e, 'C') != NULL);
    TEST_CHECK(strchr(e, 'x') != NULL);
    free(e);
    TEST_CHECK(petrush_shellopt_set('x', 0) == 0);
    TEST_CHECK(petrush_shellopt_get('x') == 0);
    e = expand_word("$-");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strchr(e, 'x') == NULL);
    free(e);
}

void test_osh16_shellopt_rejects_unknown_flag(void)
{
    petrush_shellopt_reset_for_tests();
    TEST_CHECK(petrush_shellopt_set('z', 1) != 0);
    TEST_CHECK(petrush_shellopt_get('z') == 0);
}

void test_osh16_heredoc_body_status_and_flags(void)
{
    petrush_shellopt_reset_for_tests();
    petrush_last_status_set(3);
    TEST_CHECK(petrush_shellopt_set('x', 1) == 0);
    char *e = expand_heredoc_body("st=$? fl=$-\n");
    TEST_CHECK(e != NULL);
    TEST_CHECK(strstr(e, "st=3") != NULL);
    TEST_CHECK(strstr(e, "fl=") != NULL);
    TEST_CHECK(strchr(e, 'C') != NULL);
    TEST_CHECK(strchr(e, 'x') != NULL);
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
    { "osh11_arith_one_plus_one", test_osh11_arith_one_plus_one },
    { "osh11_arith_concat", test_osh11_arith_concat },
    { "osh11_arith_unary_parens", test_osh11_arith_unary_parens },
    { "osh11_arith_var", test_osh11_arith_var },
    { "osh9_backticks_not_expanded", test_osh9_backticks_not_expanded },
    { "osh14_heredoc_body_var", test_osh14_heredoc_body_var },
    { "osh14_heredoc_body_no_tilde", test_osh14_heredoc_body_no_tilde },
    { "osh14_heredoc_body_arith", test_osh14_heredoc_body_arith },
    { "osh14_heredoc_body_cmdsubst", test_osh14_heredoc_body_cmdsubst },
    { "osh14_heredoc_body_backslash_dollar", test_osh14_heredoc_body_backslash_dollar },
    { "osh14_heredoc_body_quotes_not_special", test_osh14_heredoc_body_quotes_not_special },
    { "osh14_expand_cmd_unquoted", test_osh14_expand_cmd_unquoted },
    { "osh14_expand_cmd_quoted_literal", test_osh14_expand_cmd_quoted_literal },
    { "osh16_dollar_question_default_zero", test_osh16_dollar_question_default_zero },
    { "osh16_dollar_question_after_set", test_osh16_dollar_question_after_set },
    { "osh16_dollar_minus_has_C", test_osh16_dollar_minus_has_C },
    { "osh16_dollar_minus_x_when_on", test_osh16_dollar_minus_x_when_on },
    { "osh16_shellopt_rejects_unknown_flag", test_osh16_shellopt_rejects_unknown_flag },
    { "osh16_heredoc_body_status_and_flags", test_osh16_heredoc_body_status_and_flags },
    { NULL, NULL }
};
