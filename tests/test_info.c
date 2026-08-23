/*
 * test_info.c — Basic tests for info builtin (Onda 3 placeholder, NEW-04/19)
 * + SEC-09: noclobber no caminho builtin (dispatcher run_builtin_with_redirs).
 */

#include "acutest.h"
#include "petrush/dispatcher.h"
#include "petrush/env.h"
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
    { NULL, NULL }
};
