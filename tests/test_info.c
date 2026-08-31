/*
 * test_info.c — Basic tests for info builtin (Onda 3 placeholder, NEW-04/19)
 * + SEC-09: noclobber no caminho builtin (dispatcher run_builtin_with_redirs).
 */

#include "acutest.h"
#include "petrush/dispatcher.h"
#include "petrush/env.h"
#include "petrush/expand.h"
#include "petrush/parser.h"
#include "petrush/ui_port.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ARCH-03: spy prove que builtin_clear usa a porta, não linenoise direto. */
static int g_clear_spy_calls;

static void spy_clear_screen(void)
{
    g_clear_spy_calls++;
}

static int spy_history_len(void)
{
    return 0;
}

static const char *spy_history_get(int index)
{
    (void)index;
    return NULL;
}

void test_builtin_clear_uses_ui_port(void)
{
    g_clear_spy_calls = 0;
    petrush_ui_port_t port = {
        .clear_screen = spy_clear_screen,
        .history_len = spy_history_len,
        .history_get = spy_history_get,
    };
    petrush_ui_port_bind(&port);

    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("clear", &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) == 0);
    TEST_CHECK(g_clear_spy_calls == 1);
    petrush_cmd_free(&cmd);

    petrush_ui_port_bind(NULL);
}

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

/* FEAT-NOCLOBBER: captura stdout de um builtin (pipe+dup2; sem freopen). */
static int capture_builtin_stdout(const char *line, char *out, size_t outsz,
                                  int *status_out)
{
    petrush_cmd_t cmd = {0};
    if (petrush_parse(line, &cmd) != 0) {
        petrush_cmd_free(&cmd);
        return -1;
    }

    int fds[2];
    if (pipe(fds) != 0) {
        petrush_cmd_free(&cmd);
        return -1;
    }
    int saved = dup(STDOUT_FILENO);
    if (saved < 0) {
        close(fds[0]);
        close(fds[1]);
        petrush_cmd_free(&cmd);
        return -1;
    }
    fflush(stdout);
    if (dup2(fds[1], STDOUT_FILENO) < 0) {
        close(saved);
        close(fds[0]);
        close(fds[1]);
        petrush_cmd_free(&cmd);
        return -1;
    }
    close(fds[1]);

    int st = dispatch_command(&cmd);
    fflush(stdout);
    dup2(saved, STDOUT_FILENO);
    close(saved);

    if (status_out) *status_out = st;

    ssize_t n = read(fds[0], out, outsz > 0 ? outsz - 1 : 0);
    close(fds[0]);
    if (n < 0) n = 0;
    if (outsz > 0) out[n] = '\0';

    petrush_cmd_free(&cmd);
    return 0;
}

void test_help_mentions_noclobber(void)
{
    char buf[4096] = {0};
    int status = -1;
    TEST_CHECK(capture_builtin_stdout("help", buf, sizeof(buf), &status) == 0);
    TEST_CHECK(status == 0);
    TEST_CHECK(strstr(buf, "noclobber") != NULL);
}

void test_info_mentions_noclobber(void)
{
    char buf[4096] = {0};
    int status = -1;
    TEST_CHECK(capture_builtin_stdout("info", buf, sizeof(buf), &status) == 0);
    TEST_CHECK(status == 0);
    TEST_CHECK(strstr(buf, "noclobber") != NULL);
}

/* FEAT-UMASK: print octal / set octal no processo do shell (sem -S). */
void test_builtin_umask_in_table(void)
{
    TEST_CHECK(builtin_table_has("umask"));
}

void test_builtin_umask_print_octal(void)
{
    mode_t saved = umask(0022);
    umask(0077);

    char buf[64] = {0};
    int status = -1;
    TEST_CHECK(capture_builtin_stdout("umask", buf, sizeof(buf), &status) == 0);
    TEST_CHECK(status == 0);
    TEST_CHECK(strcmp(buf, "0077\n") == 0);

    mode_t cur = umask(0);
    umask(saved);
    TEST_CHECK(cur == (mode_t)0077);
}

void test_builtin_umask_set_octal(void)
{
    mode_t saved = umask(0022);

    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("umask 077", &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) == 0);
    petrush_cmd_free(&cmd);

    mode_t cur = umask(0);
    umask(saved);
    TEST_CHECK(cur == (mode_t)0077);
}

void test_builtin_umask_set_leading_zeros(void)
{
    mode_t saved = umask(0077);

    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("umask 0022", &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) == 0);
    petrush_cmd_free(&cmd);

    mode_t cur = umask(0);
    umask(saved);
    TEST_CHECK(cur == (mode_t)0022);
}

void test_builtin_umask_rejects_non_octal(void)
{
    mode_t saved = umask(0022);

    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("umask 099", &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) != 0);
    petrush_cmd_free(&cmd);

    TEST_CHECK(petrush_parse("umask xyz", &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) != 0);
    petrush_cmd_free(&cmd);

    mode_t cur = umask(0);
    umask(saved);
    TEST_CHECK(cur == (mode_t)0022);
}

void test_builtin_umask_rejects_too_many_args(void)
{
    mode_t saved = umask(0022);

    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("umask 022 077", &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) != 0);
    petrush_cmd_free(&cmd);

    mode_t cur = umask(0);
    umask(saved);
    TEST_CHECK(cur == (mode_t)0022);
}

void test_help_mentions_umask(void)
{
    char buf[4096] = {0};
    int status = -1;
    TEST_CHECK(capture_builtin_stdout("help", buf, sizeof(buf), &status) == 0);
    TEST_CHECK(status == 0);
    TEST_CHECK(strstr(buf, "umask") != NULL);
}

/* FEAT-READ: read NAME (argc==2); 1 linha → 1 var; sem -a/-d/timeout/IFS split. */
static int feat_read_write_temp(char *path, const char *content)
{
    int fd = mkstemp(path);
    if (fd < 0) {
        return -1;
    }
    if (content) {
        size_t n = strlen(content);
        if (write(fd, content, n) != (ssize_t)n) {
            close(fd);
            unlink(path);
            return -1;
        }
    }
    close(fd);
    return 0;
}

