/*
 * test_hist_expand.c - TDD for !! / !n / !$ / !^ (NEW-26 + FEAT-BANG)
 * ARCH-03: fixture via ui_port bind (sem linenoise.h).
 */

#include "acutest.h"
#include "petrush/hist_expand.h"
#include "petrush/ui_port.h"

#include <string.h>
#include <stdlib.h>

#define FIX_MAX 64

static char *g_hist[FIX_MAX];
static int g_hist_n;

static void fix_clear_screen(void)
{
}

static int fix_history_len(void)
{
    return g_hist_n;
}

static const char *fix_history_get(int index)
{
    if (index < 0 || index >= g_hist_n) {
        return NULL;
    }
    return g_hist[index];
}

static void fix_reset(void)
{
    for (int i = 0; i < g_hist_n; i++) {
        free(g_hist[i]);
        g_hist[i] = NULL;
    }
    g_hist_n = 0;
}

static void fix_add(const char *line)
{
    if (g_hist_n >= FIX_MAX || !line) {
        return;
    }
    g_hist[g_hist_n] = strdup(line);
    if (g_hist[g_hist_n]) {
        g_hist_n++;
    }
}

static void fix_bind(void)
{
    petrush_ui_port_t port = {
        .clear_screen = fix_clear_screen,
        .history_len = fix_history_len,
        .history_get = fix_history_get,
    };
    petrush_ui_port_bind(&port);
}

void test_bang_bang_last(void)
{
    fix_reset();
    fix_bind();
    fix_add("echo first");
    fix_add("echo second");
    char *exp = hist_expand_line("!!");
    TEST_CHECK(exp != NULL);
    if (exp) {
        TEST_CHECK(strcmp(exp, "echo second") == 0);
        free(exp);
    }
    fix_reset();
    petrush_ui_port_bind(NULL);
}

void test_bang_number(void)
{
    fix_reset();
    fix_bind();
    fix_add("cmd_one");
    fix_add("cmd_two");
    char *exp = hist_expand_line("!1");
    TEST_CHECK(exp != NULL);
    if (exp) {
        TEST_CHECK(strcmp(exp, "cmd_one") == 0);
        free(exp);
    }
    fix_reset();
    petrush_ui_port_bind(NULL);
}

void test_no_expand(void)
{
    fix_reset();
    fix_bind();
    char *exp = hist_expand_line("echo hi");
    TEST_CHECK(exp == NULL);
    petrush_ui_port_bind(NULL);
}

/* FEAT-BANG: !$ = último arg do último evento */
void test_bang_dollar_last_arg(void)
{
    fix_reset();
    fix_bind();
    fix_add("echo hello world");
    char *exp = hist_expand_line("!$");
    TEST_CHECK(exp != NULL);
    if (exp) {
        TEST_CHECK(strcmp(exp, "world") == 0);
        free(exp);
    }
    fix_reset();
    petrush_ui_port_bind(NULL);
}

/* FEAT-BANG: !^ = primeiro arg (word 1) do último evento */
void test_bang_caret_first_arg(void)
{
    fix_reset();
    fix_bind();
    fix_add("cp src dst");
    char *exp = hist_expand_line("!^");
    TEST_CHECK(exp != NULL);
    if (exp) {
        TEST_CHECK(strcmp(exp, "src") == 0);
        free(exp);
    }
    fix_reset();
    petrush_ui_port_bind(NULL);
}

/* FEAT-BANG: !$ com uma só palavra = essa palavra (bash) */
void test_bang_dollar_single_word(void)
{
    fix_reset();
    fix_bind();
    fix_add("ls");
    char *exp = hist_expand_line("!$");
    TEST_CHECK(exp != NULL);
    if (exp) {
        TEST_CHECK(strcmp(exp, "ls") == 0);
        free(exp);
    }
    fix_reset();
    petrush_ui_port_bind(NULL);
}

/* FEAT-BANG: !^ sem args = falha (sem word 1) */
void test_bang_caret_no_arg(void)
{
    fix_reset();
    fix_bind();
    fix_add("pwd");
    char *exp = hist_expand_line("!^");
    TEST_CHECK(exp == NULL);
    fix_reset();
    petrush_ui_port_bind(NULL);
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
