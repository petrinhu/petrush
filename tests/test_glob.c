/*
 * test_glob.c - TDD UX-18 pathname expansion * ?
 */

#include "acutest.h"
#include "petrush/expand.h"
#include "petrush/parser.h"
#include "petrush/env.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

static char g_tmpdir[256];
static char g_oldcwd[512];

static void free_glob(char **list, int n)
{
    if (!list) return;
    for (int i = 0; i < n; i++) free(list[i]);
    free(list);
}

/* Walk dir and unlink/rmdir children; ENOENT is fine. */
static void empty_dir_best_effort(const char *dir)
{
    DIR *d = opendir(dir);
    if (!d)
        return;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        char path[768];
        int n = snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
        if (n < 0 || (size_t)n >= sizeof(path))
            continue;
        struct stat st;
        if (lstat(path, &st) != 0) {
            if (errno != ENOENT)
                TEST_CHECK_(0, "lstat(%s): %s", path, strerror(errno));
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            empty_dir_best_effort(path);
            if (rmdir(path) != 0 && errno != ENOENT)
                TEST_CHECK_(0, "rmdir(%s): %s", path, strerror(errno));
        } else if (unlink(path) != 0 && errno != ENOENT) {
            TEST_CHECK_(0, "unlink(%s): %s", path, strerror(errno));
        }
    }
    closedir(d);
}

/* Primary: system(rm -rf). On failure, walk+rmdir; ignore ENOENT. */
static void rm_rf_best_effort(const char *dir)
{
    char cmd[512];
    int n = snprintf(cmd, sizeof(cmd), "rm -rf '%s'", dir);
    if (n < 0 || (size_t)n >= sizeof(cmd)) {
        TEST_CHECK_(0, "rm_rf path too long: %s", dir);
        return;
    }
    int rc = system(cmd);
    if (rc == 0)
        return;
    empty_dir_best_effort(dir);
    if (rmdir(dir) != 0 && errno != ENOENT)
        TEST_CHECK_(0, "rmdir(%s) after system rc=%d: %s", dir, rc, strerror(errno));
}

static int setup_tmpdir(void)
{
    snprintf(g_tmpdir, sizeof(g_tmpdir), "/tmp/petrush-glob-XXXXXX");
    if (!mkdtemp(g_tmpdir)) return -1;
    if (!getcwd(g_oldcwd, sizeof(g_oldcwd))) return -1;
    if (chdir(g_tmpdir) != 0) return -1;

    FILE *f;
    f = fopen("a.c", "w"); if (!f) return -1; fclose(f);
    f = fopen("b.c", "w"); if (!f) return -1; fclose(f);
    f = fopen("d.txt", "w"); if (!f) return -1; fclose(f);
    f = fopen(".hidden", "w"); if (!f) return -1; fclose(f);
    if (mkdir("src", 0700) != 0) return -1;
    f = fopen("src/x.c", "w"); if (!f) return -1; fclose(f);
    f = fopen("src/y.c", "w"); if (!f) return -1; fclose(f);
    return 0;
}

static void teardown_tmpdir(void)
{
    if (chdir(g_oldcwd) != 0) {
        TEST_CHECK_(0, "chdir(%s) failed: %s", g_oldcwd, strerror(errno));
        return;
    }
    rm_rf_best_effort(g_tmpdir);
}

void test_glob_star_c(void)
{
    TEST_CHECK(setup_tmpdir() == 0);
    int n = 0;
    char **m = glob_word("*.c", &n);
    TEST_CHECK(m != NULL);
    TEST_CHECK(n == 2);
    if (n == 2 && m) {
        TEST_CHECK(strcmp(m[0], "a.c") == 0);
        TEST_CHECK(strcmp(m[1], "b.c") == 0);
    }
    free_glob(m, n);
    teardown_tmpdir();
}

void test_glob_question_c(void)
{
    TEST_CHECK(setup_tmpdir() == 0);
    int n = 0;
    char **m = glob_word("?.c", &n);
    TEST_CHECK(m != NULL);
    TEST_CHECK(n == 2);
    if (n == 2 && m) {
        TEST_CHECK(strcmp(m[0], "a.c") == 0);
        TEST_CHECK(strcmp(m[1], "b.c") == 0);
    }
    free_glob(m, n);
    teardown_tmpdir();
}

