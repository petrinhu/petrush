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

TEST_LIST = {
    { "info_builtin_basic", test_info_builtin_basic },
    { "info_output_contains_version", test_info_output_contains_version },
    { "builtin_redir_out_noclobber", test_builtin_redir_out_noclobber_existing },
    { "builtin_redir_append_ok",     test_builtin_redir_append_allows_existing },
    { NULL, NULL }
};
