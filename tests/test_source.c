/*
 * test_source.c - UX-22: source / . roda arquivo no processo atual
 *
 * Teto PETRUSH_SOURCE_MAX_DEPTH, sem PATH search, fopen+fstat+rc_stat_ok,
 * argc==2, sem $1/return.
 */

#include "acutest.h"
#include "petrush/source.h"
#include "petrush/dispatcher.h"
#include "petrush/parser.h"
#include "petrush/env.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char *write_temp(const char *contents)
{
    char path[] = "/tmp/petrush_src_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) {
        return NULL;
    }
    if (contents && *contents) {
        size_t n = strlen(contents);
        if (write(fd, contents, n) != (ssize_t)n) {
            close(fd);
            unlink(path);
            return NULL;
        }
    }
    close(fd);
    if (chmod(path, 0600) != 0) {
        unlink(path);
        return NULL;
    }
    return strdup(path);
}

void test_source_argc_must_be_two(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("source", &cmd) == 0);
    TEST_CHECK(builtin_source(&cmd) != 0);
    petrush_cmd_free(&cmd);

    memset(&cmd, 0, sizeof(cmd));
    TEST_CHECK(petrush_parse("source a b", &cmd) == 0);
    TEST_CHECK(builtin_source(&cmd) != 0);
    petrush_cmd_free(&cmd);

    memset(&cmd, 0, sizeof(cmd));
    TEST_CHECK(petrush_parse(".", &cmd) == 0);
    TEST_CHECK(builtin_source(&cmd) != 0);
    petrush_cmd_free(&cmd);
}

void test_source_and_dot_same_builtin(void)
{
    char *path = write_temp("export UX22_SAME=1\n");
    if (!path) {
        TEST_SKIP("mkstemp falhou");
        return;
    }
    petrush_unsetenv("UX22_SAME");

    petrush_cmd_t cmd = {0};
    char line[512];
    snprintf(line, sizeof(line), "source %s", path);
    TEST_CHECK(petrush_parse(line, &cmd) == 0);
    TEST_CHECK(builtin_source(&cmd) == 0);
    petrush_cmd_free(&cmd);
    TEST_CHECK(petrush_getenv("UX22_SAME") != NULL);
    TEST_CHECK(strcmp(petrush_getenv("UX22_SAME"), "1") == 0);

    petrush_unsetenv("UX22_SAME");
    memset(&cmd, 0, sizeof(cmd));
    snprintf(line, sizeof(line), ". %s", path);
    TEST_CHECK(petrush_parse(line, &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) == 0);
    petrush_cmd_free(&cmd);
    TEST_CHECK(petrush_getenv("UX22_SAME") != NULL);
    TEST_CHECK(strcmp(petrush_getenv("UX22_SAME"), "1") == 0);

    unlink(path);
    free(path);
    petrush_unsetenv("UX22_SAME");
}

void test_source_missing_ok_silent(void)
{
    int rc = petrush_source_file("/tmp/petrush_no_such_source_ux22_zzz", 1);
    TEST_CHECK(rc == 0);
}

void test_source_missing_required_fails(void)
{
    int rc = petrush_source_file("/tmp/petrush_no_such_source_ux22_zzz", 0);
    TEST_CHECK(rc != 0);
}

void test_source_runs_in_current_process(void)
{
    char *path = write_temp("export UX22_CUR=alive\necho sourced-ok\n");
    if (!path) {
        TEST_SKIP("mkstemp falhou");
        return;
    }
    petrush_unsetenv("UX22_CUR");
    TEST_CHECK(petrush_source_file(path, 0) == 0);
    const char *v = petrush_getenv("UX22_CUR");
    TEST_CHECK(v != NULL);
    if (v) {
        TEST_CHECK(strcmp(v, "alive") == 0);
    }
    unlink(path);
    free(path);
    petrush_unsetenv("UX22_CUR");
}

void test_source_rejects_group_writable(void)
{
    char *path = write_temp("export UX22_BAD=1\n");
    if (!path) {
        TEST_SKIP("mkstemp falhou");
        return;
    }
    TEST_CHECK(chmod(path, 0664) == 0);
    petrush_unsetenv("UX22_BAD");
    TEST_CHECK(petrush_source_file(path, 0) != 0);
    TEST_CHECK(petrush_getenv("UX22_BAD") == NULL);
    unlink(path);
    free(path);
}

void test_source_no_path_search(void)
{
    char dir[] = "/tmp/petrush_src_path_XXXXXX";
    if (!mkdtemp(dir)) {
        TEST_SKIP("mkdtemp falhou");
        return;
    }
    char script[512];
    snprintf(script, sizeof(script), "%s/ux22only", dir);
    FILE *f = fopen(script, "w");
    if (!f) {
        rmdir(dir);
        TEST_SKIP("fopen falhou");
        return;
    }
    fputs("export UX22_PATH=1\n", f);
    fclose(f);
    chmod(script, 0700);

    char oldpath[4096];
    const char *prev = getenv("PATH");
    snprintf(oldpath, sizeof(oldpath), "%s", prev ? prev : "");
    char newpath[4608];
    snprintf(newpath, sizeof(newpath), "%s:%s", dir, oldpath);
    setenv("PATH", newpath, 1);

    petrush_unsetenv("UX22_PATH");
    /* nome sem barra: nao busca PATH */
    TEST_CHECK(petrush_source_file("ux22only", 0) != 0);
    TEST_CHECK(petrush_getenv("UX22_PATH") == NULL);

    setenv("PATH", oldpath, 1);
    unlink(script);
    rmdir(dir);
}