void test_glob_no_match_keeps_pattern(void)
{
    TEST_CHECK(setup_tmpdir() == 0);
    int n = 0;
    char **m = glob_word("no_such_*", &n);
    TEST_CHECK(m != NULL);
    TEST_CHECK(n == 1);
    if (n == 1 && m) {
        TEST_CHECK(strcmp(m[0], "no_such_*") == 0);
    }
    free_glob(m, n);
    teardown_tmpdir();
}

void test_glob_dotfile_only_with_dot_pattern(void)
{
    TEST_CHECK(setup_tmpdir() == 0);
    int n = 0;
    char **m = glob_word(".h*", &n);
    TEST_CHECK(m != NULL);
    TEST_CHECK(n == 1);
    if (n == 1 && m) {
        TEST_CHECK(strcmp(m[0], ".hidden") == 0);
    }
    free_glob(m, n);
    teardown_tmpdir();
}

void test_glob_dir_literal_basename_pattern(void)
{
    TEST_CHECK(setup_tmpdir() == 0);
    int n = 0;
    char **m = glob_word("src/*.c", &n);
    TEST_CHECK(m != NULL);
    TEST_CHECK(n == 2);
    if (n == 2 && m) {
        TEST_CHECK(strcmp(m[0], "src/x.c") == 0);
        TEST_CHECK(strcmp(m[1], "src/y.c") == 0);
    }
    free_glob(m, n);
    teardown_tmpdir();
}

void test_glob_meta_in_dir_literal(void)
{
    TEST_CHECK(setup_tmpdir() == 0);
    int n = 0;
    char **m = glob_word("*/x", &n);
    TEST_CHECK(m != NULL);
    TEST_CHECK(n == 1);
    if (n == 1 && m) {
        TEST_CHECK(strcmp(m[0], "*/x") == 0);
    }
    free_glob(m, n);
    teardown_tmpdir();
}

void test_glob_starstar_literal(void)
{
    TEST_CHECK(setup_tmpdir() == 0);
    int n = 0;
    char **m = glob_word("**", &n);
    TEST_CHECK(m != NULL);
    TEST_CHECK(n == 1);
    if (n == 1 && m) {
        TEST_CHECK(strcmp(m[0], "**") == 0);
    }
    free_glob(m, n);
    teardown_tmpdir();
}

void test_expand_unquoted_glob(void)
{
    TEST_CHECK(setup_tmpdir() == 0);
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo *.c", &cmd) == 0);
    expand_cmd_argv(&cmd);
    TEST_CHECK(cmd.argc == 3);
    if (cmd.argc == 3) {
        TEST_CHECK(strcmp(cmd.argv[0], "echo") == 0);
        TEST_CHECK(strcmp(cmd.argv[1], "a.c") == 0);
        TEST_CHECK(strcmp(cmd.argv[2], "b.c") == 0);
    }
    petrush_cmd_free(&cmd);
    teardown_tmpdir();
}

void test_expand_double_quoted_literal(void)
{
    TEST_CHECK(setup_tmpdir() == 0);
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo \"*.c\"", &cmd) == 0);
    expand_cmd_argv(&cmd);
    TEST_CHECK(cmd.argc == 2);
    if (cmd.argc == 2) {
        TEST_CHECK(strcmp(cmd.argv[1], "*.c") == 0);
    }
    petrush_cmd_free(&cmd);
    teardown_tmpdir();
}

void test_expand_single_quoted_literal(void)
{
    TEST_CHECK(setup_tmpdir() == 0);
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo '*.c'", &cmd) == 0);
    expand_cmd_argv(&cmd);
    TEST_CHECK(cmd.argc == 2);
    if (cmd.argc == 2) {
        TEST_CHECK(strcmp(cmd.argv[1], "*.c") == 0);
    }
    petrush_cmd_free(&cmd);
    teardown_tmpdir();
}