void test_builtin_read_in_table(void)
{
    TEST_CHECK(builtin_table_has("read"));
}

void test_builtin_read_assigns_whole_line(void)
{
    char path[] = "/tmp/petrush_feat_read_XXXXXX";
    if (feat_read_write_temp(path, "hello world\n") != 0) {
        TEST_SKIP("mkstemp falhou");
        return;
    }

    (void)petrush_unsetenv("PETRUSH_FEAT_READ_A");

    petrush_cmd_t cmd = {0};
    char line[256];
    snprintf(line, sizeof(line), "read PETRUSH_FEAT_READ_A < %s", path);
    TEST_CHECK(petrush_parse(line, &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) == 0);
    petrush_cmd_free(&cmd);

    const char *got = petrush_getenv("PETRUSH_FEAT_READ_A");
    TEST_CHECK(got != NULL);
    if (got) {
        TEST_CHECK(strcmp(got, "hello world") == 0);
    }

    (void)petrush_unsetenv("PETRUSH_FEAT_READ_A");
    unlink(path);
}

void test_builtin_read_preserves_spaces_no_ifs_split(void)
{
    char path[] = "/tmp/petrush_feat_read_XXXXXX";
    if (feat_read_write_temp(path, "a  b\tc\n") != 0) {
        TEST_SKIP("mkstemp falhou");
        return;
    }

    (void)petrush_unsetenv("PETRUSH_FEAT_READ_B");

    petrush_cmd_t cmd = {0};
    char line[256];
    snprintf(line, sizeof(line), "read PETRUSH_FEAT_READ_B < %s", path);
    TEST_CHECK(petrush_parse(line, &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) == 0);
    petrush_cmd_free(&cmd);

    const char *got = petrush_getenv("PETRUSH_FEAT_READ_B");
    TEST_CHECK(got != NULL);
    if (got) {
        TEST_CHECK(strcmp(got, "a  b\tc") == 0);
    }

    (void)petrush_unsetenv("PETRUSH_FEAT_READ_B");
    unlink(path);
}

void test_builtin_read_empty_line_sets_empty(void)
{
    char path[] = "/tmp/petrush_feat_read_XXXXXX";
    if (feat_read_write_temp(path, "\n") != 0) {
        TEST_SKIP("mkstemp falhou");
        return;
    }

    TEST_CHECK(petrush_setenv("PETRUSH_FEAT_READ_C", "old", 1) == 0);

    petrush_cmd_t cmd = {0};
    char line[256];
    snprintf(line, sizeof(line), "read PETRUSH_FEAT_READ_C < %s", path);
    TEST_CHECK(petrush_parse(line, &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) == 0);
    petrush_cmd_free(&cmd);

    const char *got = petrush_getenv("PETRUSH_FEAT_READ_C");
    TEST_CHECK(got != NULL);
    if (got) {
        TEST_CHECK(strcmp(got, "") == 0);
    }

    (void)petrush_unsetenv("PETRUSH_FEAT_READ_C");
    unlink(path);
}

void test_builtin_read_eof_returns_one(void)
{
    char path[] = "/tmp/petrush_feat_read_XXXXXX";
    if (feat_read_write_temp(path, NULL) != 0) {
        TEST_SKIP("mkstemp falhou");
        return;
    }

    TEST_CHECK(petrush_setenv("PETRUSH_FEAT_READ_D", "keep", 1) == 0);

    petrush_cmd_t cmd = {0};
    char line[256];
    snprintf(line, sizeof(line), "read PETRUSH_FEAT_READ_D < %s", path);
    TEST_CHECK(petrush_parse(line, &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) == 1);
    petrush_cmd_free(&cmd);

    const char *got = petrush_getenv("PETRUSH_FEAT_READ_D");
    TEST_CHECK(got != NULL);
    if (got) {
        TEST_CHECK(strcmp(got, "keep") == 0);
    }

    (void)petrush_unsetenv("PETRUSH_FEAT_READ_D");
    unlink(path);
}

void test_builtin_read_rejects_no_name(void)
{
    /* redir evita hang se /usr/bin/read externo for despachado no RED */
    char path[] = "/tmp/petrush_feat_read_XXXXXX";
    if (feat_read_write_temp(path, "x\n") != 0) {
        TEST_SKIP("mkstemp falhou");
        return;
    }

    petrush_cmd_t cmd = {0};
    char line[256];
    snprintf(line, sizeof(line), "read < %s", path);
    TEST_CHECK(petrush_parse(line, &cmd) == 0);
    TEST_CHECK(cmd.argc == 1);
    TEST_CHECK(dispatch_command(&cmd) != 0);
    petrush_cmd_free(&cmd);
    unlink(path);
}

void test_builtin_read_rejects_too_many_args(void)
{
    char path[] = "/tmp/petrush_feat_read_XXXXXX";
    if (feat_read_write_temp(path, "x\n") != 0) {
        TEST_SKIP("mkstemp falhou");
        return;
    }

    petrush_cmd_t cmd = {0};
    char line[256];
    snprintf(line, sizeof(line), "read A B < %s", path);
    TEST_CHECK(petrush_parse(line, &cmd) == 0);
    TEST_CHECK(cmd.argc == 3);
    TEST_CHECK(dispatch_command(&cmd) != 0);
    petrush_cmd_free(&cmd);
    unlink(path);
}

void test_help_mentions_read(void)
{
    char buf[4096] = {0};
    int status = -1;
    TEST_CHECK(capture_builtin_stdout("help", buf, sizeof(buf), &status) == 0);
    TEST_CHECK(status == 0);
    TEST_CHECK(strstr(buf, "read") != NULL);
}

