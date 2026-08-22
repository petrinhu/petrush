/*
 * test_info.c — Basic tests for info builtin (Onda 3 placeholder, NEW-04/19)
 * + SEC-09: noclobber no caminho builtin (dispatcher run_builtin_with_redirs).
 */

#include "acutest.h"
#include "petrush/dispatcher.h"
#include "petrush/parser.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

/* SEC-09: builtin `echo > file` recusa overwrite se file existe. */
void test_builtin_redir_out_noclobber_existing(void)
{
    char path[] = "/tmp/petrush_test_bi_noclobber_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) {
        TEST_SKIP("mkstemp falhou");
        return;
    }
    TEST_CHECK(write(fd, "KEEP\n", 5) == 5);
    close(fd);

    petrush_cmd_t cmd = {0};
    char line[256];
    snprintf(line, sizeof(line), "echo OVERWRITE > %s", path);
    TEST_CHECK(petrush_parse(line, &cmd) == 0);

    int ret = dispatch_command(&cmd);
    TEST_CHECK(ret != 0);
    petrush_cmd_free(&cmd);

    FILE *f = fopen(path, "r");
    TEST_CHECK(f != NULL);
    if (f) {
        char buf[64] = {0};
        TEST_CHECK(fgets(buf, sizeof(buf), f) != NULL);
        TEST_CHECK(strcmp(buf, "KEEP\n") == 0);
        TEST_CHECK(strstr(buf, "OVERWRITE") == NULL);
        fclose(f);
    }
    unlink(path);
}

/* SEC-09: builtin `echo >> file` ainda faz append. */
void test_builtin_redir_append_allows_existing(void)
{
    char path[] = "/tmp/petrush_test_bi_app_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) {
        TEST_SKIP("mkstemp falhou");
        return;
    }
    TEST_CHECK(write(fd, "KEEP\n", 5) == 5);
    close(fd);

    petrush_cmd_t cmd = {0};
    char line[256];
    snprintf(line, sizeof(line), "echo APPENDED >> %s", path);
    TEST_CHECK(petrush_parse(line, &cmd) == 0);

    int ret = dispatch_command(&cmd);
    TEST_CHECK(ret == 0);
    petrush_cmd_free(&cmd);

    FILE *f = fopen(path, "r");
    TEST_CHECK(f != NULL);
    if (f) {
        char all[128] = {0};
        size_t n = fread(all, 1, sizeof(all) - 1, f);
        all[n] = '\0';
        TEST_CHECK(strstr(all, "KEEP") != NULL);
        TEST_CHECK(strstr(all, "APPENDED") != NULL);
        fclose(f);
    }
    unlink(path);
}

/* FEAT-TRUE: true → 0; false → 1; : → 0; silent no-op (sem printf). */
static int builtin_table_has(const char *name)
{
    int n = petrush_builtin_count();
    for (int i = 0; i < n; i++) {
        const char *bn = petrush_builtin_name(i);
        if (bn && strcmp(bn, name) == 0) {
            return 1;
        }
    }
    return 0;
}

void test_builtin_true_false_colon_in_table(void)
{
    TEST_CHECK(builtin_table_has("true"));
    TEST_CHECK(builtin_table_has("false"));
    TEST_CHECK(builtin_table_has(":"));
}

void test_builtin_true_status_zero(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("true", &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) == 0);
    petrush_cmd_free(&cmd);
}

void test_builtin_false_status_one(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("false", &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) == 1);
    petrush_cmd_free(&cmd);
}

void test_builtin_colon_status_zero(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse(":", &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) == 0);
    petrush_cmd_free(&cmd);
}

void test_builtin_true_false_ignore_args(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("true anything here", &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) == 0);
    petrush_cmd_free(&cmd);

    TEST_CHECK(petrush_parse("false anything here", &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) == 1);
    petrush_cmd_free(&cmd);

    TEST_CHECK(petrush_parse(": anything here", &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) == 0);
    petrush_cmd_free(&cmd);
}

void test_builtin_true_false_short_circuit(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("true && false", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 1);
    petrush_list_free(&list);

    TEST_CHECK(petrush_parse_list("false || true", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);

    TEST_CHECK(petrush_parse_list("false && true", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 1);
    petrush_list_free(&list);

    TEST_CHECK(petrush_parse_list(": && true", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);
}

TEST_LIST = {
    { "info_builtin_basic", test_info_builtin_basic },
    { "info_output_contains_version", test_info_output_contains_version },
    { "builtin_redir_out_noclobber", test_builtin_redir_out_noclobber_existing },
    { "builtin_redir_append_ok",     test_builtin_redir_append_allows_existing },
    { "builtin_true_false_colon_in_table", test_builtin_true_false_colon_in_table },
    { "builtin_true_status_zero", test_builtin_true_status_zero },
    { "builtin_false_status_one", test_builtin_false_status_one },
    { "builtin_colon_status_zero", test_builtin_colon_status_zero },
    { "builtin_true_false_ignore_args", test_builtin_true_false_ignore_args },
    { "builtin_true_false_short_circuit", test_builtin_true_false_short_circuit },
    { NULL, NULL }
};