void test_expand_splice_pre_post(void)
{
    TEST_CHECK(setup_tmpdir() == 0);
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo pre *.c post", &cmd) == 0);
    expand_cmd_argv(&cmd);
    TEST_CHECK(cmd.argc == 5);
    if (cmd.argc == 5) {
        TEST_CHECK(strcmp(cmd.argv[0], "echo") == 0);
        TEST_CHECK(strcmp(cmd.argv[1], "pre") == 0);
        TEST_CHECK(strcmp(cmd.argv[2], "a.c") == 0);
        TEST_CHECK(strcmp(cmd.argv[3], "b.c") == 0);
        TEST_CHECK(strcmp(cmd.argv[4], "post") == 0);
    }
    petrush_cmd_free(&cmd);
    teardown_tmpdir();
}

void test_glob_max_fail_closed(void)
{
    char tmp[256];
    char old[512];
    snprintf(tmp, sizeof(tmp), "/tmp/petrush-globmax-XXXXXX");
    TEST_CHECK(mkdtemp(tmp) != NULL);
    TEST_CHECK(getcwd(old, sizeof(old)) != NULL);
    TEST_CHECK(chdir(tmp) == 0);

    for (int i = 0; i < 257; i++) {
        char name[32];
        snprintf(name, sizeof(name), "f%03d.dat", i);
        FILE *f = fopen(name, "w");
        TEST_CHECK(f != NULL);
        if (f) fclose(f);
    }

    int n = 0;
    char **m = glob_word("f*.dat", &n);
    TEST_CHECK(m == NULL); /* fail closed */

    if (chdir(old) != 0) {
        TEST_CHECK_(0, "chdir(%s) failed: %s", old, strerror(errno));
        return;
    }
    rm_rf_best_effort(tmp);
}

void test_expand_var_then_glob(void)
{
    TEST_CHECK(setup_tmpdir() == 0);
    petrush_setenv("PAT_GLOB", "*.c", 1);

    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo $PAT_GLOB", &cmd) == 0);
    expand_cmd_argv(&cmd);
    TEST_CHECK(cmd.argc == 3);
    if (cmd.argc == 3) {
        TEST_CHECK(strcmp(cmd.argv[1], "a.c") == 0);
        TEST_CHECK(strcmp(cmd.argv[2], "b.c") == 0);
    }
    petrush_cmd_free(&cmd);

    TEST_CHECK(petrush_parse("echo \"$PAT_GLOB\"", &cmd) == 0);
    expand_cmd_argv(&cmd);
    TEST_CHECK(cmd.argc == 2);
    if (cmd.argc == 2) {
        TEST_CHECK(strcmp(cmd.argv[1], "*.c") == 0);
    }
    petrush_cmd_free(&cmd);
    teardown_tmpdir();
}

void test_expand_redir_no_glob(void)
{
    TEST_CHECK(setup_tmpdir() == 0);
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo hi > *.c", &cmd) == 0);
    expand_cmd_argv(&cmd);
    TEST_CHECK(cmd.redir_out != NULL);
    if (cmd.redir_out) {
        TEST_CHECK(strcmp(cmd.redir_out, "*.c") == 0);
    }
    TEST_CHECK(cmd.argc == 2);
    petrush_cmd_free(&cmd);
    teardown_tmpdir();
}

TEST_LIST = {
    { "glob_star_c", test_glob_star_c },
    { "glob_question_c", test_glob_question_c },
    { "glob_no_match_keeps_pattern", test_glob_no_match_keeps_pattern },
    { "glob_dotfile_only_with_dot_pattern", test_glob_dotfile_only_with_dot_pattern },
    { "glob_dir_literal_basename_pattern", test_glob_dir_literal_basename_pattern },
    { "glob_meta_in_dir_literal", test_glob_meta_in_dir_literal },
    { "glob_starstar_literal", test_glob_starstar_literal },
    { "expand_unquoted_glob", test_expand_unquoted_glob },
    { "expand_double_quoted_literal", test_expand_double_quoted_literal },
    { "expand_single_quoted_literal", test_expand_single_quoted_literal },
    { "expand_splice_pre_post", test_expand_splice_pre_post },
    { "glob_max_fail_closed", test_glob_max_fail_closed },
    { "expand_var_then_glob", test_expand_var_then_glob },
    { "expand_redir_no_glob", test_expand_redir_no_glob },
    { NULL, NULL }
};