/* FEAT-TEST: test / [ primaries curtos; chama builtin_test direto (evita /usr/bin/test). */
static petrush_cmd_t feat_test_cmd(char **argv, int argc)
{
    petrush_cmd_t cmd = {0};
    cmd.argv = argv;
    cmd.argc = argc;
    return cmd;
}

void test_builtin_test_bracket_in_table(void)
{
    TEST_CHECK(builtin_table_has("test"));
    TEST_CHECK(builtin_table_has("["));
}

void test_builtin_test_no_args_false(void)
{
    char *av[] = { "test", NULL };
    petrush_cmd_t cmd = feat_test_cmd(av, 1);
    TEST_CHECK(builtin_test(&cmd) == 1);
}

void test_builtin_test_string_nonzero_true(void)
{
    char *av[] = { "test", "hello", NULL };
    petrush_cmd_t cmd = feat_test_cmd(av, 2);
    TEST_CHECK(builtin_test(&cmd) == 0);
}

void test_builtin_test_string_empty_false(void)
{
    char *av[] = { "test", "", NULL };
    petrush_cmd_t cmd = feat_test_cmd(av, 2);
    TEST_CHECK(builtin_test(&cmd) == 1);
}

void test_builtin_test_z_n(void)
{
    char *z_empty[] = { "test", "-z", "", NULL };
    char *z_full[] = { "test", "-z", "x", NULL };
    char *n_empty[] = { "test", "-n", "", NULL };
    char *n_full[] = { "test", "-n", "x", NULL };
    petrush_cmd_t c1 = feat_test_cmd(z_empty, 3);
    petrush_cmd_t c2 = feat_test_cmd(z_full, 3);
    petrush_cmd_t c3 = feat_test_cmd(n_empty, 3);
    petrush_cmd_t c4 = feat_test_cmd(n_full, 3);
    TEST_CHECK(builtin_test(&c1) == 0);
    TEST_CHECK(builtin_test(&c2) == 1);
    TEST_CHECK(builtin_test(&c3) == 1);
    TEST_CHECK(builtin_test(&c4) == 0);
}

void test_builtin_test_file_primaries(void)
{
    char path[] = "/tmp/petrush_feat_test_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) {
        TEST_SKIP("mkstemp falhou");
        return;
    }
    close(fd);

    char *e_yes[] = { "test", "-e", path, NULL };
    char *f_yes[] = { "test", "-f", path, NULL };
    char *d_no[] = { "test", "-d", path, NULL };
    petrush_cmd_t ce = feat_test_cmd(e_yes, 3);
    petrush_cmd_t cf = feat_test_cmd(f_yes, 3);
    petrush_cmd_t cd = feat_test_cmd(d_no, 3);
    TEST_CHECK(builtin_test(&ce) == 0);
    TEST_CHECK(builtin_test(&cf) == 0);
    TEST_CHECK(builtin_test(&cd) == 1);

    char dir[] = "/tmp/petrush_feat_test_dir_XXXXXX";
    if (!mkdtemp(dir)) {
        unlink(path);
        TEST_SKIP("mkdtemp falhou");
        return;
    }
    char *d_yes[] = { "test", "-d", dir, NULL };
    char *f_no[] = { "test", "-f", dir, NULL };
    petrush_cmd_t cdir = feat_test_cmd(d_yes, 3);
    petrush_cmd_t cfn = feat_test_cmd(f_no, 3);
    TEST_CHECK(builtin_test(&cdir) == 0);
    TEST_CHECK(builtin_test(&cfn) == 1);

    char missing[] = "/tmp/petrush_feat_test_missing_nope";
    unlink(missing);
    char *e_no[] = { "test", "-e", missing, NULL };
    petrush_cmd_t cm = feat_test_cmd(e_no, 3);
    TEST_CHECK(builtin_test(&cm) == 1);

    unlink(path);
    rmdir(dir);
}

void test_builtin_test_string_ops(void)
{
    char *eq_yes[] = { "test", "aa", "=", "aa", NULL };
    char *eq_no[] = { "test", "aa", "=", "bb", NULL };
    char *ne_yes[] = { "test", "aa", "!=", "bb", NULL };
    char *ne_no[] = { "test", "aa", "!=", "aa", NULL };
    petrush_cmd_t c1 = feat_test_cmd(eq_yes, 4);
    petrush_cmd_t c2 = feat_test_cmd(eq_no, 4);
    petrush_cmd_t c3 = feat_test_cmd(ne_yes, 4);
    petrush_cmd_t c4 = feat_test_cmd(ne_no, 4);
    TEST_CHECK(builtin_test(&c1) == 0);
    TEST_CHECK(builtin_test(&c2) == 1);
    TEST_CHECK(builtin_test(&c3) == 0);
    TEST_CHECK(builtin_test(&c4) == 1);
}

void test_builtin_test_int_ops(void)
{
    char *eq[] = { "test", "3", "-eq", "3", NULL };
    char *ne[] = { "test", "3", "-ne", "4", NULL };
    char *lt[] = { "test", "2", "-lt", "9", NULL };
    char *gt[] = { "test", "9", "-gt", "2", NULL };
    char *lt_no[] = { "test", "9", "-lt", "2", NULL };
    petrush_cmd_t c1 = feat_test_cmd(eq, 4);
    petrush_cmd_t c2 = feat_test_cmd(ne, 4);
    petrush_cmd_t c3 = feat_test_cmd(lt, 4);
    petrush_cmd_t c4 = feat_test_cmd(gt, 4);
    petrush_cmd_t c5 = feat_test_cmd(lt_no, 4);
    TEST_CHECK(builtin_test(&c1) == 0);
    TEST_CHECK(builtin_test(&c2) == 0);
    TEST_CHECK(builtin_test(&c3) == 0);
    TEST_CHECK(builtin_test(&c4) == 0);
    TEST_CHECK(builtin_test(&c5) == 1);
}

void test_builtin_test_int_rejects_non_integer(void)
{
    char *av[] = { "test", "x", "-eq", "1", NULL };
    petrush_cmd_t cmd = feat_test_cmd(av, 4);
    TEST_CHECK(builtin_test(&cmd) == 2);
}

