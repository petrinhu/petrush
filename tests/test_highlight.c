/*
 * test_highlight.c — TDD UX-21 highlight mínimo (aspas + token grosso)
 */

#include "acutest.h"
#include "petrush/highlight.h"

#include <stdlib.h>
#include <string.h>

static int span_is(const petrush_hl_span_t *s, size_t start, size_t len,
                   enum petrush_hl_kind kind)
{
    return s->start == start && s->len == len && s->kind == kind;
}

static char *strip_csi(const char *s)
{
    size_t n = strlen(s);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < n; ) {
        if (s[i] == '\033' && i + 1 < n && s[i + 1] == '[') {
            i += 2;
            while (i < n && !((s[i] >= '@' && s[i] <= '~'))) i++;
            if (i < n) i++;
            continue;
        }
        out[j++] = s[i++];
    }
    out[j] = '\0';
    return out;
}

void test_scan_echo_hi(void)
{
    petrush_hl_span_t sp[16];
    const char *buf = "echo hi";
    int n = petrush_hl_scan(buf, strlen(buf), sp, 16);
    TEST_CHECK(n >= 1);
    TEST_CHECK(span_is(&sp[0], 0, 4, PETRUSH_HL_CMD));
    int plain = 0;
    for (int i = 1; i < n; i++) {
        if (sp[i].kind == PETRUSH_HL_PLAIN) plain = 1;
        TEST_CHECK(sp[i].kind != PETRUSH_HL_CMD);
    }
    TEST_CHECK(plain || n == 1);
}

void test_scan_closed_double(void)
{
    petrush_hl_span_t sp[16];
    const char *buf = "echo \"hi\"";
    int n = petrush_hl_scan(buf, strlen(buf), sp, 16);
    TEST_CHECK(n >= 2);
    TEST_CHECK(span_is(&sp[0], 0, 4, PETRUSH_HL_CMD));
    int found_str = 0;
    for (int i = 0; i < n; i++) {
        if (sp[i].kind == PETRUSH_HL_STR) {
            found_str = 1;
            TEST_CHECK(sp[i].start == 5 && sp[i].len == 4); /* "hi" */
        }
        TEST_CHECK(sp[i].kind != PETRUSH_HL_UNCLOSED);
    }
    TEST_CHECK(found_str);
}

void test_scan_unclosed_double(void)
{
    petrush_hl_span_t sp[16];
    const char *buf = "echo \"hi";
    int n = petrush_hl_scan(buf, strlen(buf), sp, 16);
    TEST_CHECK(n >= 2);
    int found = 0;
    for (int i = 0; i < n; i++) {
        if (sp[i].kind == PETRUSH_HL_UNCLOSED) {
            found = 1;
            TEST_CHECK(sp[i].start == 5);
            TEST_CHECK(sp[i].start + sp[i].len == strlen(buf));
        }
        TEST_CHECK(sp[i].kind != PETRUSH_HL_STR);
    }
    TEST_CHECK(found);
}

void test_scan_single_embeds_double(void)
{
    petrush_hl_span_t sp[16];
    const char *buf = "echo 'a\"b'";
    int n = petrush_hl_scan(buf, strlen(buf), sp, 16);
    TEST_CHECK(n >= 2);
    int found = 0;
    for (int i = 0; i < n; i++) {
        if (sp[i].kind == PETRUSH_HL_STR) {
            found = 1;
            TEST_CHECK(sp[i].start == 5 && sp[i].len == 5); /* 'a"b' */
        }
    }
    TEST_CHECK(found);
}

void test_scan_pipe_second_cmd(void)
{
    petrush_hl_span_t sp[16];
    const char *buf = "echo | cat";
    int n = petrush_hl_scan(buf, strlen(buf), sp, 16);
    TEST_CHECK(n >= 3);
    TEST_CHECK(span_is(&sp[0], 0, 4, PETRUSH_HL_CMD));
    int saw_op = 0, saw_cat = 0;
    for (int i = 0; i < n; i++) {
        if (sp[i].kind == PETRUSH_HL_OP && sp[i].len == 1 &&
            buf[sp[i].start] == '|')
            saw_op = 1;
        if (sp[i].kind == PETRUSH_HL_CMD && sp[i].len == 3 &&
            memcmp(buf + sp[i].start, "cat", 3) == 0)
            saw_cat = 1;
    }
    TEST_CHECK(saw_op);
    TEST_CHECK(saw_cat);
}

void test_scan_and_and(void)
{
    petrush_hl_span_t sp[16];
    const char *buf = "true && false";
    int n = petrush_hl_scan(buf, strlen(buf), sp, 16);
    TEST_CHECK(n >= 3);
    int saw_op = 0, saw_false = 0;
    for (int i = 0; i < n; i++) {
        if (sp[i].kind == PETRUSH_HL_OP && sp[i].len == 2 &&
            memcmp(buf + sp[i].start, "&&", 2) == 0)
            saw_op = 1;
        if (sp[i].kind == PETRUSH_HL_CMD && sp[i].len == 5 &&
            memcmp(buf + sp[i].start, "false", 5) == 0)
            saw_false = 1;
    }
    TEST_CHECK(saw_op);
    TEST_CHECK(saw_false);
}