void test_source_depth_ceiling(void)
{
    char *paths[PETRUSH_SOURCE_MAX_DEPTH + 2];
    memset(paths, 0, sizeof(paths));

    for (int i = PETRUSH_SOURCE_MAX_DEPTH + 1; i >= 0; i--) {
        char body[640];
        if (i == PETRUSH_SOURCE_MAX_DEPTH + 1) {
            snprintf(body, sizeof(body), "export UX22_DEEP=bottom\n");
        } else {
            /* preenchido depois de criar o filho */
            body[0] = '\0';
        }
        paths[i] = write_temp(body[0] ? body : "# placeholder\n");
        if (!paths[i]) {
            for (int j = i + 1; j <= PETRUSH_SOURCE_MAX_DEPTH + 1; j++) {
                if (paths[j]) {
                    unlink(paths[j]);
                    free(paths[j]);
                }
            }
            TEST_SKIP("mkstemp falhou");
            return;
        }
    }

    for (int i = 0; i <= PETRUSH_SOURCE_MAX_DEPTH; i++) {
        char body[640];
        snprintf(body, sizeof(body), "source %s\n", paths[i + 1]);
        FILE *f = fopen(paths[i], "w");
        TEST_CHECK(f != NULL);
        if (!f) {
            goto cleanup;
        }
        fputs(body, f);
        fclose(f);
        chmod(paths[i], 0600);
    }

    petrush_unsetenv("UX22_DEEP");
    /* cadeia de MAX+2 niveis: deve falhar no teto e nao setar bottom */
    TEST_CHECK(petrush_source_file(paths[0], 0) != 0);
    TEST_CHECK(petrush_getenv("UX22_DEEP") == NULL);

    /* cadeia rasa (<= MAX) ok */
    petrush_unsetenv("UX22_DEEP");
    TEST_CHECK(petrush_source_file(paths[PETRUSH_SOURCE_MAX_DEPTH], 0) == 0);
    TEST_CHECK(petrush_getenv("UX22_DEEP") != NULL);

cleanup:
    for (int i = 0; i <= PETRUSH_SOURCE_MAX_DEPTH + 1; i++) {
        if (paths[i]) {
            unlink(paths[i]);
            free(paths[i]);
        }
    }
    petrush_unsetenv("UX22_DEEP");
}

void test_source_registered_in_table(void)
{
    int found_source = 0;
    int found_dot = 0;
    int n = petrush_builtin_count();
    for (int i = 0; i < n; i++) {
        const char *name = petrush_builtin_name(i);
        if (name && strcmp(name, "source") == 0) {
            found_source = 1;
        }
        if (name && strcmp(name, ".") == 0) {
            found_dot = 1;
        }
    }
    TEST_CHECK(found_source);
    TEST_CHECK(found_dot);
}

/* --- OSH-0: petrush_run_script --- */

void test_run_script_missing_is_127(void)
{
    int rc = petrush_run_script("/var/tmp/petrush_no_such_osh0_zzz");
    TEST_CHECK(rc == 127);
}

void test_run_script_allows_group_writable(void)
{
    char *path = write_temp("export OSH0_GW=1\n");
    if (!path) {
        TEST_SKIP("mkstemp falhou");
        return;
    }
    TEST_CHECK(chmod(path, 0664) == 0);
    petrush_unsetenv("OSH0_GW");
    TEST_CHECK(petrush_run_script(path) == 0);
    TEST_CHECK(petrush_getenv("OSH0_GW") != NULL);
    unlink(path);
    free(path);
    petrush_unsetenv("OSH0_GW");
}

void test_run_script_rejects_directory(void)
{
    char dir[] = "/var/tmp/petrush_osh0_dir_XXXXXX";
    if (!mkdtemp(dir)) {
        TEST_SKIP("mkdtemp falhou");
        return;
    }
    int rc = petrush_run_script(dir);
    TEST_CHECK(rc != 0);
    TEST_CHECK(rc != 127);
    rmdir(dir);
}

void test_run_script_last_status(void)
{
    char *path = write_temp("/bin/false\n");
    if (!path) {
        TEST_SKIP("mkstemp falhou");
        return;
    }
    TEST_CHECK(petrush_run_script(path) == 1);
    unlink(path);
    free(path);
}

void test_run_script_skips_hash_and_blank(void)
{
    char *path = write_temp("# comment\n\nexport OSH0_SKIP=ok\n");
    if (!path) {
        TEST_SKIP("mkstemp falhou");
        return;
    }
    petrush_unsetenv("OSH0_SKIP");
    TEST_CHECK(petrush_run_script(path) == 0);
    TEST_CHECK(petrush_getenv("OSH0_SKIP") != NULL);
    unlink(path);
    free(path);
    petrush_unsetenv("OSH0_SKIP");
}

TEST_LIST = {
    { "source_argc_must_be_two", test_source_argc_must_be_two },
    { "source_and_dot_same_builtin", test_source_and_dot_same_builtin },
    { "source_missing_ok_silent", test_source_missing_ok_silent },
    { "source_missing_required_fails", test_source_missing_required_fails },
    { "source_runs_in_current_process", test_source_runs_in_current_process },
    { "source_rejects_group_writable", test_source_rejects_group_writable },
    { "source_no_path_search", test_source_no_path_search },
    { "source_depth_ceiling", test_source_depth_ceiling },
    { "source_registered_in_table", test_source_registered_in_table },
    { "run_script_missing_is_127", test_run_script_missing_is_127 },
    { "run_script_allows_group_writable", test_run_script_allows_group_writable },
    { "run_script_rejects_directory", test_run_script_rejects_directory },
    { "run_script_last_status", test_run_script_last_status },
    { "run_script_skips_hash_and_blank", test_run_script_skips_hash_and_blank },
    { NULL, NULL }
};