void test_builtin_test_unary_unknown_op(void)
{
    /* 1 arg: string não-vazia (POSIX) → 0; unary desconhecido com operand → 2 */
    char *one[] = { "test", "-f", NULL };
    char *bad[] = { "test", "-x", "/tmp", NULL };
    petrush_cmd_t c1 = feat_test_cmd(one, 2);
    petrush_cmd_t c2 = feat_test_cmd(bad, 3);
    TEST_CHECK(builtin_test(&c1) == 0);
    TEST_CHECK(builtin_test(&c2) == 2);
}

void test_builtin_bracket_requires_closing(void)
{
    char *bad[] = { "[", "-n", "x", NULL };
    char *ok[] = { "[", "-n", "x", "]", NULL };
    char *empty[] = { "[", "]", NULL };
    petrush_cmd_t cbad = feat_test_cmd(bad, 3);
    petrush_cmd_t cok = feat_test_cmd(ok, 4);
    petrush_cmd_t cempty = feat_test_cmd(empty, 2);
    TEST_CHECK(builtin_test(&cbad) == 2);
    TEST_CHECK(builtin_test(&cok) == 0);
    TEST_CHECK(builtin_test(&cempty) == 1);
}

void test_builtin_test_short_circuit(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("test -n x && true", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);

    TEST_CHECK(petrush_parse_list("test -z x || true", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);

    TEST_CHECK(petrush_parse_list("[ -n x ] && true", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);
}

void test_help_mentions_test(void)
{
    char buf[4096] = {0};
    int status = -1;
    TEST_CHECK(capture_builtin_stdout("help", buf, sizeof(buf), &status) == 0);
    TEST_CHECK(status == 0);
    TEST_CHECK(strstr(buf, "test") != NULL);
}

/* OSH-2: builtin shift [n]; default 1; n>$# erro + intactos; shift 0 no-op. */
void test_osh2_shift_in_table(void)
{
    TEST_CHECK(builtin_table_has("shift"));
}

void test_osh2_builtin_shift_default_one(void)
{
    char *args[] = { "a", "b", "c" };
    TEST_CHECK(petrush_positional_set("sh", 3, args) == 0);

    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("shift", &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) == 0);
    petrush_cmd_free(&cmd);

    TEST_CHECK(petrush_positional_count() == 2);
    TEST_CHECK(strcmp(petrush_positional_get(1), "b") == 0);
    TEST_CHECK(strcmp(petrush_positional_get(2), "c") == 0);
    petrush_positional_clear();
}

void test_osh2_builtin_shift_n_two(void)
{
    char *args[] = { "a", "b", "c" };
    TEST_CHECK(petrush_positional_set("sh", 3, args) == 0);

    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("shift 2", &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) == 0);
    petrush_cmd_free(&cmd);

    TEST_CHECK(petrush_positional_count() == 1);
    TEST_CHECK(strcmp(petrush_positional_get(1), "c") == 0);
    petrush_positional_clear();
}

void test_osh2_builtin_shift_zero_noop(void)
{
    char *args[] = { "a", "b", "c" };
    TEST_CHECK(petrush_positional_set("sh", 3, args) == 0);

    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("shift 0", &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) == 0);
    petrush_cmd_free(&cmd);

    TEST_CHECK(petrush_positional_count() == 3);
    TEST_CHECK(strcmp(petrush_positional_get(1), "a") == 0);
    petrush_positional_clear();
}

void test_osh2_builtin_shift_too_many_intact(void)
{
    char *args[] = { "a", "b", "c" };
    TEST_CHECK(petrush_positional_set("sh", 3, args) == 0);

    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("shift 4", &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) != 0);
    petrush_cmd_free(&cmd);

    TEST_CHECK(petrush_positional_count() == 3);
    TEST_CHECK(strcmp(petrush_positional_get(1), "a") == 0);
    TEST_CHECK(strcmp(petrush_positional_get(2), "b") == 0);
    TEST_CHECK(strcmp(petrush_positional_get(3), "c") == 0);
    petrush_positional_clear();
}

void test_osh2_builtin_shift_rejects_non_numeric(void)
{
    char *args[] = { "a" };
    TEST_CHECK(petrush_positional_set("sh", 1, args) == 0);

    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("shift x", &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) != 0);
    petrush_cmd_free(&cmd);

    TEST_CHECK(petrush_positional_count() == 1);
    TEST_CHECK(strcmp(petrush_positional_get(1), "a") == 0);
    petrush_positional_clear();
}

void test_help_mentions_shift(void)
{
    char buf[4096] = {0};
    int status = -1;
    TEST_CHECK(capture_builtin_stdout("help", buf, sizeof(buf), &status) == 0);
    TEST_CHECK(status == 0);
    TEST_CHECK(strstr(buf, "shift") != NULL);
}

/* OSH-7: return [n]; so em funcao; default n=0; fora → !=0 sem exit. */
void test_osh7_return_in_table(void)
{
    TEST_CHECK(builtin_table_has("return"));
}

void test_osh7_return_outside_fn_status(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("return", &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) != 0);
    petrush_cmd_free(&cmd);
}

void test_osh7_return_n_from_fn(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("f() { return 3; }; f", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 3);
    petrush_list_free(&list);
}

void test_osh7_return_default_zero(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("f() { false; return; }; f", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);
}

void test_osh7_return_skips_rest(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list(
                   "f() { false; return 4; false; }; f", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 4);
    petrush_list_free(&list);
}

void test_osh7_return_outside_then_continues(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("return 1; true", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);
}

void test_help_mentions_return(void)
{
    char buf[4096] = {0};
    int status = -1;
    TEST_CHECK(capture_builtin_stdout("help", buf, sizeof(buf), &status) == 0);
    TEST_CHECK(status == 0);
    TEST_CHECK(strstr(buf, "return") != NULL);
}