void test_scan_semicolon(void)
{
    petrush_hl_span_t sp[16];
    const char *buf = "echo a; echo b";
    int n = petrush_hl_scan(buf, strlen(buf), sp, 16);
    TEST_CHECK(n >= 3);
    int saw_semi = 0, second_echo = 0;
    for (int i = 0; i < n; i++) {
        if (sp[i].kind == PETRUSH_HL_OP && sp[i].len == 1 &&
            buf[sp[i].start] == ';')
            saw_semi = 1;
        if (sp[i].kind == PETRUSH_HL_CMD && sp[i].start == 8 &&
            sp[i].len == 4)
            second_echo = 1;
    }
    TEST_CHECK(saw_semi);
    TEST_CHECK(second_echo);
}

void test_scan_errtoout(void)
{
    petrush_hl_span_t sp[16];
    const char *buf = "echo 2>&1";
    int n = petrush_hl_scan(buf, strlen(buf), sp, 16);
    TEST_CHECK(n >= 2);
    int found = 0;
    for (int i = 0; i < n; i++) {
        if (sp[i].kind == PETRUSH_HL_OP && sp[i].len == 4 &&
            memcmp(buf + sp[i].start, "2>&1", 4) == 0)
            found = 1;
        TEST_CHECK(!(sp[i].kind == PETRUSH_HL_CMD && sp[i].len == 4 &&
                     memcmp(buf + sp[i].start, "2>&1", 4) == 0));
    }
    TEST_CHECK(found);
}

void test_scan_empty_quotes(void)
{
    petrush_hl_span_t sp[16];
    const char *d = "\"\"";
    int n = petrush_hl_scan(d, 2, sp, 16);
    TEST_CHECK(n == 1);
    TEST_CHECK(span_is(&sp[0], 0, 2, PETRUSH_HL_STR));

    const char *s = "''";
    n = petrush_hl_scan(s, 2, sp, 16);
    TEST_CHECK(n == 1);
    TEST_CHECK(span_is(&sp[0], 0, 2, PETRUSH_HL_STR));
}

void test_scan_null_empty(void)
{
    petrush_hl_span_t sp[4];
    TEST_CHECK(petrush_hl_scan(NULL, 0, sp, 4) == 0);
    TEST_CHECK(petrush_hl_scan("", 0, sp, 4) == 0);
    char *c = petrush_hl_colorize(NULL, 0);
    TEST_CHECK(c == NULL);
    c = petrush_hl_colorize("", 0);
    TEST_CHECK(c != NULL);
    if (c) {
        TEST_CHECK(c[0] == '\0');
        free(c);
    }
}

void test_colorize_strip_invariant(void)
{
    const char *inputs[] = {
        "echo hi",
        "echo \"hi\"",
        "echo \"hi",
        "echo 'a\"b'",
        "echo | cat",
        "true && false",
        "echo a; echo b",
        "echo 2>&1",
        "\"\"",
        "''",
        "",
    };
    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
        const char *in = inputs[i];
        char *colored = petrush_hl_colorize(in, strlen(in));
        TEST_CHECK(colored != NULL);
        if (!colored) continue;
        char *stripped = strip_csi(colored);
        TEST_CHECK(stripped != NULL);
        if (stripped) {
            TEST_CHECK(strcmp(stripped, in) == 0);
            free(stripped);
        }
        free(colored);
    }
}

void test_colorize_csi_kinds(void)
{
    char *u = petrush_hl_colorize("echo \"hi", strlen("echo \"hi"));
    TEST_CHECK(u != NULL);
    if (u) {
        TEST_CHECK(strstr(u, "\033[1;31m") != NULL);
        free(u);
    }
    char *s = petrush_hl_colorize("echo \"hi\"", strlen("echo \"hi\""));
    TEST_CHECK(s != NULL);
    if (s) {
        TEST_CHECK(strstr(s, "\033[33m") != NULL);
        TEST_CHECK(strstr(s, "\033[1;31m") == NULL);
        free(s);
    }
}

TEST_LIST = {
    { "scan_echo_hi", test_scan_echo_hi },
    { "scan_closed_double", test_scan_closed_double },
    { "scan_unclosed_double", test_scan_unclosed_double },
    { "scan_single_embeds_double", test_scan_single_embeds_double },
    { "scan_pipe_second_cmd", test_scan_pipe_second_cmd },
    { "scan_and_and", test_scan_and_and },
    { "scan_semicolon", test_scan_semicolon },
    { "scan_errtoout", test_scan_errtoout },
    { "scan_empty_quotes", test_scan_empty_quotes },
    { "scan_null_empty", test_scan_null_empty },
    { "colorize_strip_invariant", test_colorize_strip_invariant },
    { "colorize_csi_kinds", test_colorize_csi_kinds },
    { NULL, NULL }
};