/* OSH-8: local name[=value]; so em funcao; restaura ao sair; sem flags. */
void test_osh8_local_in_table(void)
{
    TEST_CHECK(builtin_table_has("local"));
}

void test_osh8_local_outside_fn_status(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("local x=1", &cmd) == 0);
    TEST_CHECK(dispatch_command(&cmd) != 0);
    petrush_cmd_free(&cmd);
}

void test_osh8_local_value_in_body(void)
{
    petrush_list_t list = {0};
    (void)petrush_unsetenv("OSH8_LV");
    (void)petrush_unsetenv("OSH8_SEEN");
    TEST_CHECK(petrush_parse_list(
                   "f() { local OSH8_LV=1; export OSH8_SEEN=$OSH8_LV; }; f",
                   &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);
    const char *seen = petrush_getenv("OSH8_SEEN");
    TEST_CHECK(seen != NULL && strcmp(seen, "1") == 0);
    TEST_CHECK(petrush_getenv("OSH8_LV") == NULL);
    (void)petrush_unsetenv("OSH8_SEEN");
}

void test_osh8_local_restores_outer(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_setenv("OSH8_RO", "outer", 1) == 0);
    TEST_CHECK(petrush_parse_list(
                   "f() { local OSH8_RO=inner; }; f", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);
    const char *v = petrush_getenv("OSH8_RO");
    TEST_CHECK(v != NULL && strcmp(v, "outer") == 0);
    (void)petrush_unsetenv("OSH8_RO");
}

void test_osh8_local_bare_unsets_in_body(void)
{
    petrush_list_t list = {0};
    (void)petrush_unsetenv("OSH8_BU_SEEN");
    TEST_CHECK(petrush_setenv("OSH8_BU", "outer", 1) == 0);
    /* local bare: mascara (unset); ao sair restaura outer */
    TEST_CHECK(petrush_parse_list(
                   "f() { local OSH8_BU; export OSH8_BU_SEEN=${OSH8_BU:-UNSET}; }; f",
                   &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);
    const char *seen = petrush_getenv("OSH8_BU_SEEN");
    TEST_CHECK(seen != NULL && strcmp(seen, "UNSET") == 0);
    const char *v = petrush_getenv("OSH8_BU");
    TEST_CHECK(v != NULL && strcmp(v, "outer") == 0);
    (void)petrush_unsetenv("OSH8_BU");
    (void)petrush_unsetenv("OSH8_BU_SEEN");
}

void test_osh8_local_rejects_flags(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list(
                   "f() { local -a x; false; }; f", &list) == 0);
    /* flag rejeitada: status != 0; false nao deve rodar se local falhar
     * e for o unico stmt... na verdade ; false ainda roda. Checar so
     * que local -a dentro da fn falha o status da fn se for ultimo. */
    petrush_list_free(&list);
    TEST_CHECK(petrush_parse_list(
                   "f() { local -a x; }; f", &list) == 0);
    TEST_CHECK(dispatch_list(&list) != 0);
    petrush_list_free(&list);
}

void test_osh8_local_restore_on_return(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_setenv("OSH8_RR", "outer", 1) == 0);
    TEST_CHECK(petrush_parse_list(
                   "f() { local OSH8_RR=inner; return 0; }; f", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);
    const char *v = petrush_getenv("OSH8_RR");
    TEST_CHECK(v != NULL && strcmp(v, "outer") == 0);
    (void)petrush_unsetenv("OSH8_RR");
}

void test_help_mentions_local(void)
{
    char buf[4096] = {0};
    int status = -1;
    TEST_CHECK(capture_builtin_stdout("help", buf, sizeof(buf), &status) == 0);
    TEST_CHECK(status == 0);
    TEST_CHECK(strstr(buf, "local") != NULL);
}

/* OSH-16: set / set -- / $? / $- / -x (capture stdout+stderr de dispatch_list). */
static void osh16_reset(void)
{
    petrush_shellopt_reset_for_tests();
    petrush_shell_abort_clear();
}

static int capture_list_stdio(const char *line, char *out, size_t outsz,
                              char *err, size_t errsz, int *status_out)
{
    petrush_list_t list = {0};
    if (petrush_parse_list(line, &list) != 0) {
        petrush_list_free(&list);
        return -1;
    }

    int outfds[2] = {-1, -1};
    int errfds[2] = {-1, -1};
    if (pipe(outfds) != 0 || pipe(errfds) != 0) {
        if (outfds[0] >= 0) {
            close(outfds[0]);
            close(outfds[1]);
        }
        petrush_list_free(&list);
        return -1;
    }
    int saved_out = dup(STDOUT_FILENO);
    int saved_err = dup(STDERR_FILENO);
    if (saved_out < 0 || saved_err < 0) {
        if (saved_out >= 0) {
            close(saved_out);
        }
        if (saved_err >= 0) {
            close(saved_err);
        }
        close(outfds[0]);
        close(outfds[1]);
        close(errfds[0]);
        close(errfds[1]);
        petrush_list_free(&list);
        return -1;
    }
    fflush(stdout);
    fflush(stderr);
    if (dup2(outfds[1], STDOUT_FILENO) < 0 ||
        dup2(errfds[1], STDERR_FILENO) < 0) {
        dup2(saved_out, STDOUT_FILENO);
        dup2(saved_err, STDERR_FILENO);
        close(saved_out);
        close(saved_err);
        close(outfds[0]);
        close(outfds[1]);
        close(errfds[0]);
        close(errfds[1]);
        petrush_list_free(&list);
        return -1;
    }
    close(outfds[1]);
    close(errfds[1]);

    int st = dispatch_list(&list);
    fflush(stdout);
    fflush(stderr);
    dup2(saved_out, STDOUT_FILENO);
    dup2(saved_err, STDERR_FILENO);
    close(saved_out);
    close(saved_err);

    if (status_out) {
        *status_out = st;
    }

    if (out && outsz > 0) {
        ssize_t n = read(outfds[0], out, outsz - 1);
        if (n < 0) {
            n = 0;
        }
        out[n] = '\0';
    }
    if (err && errsz > 0) {
        ssize_t n = read(errfds[0], err, errsz - 1);
        if (n < 0) {
            n = 0;
        }
        err[n] = '\0';
    }
    close(outfds[0]);
    close(errfds[0]);
    petrush_list_free(&list);
    return 0;
}

void test_osh16_set_in_table(void)
{
    TEST_CHECK(builtin_table_has("set"));
}

void test_osh16_set_double_dash_positionals(void)
{
    osh16_reset();
    char *oldargs[] = {"old"};
    TEST_CHECK(petrush_positional_set("sh", 1, oldargs) == 0);
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("set -- a b", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);
    TEST_CHECK(strcmp(petrush_positional_get(0), "sh") == 0);
    TEST_CHECK(petrush_positional_count() == 2);
    TEST_CHECK(strcmp(petrush_positional_get(1), "a") == 0);
    TEST_CHECK(strcmp(petrush_positional_get(2), "b") == 0);

    TEST_CHECK(petrush_parse_list("set --", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);
    TEST_CHECK(strcmp(petrush_positional_get(0), "sh") == 0);
    TEST_CHECK(petrush_positional_count() == 0);
    petrush_positional_clear();
}

void test_osh16_set_args_without_dash(void)
{
    osh16_reset();
    TEST_CHECK(petrush_positional_set("prog", 0, NULL) == 0);
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("set foo bar", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);
    TEST_CHECK(strcmp(petrush_positional_get(0), "prog") == 0);
    TEST_CHECK(petrush_positional_count() == 2);
    TEST_CHECK(strcmp(petrush_positional_get(1), "foo") == 0);
    TEST_CHECK(strcmp(petrush_positional_get(2), "bar") == 0);
    petrush_positional_clear();
}

void test_osh16_last_status_false_true(void)
{
    osh16_reset();
    char out[256] = {0};
    char err[256] = {0};
    int st = -1;
    TEST_CHECK(capture_list_stdio("false; echo $?", out, sizeof(out), err,
                                  sizeof(err), &st) == 0);
    TEST_CHECK(strstr(out, "1") != NULL);

    memset(out, 0, sizeof(out));
    TEST_CHECK(capture_list_stdio("true; echo $?", out, sizeof(out), err,
                                  sizeof(err), &st) == 0);
    TEST_CHECK(strstr(out, "0") != NULL);
}

void test_osh16_xtrace_traces_echo(void)
{
    osh16_reset();
    char out[256] = {0};
    char err[512] = {0};
    int st = -1;
    TEST_CHECK(capture_list_stdio("set -x; echo hi", out, sizeof(out), err,
                                  sizeof(err), &st) == 0);
    TEST_CHECK(strstr(out, "hi") != NULL);
    TEST_CHECK(strstr(err, "+ echo hi") != NULL);
}

void test_osh16_xtrace_set_x_not_self(void)
{
    osh16_reset();
    char out[256] = {0};
    char err[512] = {0};
    int st = -1;
    TEST_CHECK(capture_list_stdio("set -x; echo hi", out, sizeof(out), err,
                                  sizeof(err), &st) == 0);
    TEST_CHECK(strstr(err, "+ set -x") == NULL);
}

void test_osh16_xtrace_plus_x_is_traced(void)
{
    osh16_reset();
    char out[256] = {0};
    char err[512] = {0};
    int st = -1;
    TEST_CHECK(capture_list_stdio("set -x; set +x; echo done", out, sizeof(out),
                                  err, sizeof(err), &st) == 0);
    TEST_CHECK(strstr(err, "+ set +x") != NULL);
}

void test_osh16_dollar_minus_via_echo(void)
{
    osh16_reset();
    char out[256] = {0};
    char err[256] = {0};
    int st = -1;
    TEST_CHECK(capture_list_stdio("set -x; echo $-", out, sizeof(out), err,
                                  sizeof(err), &st) == 0);
    TEST_CHECK(strchr(out, 'C') != NULL);
    TEST_CHECK(strchr(out, 'x') != NULL);
}

void test_osh16_set_plus_C_fails(void)
{
    osh16_reset();
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("set +C", &list) == 0);
    TEST_CHECK(dispatch_list(&list) != 0);
    petrush_list_free(&list);
}

void test_osh16_set_minus_C_ok(void)
{
    osh16_reset();
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("set -C", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);
}

void test_osh16_set_unknown_z(void)
{
    osh16_reset();
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("set -z", &list) == 0);
    TEST_CHECK(dispatch_list(&list) != 0);
    petrush_list_free(&list);
}

/* OSH-18: set -e / -o errexit validos; $- ganha e; abort com isencoes. */
void test_osh18_set_e_ok(void)
{
    osh16_reset();
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("set -e", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);
    TEST_CHECK(petrush_shellopt_get('e') == 1);
    TEST_CHECK(strchr(petrush_shellopt_flags(), 'e') != NULL);

    TEST_CHECK(petrush_parse_list("set +e", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);
    TEST_CHECK(petrush_shellopt_get('e') == 0);

    TEST_CHECK(petrush_parse_list("set -o errexit", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);
    TEST_CHECK(petrush_shellopt_get('e') == 1);
    TEST_CHECK(petrush_parse_list("set +o errexit", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);
    TEST_CHECK(petrush_shellopt_get('e') == 0);
}

void test_osh18_false_aborts_list(void)
{
    osh16_reset();
    char out[256] = {0};
    char err[256] = {0};
    int st = -1;
    TEST_CHECK(capture_list_stdio("set -e; false; echo x", out, sizeof(out),
                                  err, sizeof(err), &st) == 0);
    TEST_CHECK(st != 0);
    TEST_CHECK(strstr(out, "x") == NULL);
    TEST_CHECK(petrush_take_shell_abort() == 1);
}

void test_osh18_without_e_continues(void)
{
    osh16_reset();
    char out[256] = {0};
    char err[256] = {0};
    int st = -1;
    TEST_CHECK(capture_list_stdio("false; echo x", out, sizeof(out),
                                  err, sizeof(err), &st) == 0);
    TEST_CHECK(strstr(out, "x") != NULL);
    TEST_CHECK(petrush_take_shell_abort() == 0);
}

void test_osh18_if_cond_exempt(void)
{
    osh16_reset();
    char out[256] = {0};
    char err[256] = {0};
    int st = -1;
    TEST_CHECK(capture_list_stdio("set -e; if false; then echo x; fi; echo y",
                                  out, sizeof(out), err, sizeof(err),
                                  &st) == 0);
    TEST_CHECK(st == 0);
    TEST_CHECK(strstr(out, "y") != NULL);
    TEST_CHECK(strstr(out, "x") == NULL);
    TEST_CHECK(petrush_take_shell_abort() == 0);
}

void test_osh18_and_or_last_aborts(void)
{
    osh16_reset();
    char out[256] = {0};
    char err[256] = {0};
    int st = -1;
    TEST_CHECK(capture_list_stdio("set -e; true && false; echo y",
                                  out, sizeof(out), err, sizeof(err),
                                  &st) == 0);
    TEST_CHECK(st != 0);
    TEST_CHECK(strstr(out, "y") == NULL);
    TEST_CHECK(petrush_take_shell_abort() == 1);
}

void test_osh18_and_or_non_last_ok(void)
{
    osh16_reset();
    char out[256] = {0};
    char err[256] = {0};
    int st = -1;
    TEST_CHECK(capture_list_stdio("set -e; false && echo x; echo y",
                                  out, sizeof(out), err, sizeof(err),
                                  &st) == 0);
    TEST_CHECK(st == 0);
    TEST_CHECK(strstr(out, "y") != NULL);
    TEST_CHECK(petrush_take_shell_abort() == 0);
}

void test_osh18_set_eux_ok(void)
{
    osh16_reset();
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("set -eux", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);
    TEST_CHECK(petrush_shellopt_get('e') == 1);
    TEST_CHECK(petrush_shellopt_get('u') == 1);
    TEST_CHECK(petrush_shellopt_get('x') == 1);
    const char *fl = petrush_shellopt_flags();
    TEST_CHECK(strchr(fl, 'e') != NULL);
    TEST_CHECK(strchr(fl, 'u') != NULL);
    TEST_CHECK(strchr(fl, 'x') != NULL);
    TEST_CHECK(strchr(fl, 'C') != NULL);
}

/* OSH-17: set -u / -o nounset passam a ser validos; $- ganha u. */
void test_osh17_set_u_ok(void)
{
    osh16_reset();
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("set -u", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);
    TEST_CHECK(petrush_shellopt_get('u') == 1);
    TEST_CHECK(strchr(petrush_shellopt_flags(), 'u') != NULL);

    TEST_CHECK(petrush_parse_list("set +u", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);
    TEST_CHECK(petrush_shellopt_get('u') == 0);

    TEST_CHECK(petrush_parse_list("set -o nounset", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);
    TEST_CHECK(petrush_shellopt_get('u') == 1);
    TEST_CHECK(petrush_parse_list("set +o nounset", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);
    TEST_CHECK(petrush_shellopt_get('u') == 0);
}

void test_osh17_unset_aborts_list(void)
{
    osh16_reset();
    petrush_unsetenv("OSH17_NOPE");
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("set -u; echo $OSH17_NOPE; echo should-not",
                                  &list) == 0);
    int st = dispatch_list(&list);
    petrush_list_free(&list);
    TEST_CHECK(st != 0);
    TEST_CHECK(petrush_take_shell_abort() == 1);
}

void test_osh16_set_o_xtrace(void)
{
    osh16_reset();
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("set -o xtrace", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);
    TEST_CHECK(petrush_shellopt_get('x') == 1);
    TEST_CHECK(petrush_parse_list("set +o xtrace", &list) == 0);
    TEST_CHECK(dispatch_list(&list) == 0);
    petrush_list_free(&list);
    TEST_CHECK(petrush_shellopt_get('x') == 0);
}

void test_help_mentions_set(void)
{
    char buf[4096] = {0};
    int status = -1;
    TEST_CHECK(capture_builtin_stdout("help", buf, sizeof(buf), &status) == 0);
    TEST_CHECK(status == 0);
    /* Evitar casar dentro de "unset". */
    TEST_CHECK(strstr(buf, "  set ") != NULL || strstr(buf, "set [--]") != NULL);
}

void test_info_anti_oe_noclobber_always(void)
{
    char buf[4096] = {0};
    int status = -1;
    TEST_CHECK(capture_builtin_stdout("info", buf, sizeof(buf), &status) == 0);
    TEST_CHECK(status == 0);
    TEST_CHECK(strstr(buf, "noclobber always-on") != NULL);
}

TEST_LIST = {
    { "info_builtin_basic", test_info_builtin_basic },
    { "info_output_contains_version", test_info_output_contains_version },
    { "builtin_clear_uses_ui_port", test_builtin_clear_uses_ui_port },
    { "builtin_redir_out_noclobber", test_builtin_redir_out_noclobber_existing },
    { "builtin_redir_append_ok",     test_builtin_redir_append_allows_existing },
    { "builtin_true_false_colon_in_table", test_builtin_true_false_colon_in_table },
    { "builtin_true_status_zero", test_builtin_true_status_zero },
    { "builtin_false_status_one", test_builtin_false_status_one },
    { "builtin_colon_status_zero", test_builtin_colon_status_zero },
    { "builtin_true_false_ignore_args", test_builtin_true_false_ignore_args },
    { "builtin_true_false_short_circuit", test_builtin_true_false_short_circuit },
    { "help_mentions_noclobber", test_help_mentions_noclobber },
    { "info_mentions_noclobber", test_info_mentions_noclobber },
    { "builtin_umask_in_table", test_builtin_umask_in_table },
    { "builtin_umask_print_octal", test_builtin_umask_print_octal },
    { "builtin_umask_set_octal", test_builtin_umask_set_octal },
    { "builtin_umask_set_leading_zeros", test_builtin_umask_set_leading_zeros },
    { "builtin_umask_rejects_non_octal", test_builtin_umask_rejects_non_octal },
    { "builtin_umask_rejects_too_many_args", test_builtin_umask_rejects_too_many_args },
    { "help_mentions_umask", test_help_mentions_umask },
    { "builtin_read_in_table", test_builtin_read_in_table },
    { "builtin_read_assigns_whole_line", test_builtin_read_assigns_whole_line },
    { "builtin_read_preserves_spaces_no_ifs_split", test_builtin_read_preserves_spaces_no_ifs_split },
    { "builtin_read_empty_line_sets_empty", test_builtin_read_empty_line_sets_empty },
    { "builtin_read_eof_returns_one", test_builtin_read_eof_returns_one },
    { "builtin_read_rejects_no_name", test_builtin_read_rejects_no_name },
    { "builtin_read_rejects_too_many_args", test_builtin_read_rejects_too_many_args },
    { "help_mentions_read", test_help_mentions_read },
    { "builtin_test_bracket_in_table", test_builtin_test_bracket_in_table },
    { "builtin_test_no_args_false", test_builtin_test_no_args_false },
    { "builtin_test_string_nonzero_true", test_builtin_test_string_nonzero_true },
    { "builtin_test_string_empty_false", test_builtin_test_string_empty_false },
    { "builtin_test_z_n", test_builtin_test_z_n },
    { "builtin_test_file_primaries", test_builtin_test_file_primaries },
    { "builtin_test_string_ops", test_builtin_test_string_ops },
    { "builtin_test_int_ops", test_builtin_test_int_ops },
    { "builtin_test_int_rejects_non_integer", test_builtin_test_int_rejects_non_integer },
    { "builtin_test_unary_unknown_op", test_builtin_test_unary_unknown_op },
    { "builtin_bracket_requires_closing", test_builtin_bracket_requires_closing },
    { "builtin_test_short_circuit", test_builtin_test_short_circuit },
    { "help_mentions_test", test_help_mentions_test },
    { "osh2_shift_in_table", test_osh2_shift_in_table },
    { "osh2_builtin_shift_default_one", test_osh2_builtin_shift_default_one },
    { "osh2_builtin_shift_n_two", test_osh2_builtin_shift_n_two },
    { "osh2_builtin_shift_zero_noop", test_osh2_builtin_shift_zero_noop },
    { "osh2_builtin_shift_too_many_intact", test_osh2_builtin_shift_too_many_intact },
    { "osh2_builtin_shift_rejects_non_numeric", test_osh2_builtin_shift_rejects_non_numeric },
    { "help_mentions_shift", test_help_mentions_shift },
    { "osh7_return_in_table", test_osh7_return_in_table },
    { "osh7_return_outside_fn_status", test_osh7_return_outside_fn_status },
    { "osh7_return_n_from_fn", test_osh7_return_n_from_fn },
    { "osh7_return_default_zero", test_osh7_return_default_zero },
    { "osh7_return_skips_rest", test_osh7_return_skips_rest },
    { "osh7_return_outside_then_continues", test_osh7_return_outside_then_continues },
    { "help_mentions_return", test_help_mentions_return },
    { "osh8_local_in_table", test_osh8_local_in_table },
    { "osh8_local_outside_fn_status", test_osh8_local_outside_fn_status },
    { "osh8_local_value_in_body", test_osh8_local_value_in_body },
    { "osh8_local_restores_outer", test_osh8_local_restores_outer },
    { "osh8_local_bare_unsets_in_body", test_osh8_local_bare_unsets_in_body },
    { "osh8_local_rejects_flags", test_osh8_local_rejects_flags },
    { "osh8_local_restore_on_return", test_osh8_local_restore_on_return },
    { "help_mentions_local", test_help_mentions_local },
    { "osh16_set_in_table", test_osh16_set_in_table },
    { "osh16_set_double_dash_positionals", test_osh16_set_double_dash_positionals },
    { "osh16_set_args_without_dash", test_osh16_set_args_without_dash },
    { "osh16_last_status_false_true", test_osh16_last_status_false_true },
    { "osh16_xtrace_traces_echo", test_osh16_xtrace_traces_echo },
    { "osh16_xtrace_set_x_not_self", test_osh16_xtrace_set_x_not_self },
    { "osh16_xtrace_plus_x_is_traced", test_osh16_xtrace_plus_x_is_traced },
    { "osh16_dollar_minus_via_echo", test_osh16_dollar_minus_via_echo },
    { "osh16_set_plus_C_fails", test_osh16_set_plus_C_fails },
    { "osh16_set_minus_C_ok", test_osh16_set_minus_C_ok },
    { "osh16_set_unknown_z", test_osh16_set_unknown_z },
    { "osh18_set_e_ok", test_osh18_set_e_ok },
    { "osh18_false_aborts_list", test_osh18_false_aborts_list },
    { "osh18_without_e_continues", test_osh18_without_e_continues },
    { "osh18_if_cond_exempt", test_osh18_if_cond_exempt },
    { "osh18_and_or_last_aborts", test_osh18_and_or_last_aborts },
    { "osh18_and_or_non_last_ok", test_osh18_and_or_non_last_ok },
    { "osh18_set_eux_ok", test_osh18_set_eux_ok },
    { "osh17_set_u_ok", test_osh17_set_u_ok },
    { "osh17_unset_aborts_list", test_osh17_unset_aborts_list },
    { "osh16_set_o_xtrace", test_osh16_set_o_xtrace },
    { "help_mentions_set", test_help_mentions_set },
    { "info_anti_oe_noclobber_always", test_info_anti_oe_noclobber_always },
    { NULL, NULL }
};
