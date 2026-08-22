/*
 * test_process.c — Testes para execução de processos externos (foundation)
 *
 * Foco: cenários de erro (não encontrado 127, permissão negada 126),
 *       inicialização de termios, e contratos básicos da API.
 * Cenários de sucesso real (fork/exec) e sinais são exercitados via smoke
 * manual + builds Sanitize (difícil de unit-test sem poluir o processo/terminal).
 *
 * TDD: testes escritos para validar as melhorias de PR-06.1 / PR-06.12 / PR-06.10.
 */

#include "acutest.h"
#include "petrush/process.h"
#include "petrush/parser.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

/* ===================== TESTES DE CONTRATO E ERROS ===================== */

void test_execute_null_cmd(void)
{
    int status = 0;
    int rc = execute_external(NULL, &status);

    TEST_CHECK(rc == -1);
    /* status não deve ser tocado em caso de argumento inválido */
}

void test_execute_empty_cmd(void)
{
    petrush_cmd_t cmd = {0}; /* argc == 0 */

    int status = 42; /* sentinel */
    int rc = execute_external(&cmd, &status);

    TEST_CHECK(rc == -1);
    /* status não deve ser forçado em caso de argv inválido */
}

void test_execute_not_found_returns_127(void)
{
    petrush_cmd_t cmd = {0};
    /* argv com terminador NULL é garantido pelo parser, mas montamos manualmente */
    static char *fake_argv[] = { "este_comando_definitivamente_nao_existe_123456789", NULL };
    cmd.argc = 1;
    cmd.argv = fake_argv;

    int status = 0;
    int rc = execute_external(&cmd, &status);

    TEST_CHECK(rc == -1);
    TEST_CHECK(status == 127);  /* código clássico de "command not found" */
}

void test_execute_permission_denied_for_nonexec_path_returns_126(void)
{
    /* Cria um arquivo temporário sem permissão de execução */
    char template[] = "/tmp/petrush_test_nonexec_XXXXXX";
    int fd = mkstemp(template);
    if (fd == -1) {
        TEST_SKIP("não foi possível criar arquivo temporário para o teste de 126");
        return;
    }
    close(fd);

    /* Remove permissão de execução (mantém legível) */
    chmod(template, 0644);

    petrush_cmd_t cmd = {0};
    static char *argv_nonexec[2];
    argv_nonexec[0] = template;
    argv_nonexec[1] = NULL;
    cmd.argc = 1;
    cmd.argv = argv_nonexec;

    int status = 0;
    int rc = execute_external(&cmd, &status);

    /* Limpeza */
    unlink(template);

    TEST_CHECK(rc == -1);
    TEST_CHECK(status == 126);  /* permissão negada / não executável */
}

void test_init_shell_termios_is_safe(void)
{
    /* Apenas garante que a chamada não crasha e pode ser chamada múltiplas vezes */
    petrush_init_shell_termios();
    petrush_init_shell_termios();

    TEST_CHECK(1);  /* se chegamos aqui, passou */
}

/* NEW-20: pipeline / redir — contrato com externos estaveis em /bin */
void test_pipeline_true_pipe_true(void)
{
    petrush_pipeline_t pl = {0};
    TEST_CHECK(petrush_parse_pipeline("/bin/true | /bin/true", &pl) == 0);
    TEST_CHECK(pl.ncmds == 2);

    int status = -1;
    int rc = execute_pipeline(&pl, &status);
    TEST_CHECK(rc == 0);
    TEST_CHECK(status == 0);
    petrush_pipeline_free(&pl);
}

void test_pipeline_false_last_status(void)
{
    petrush_pipeline_t pl = {0};
    TEST_CHECK(petrush_parse_pipeline("/bin/true | /bin/false", &pl) == 0);

    int status = 0;
    int rc = execute_pipeline(&pl, &status);
    TEST_CHECK(rc == 0);
    TEST_CHECK(status == 1); /* último estágio define status */
    petrush_pipeline_free(&pl);
}

void test_execute_external_redir_out_writes_file(void)
{
    char out_path[] = "/tmp/petrush_test_redir_XXXXXX";
    int fd = mkstemp(out_path);
    if (fd < 0) {
        TEST_SKIP("mkstemp falhou");
        return;
    }
    close(fd);
    unlink(out_path); /* execute recria com O_CREAT */

    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo redir-unit-ok > ", &cmd) != 0); /* incompleto = erro */

    petrush_pipeline_t pl = {0};
    char line[256];
    snprintf(line, sizeof(line), "/bin/echo redir-unit-ok > %s", out_path);
    TEST_CHECK(petrush_parse_pipeline(line, &pl) == 0);
    TEST_CHECK(pl.ncmds == 1);
    TEST_CHECK(pl.cmds[0].redir_out != NULL);

    int status = -1;
    int rc = execute_pipeline(&pl, &status);
    TEST_CHECK(rc == 0);
    TEST_CHECK(status == 0);

    FILE *f = fopen(out_path, "r");
    TEST_CHECK(f != NULL);
    if (f) {
        char buf[64] = {0};
        TEST_CHECK(fgets(buf, sizeof(buf), f) != NULL);
        TEST_CHECK(strstr(buf, "redir-unit-ok") != NULL);
        fclose(f);
    }
    unlink(out_path);
    petrush_pipeline_free(&pl);
}

/* UX-16: stderr via /bin/sh -c 'echo OUT; echo ERR >&2' */
static int parse_run_sh_both(const char *redir_suffix, const char *path,
                             int *status_out)
{
    petrush_pipeline_t pl = {0};
    char line[512];
    snprintf(line, sizeof(line),
             "/bin/sh -c 'echo OUT; echo ERR >&2' %s %s",
             redir_suffix, path);
    if (petrush_parse_pipeline(line, &pl) != 0) {
        petrush_pipeline_free(&pl);
        return -1;
    }
    int status = -1;
    int rc = execute_pipeline(&pl, &status);
    petrush_pipeline_free(&pl);
    if (status_out) *status_out = status;
    return rc;
}

void test_execute_redir_err_file(void)
{
    char err_path[] = "/tmp/petrush_test_err_XXXXXX";
    int fd = mkstemp(err_path);
    if (fd < 0) {
        TEST_SKIP("mkstemp falhou");
        return;
    }
    close(fd);
    unlink(err_path);

    int status = -1;
    TEST_CHECK(parse_run_sh_both("2>", err_path, &status) == 0);
    TEST_CHECK(status == 0);

    FILE *f = fopen(err_path, "r");
    TEST_CHECK(f != NULL);
    if (f) {
        char buf[128] = {0};
        TEST_CHECK(fgets(buf, sizeof(buf), f) != NULL);
        TEST_CHECK(strstr(buf, "ERR") != NULL);
        TEST_CHECK(strstr(buf, "OUT") == NULL);
        fclose(f);
    }
    unlink(err_path);
}

void test_execute_redir_err_append(void)
{
    char err_path[] = "/tmp/petrush_test_errapp_XXXXXX";
    int fd = mkstemp(err_path);
    if (fd < 0) {
        TEST_SKIP("mkstemp falhou");
        return;
    }
    close(fd);
    unlink(err_path);

    int status = -1;
    TEST_CHECK(parse_run_sh_both("2>>", err_path, &status) == 0);
    TEST_CHECK(status == 0);
    TEST_CHECK(parse_run_sh_both("2>>", err_path, &status) == 0);
    TEST_CHECK(status == 0);

    FILE *f = fopen(err_path, "r");
    TEST_CHECK(f != NULL);
    if (f) {
        char buf[128] = {0};
        int n = 0;
        while (fgets(buf, sizeof(buf), f)) {
            if (strstr(buf, "ERR")) n++;
        }
        TEST_CHECK(n == 2);
        fclose(f);
    }
    unlink(err_path);
}

void test_execute_redir_err_merge_to_out(void)
{
    char out_path[] = "/tmp/petrush_test_merge_XXXXXX";
    int fd = mkstemp(out_path);
    if (fd < 0) {
        TEST_SKIP("mkstemp falhou");
        return;
    }
    close(fd);
    unlink(out_path);

    petrush_pipeline_t pl = {0};
    char line[512];
    snprintf(line, sizeof(line),
             "/bin/sh -c 'echo OUT; echo ERR >&2' > %s 2>&1", out_path);
    TEST_CHECK(petrush_parse_pipeline(line, &pl) == 0);
    int status = -1;
    TEST_CHECK(execute_pipeline(&pl, &status) == 0);
    TEST_CHECK(status == 0);
    petrush_pipeline_free(&pl);

    FILE *f = fopen(out_path, "r");
    TEST_CHECK(f != NULL);
    if (f) {
        char all[256] = {0};
        size_t n = fread(all, 1, sizeof(all) - 1, f);
        all[n] = '\0';
        TEST_CHECK(strstr(all, "OUT") != NULL);
        TEST_CHECK(strstr(all, "ERR") != NULL);
        fclose(f);
    }
    unlink(out_path);
}

void test_execute_redir_ampgt_both(void)
{
    char out_path[] = "/tmp/petrush_test_ampgt_XXXXXX";
    int fd = mkstemp(out_path);
    if (fd < 0) {
        TEST_SKIP("mkstemp falhou");
        return;
    }
    close(fd);
    unlink(out_path);

    petrush_pipeline_t pl = {0};
    char line[512];
    snprintf(line, sizeof(line),
             "/bin/sh -c 'echo OUT; echo ERR >&2' &> %s", out_path);
    TEST_CHECK(petrush_parse_pipeline(line, &pl) == 0);
    int status = -1;
    TEST_CHECK(execute_pipeline(&pl, &status) == 0);
    TEST_CHECK(status == 0);
    petrush_pipeline_free(&pl);

    FILE *f = fopen(out_path, "r");
    TEST_CHECK(f != NULL);
    if (f) {
        char all[256] = {0};
        size_t n = fread(all, 1, sizeof(all) - 1, f);
        all[n] = '\0';
        TEST_CHECK(strstr(all, "OUT") != NULL);
        TEST_CHECK(strstr(all, "ERR") != NULL);
        fclose(f);
    }
    unlink(out_path);
}

void test_execute_redir_err_open_fail(void)
{
    petrush_pipeline_t pl = {0};
    /* diretório inexistente → open falha */
    TEST_CHECK(petrush_parse_pipeline(
        "/bin/sh -c 'echo ERR >&2' 2> /tmp/petrush-no-such-dir-ux16/err.txt",
        &pl) == 0);
    int status = 0;
    int rc = execute_pipeline(&pl, &status);
    TEST_CHECK(rc != 0 || status != 0);
    petrush_pipeline_free(&pl);
}

/* SEC-09: `>` recusa overwrite se o destino ja existe (O_EXCL). */
void test_execute_redir_out_noclobber_existing(void)
{
    char out_path[] = "/tmp/petrush_test_noclobber_XXXXXX";
    int fd = mkstemp(out_path);
    if (fd < 0) {
        TEST_SKIP("mkstemp falhou");
        return;
    }
    TEST_CHECK(write(fd, "KEEP\n", 5) == 5);
    close(fd);

    petrush_pipeline_t pl = {0};
    char line[256];
    snprintf(line, sizeof(line), "/bin/echo OVERWRITE > %s", out_path);
    TEST_CHECK(petrush_parse_pipeline(line, &pl) == 0);

    int status = 0;
    int rc = execute_pipeline(&pl, &status);
    TEST_CHECK(rc != 0 || status != 0);
    petrush_pipeline_free(&pl);

    FILE *f = fopen(out_path, "r");
    TEST_CHECK(f != NULL);
    if (f) {
        char buf[64] = {0};
        TEST_CHECK(fgets(buf, sizeof(buf), f) != NULL);
        TEST_CHECK(strcmp(buf, "KEEP\n") == 0);
        TEST_CHECK(strstr(buf, "OVERWRITE") == NULL);
        fclose(f);
    }
    unlink(out_path);
}

/* SEC-09: `>>` continua append em arquivo existente. */
void test_execute_redir_append_allows_existing(void)
{
    char out_path[] = "/tmp/petrush_test_noclobber_app_XXXXXX";
    int fd = mkstemp(out_path);
    if (fd < 0) {
        TEST_SKIP("mkstemp falhou");
        return;
    }
    TEST_CHECK(write(fd, "KEEP\n", 5) == 5);
    close(fd);

    petrush_pipeline_t pl = {0};
    char line[256];
    snprintf(line, sizeof(line), "/bin/echo APPENDED >> %s", out_path);
    TEST_CHECK(petrush_parse_pipeline(line, &pl) == 0);

    int status = -1;
    TEST_CHECK(execute_pipeline(&pl, &status) == 0);
    TEST_CHECK(status == 0);
    petrush_pipeline_free(&pl);

    FILE *f = fopen(out_path, "r");
    TEST_CHECK(f != NULL);
    if (f) {
        char all[128] = {0};
        size_t n = fread(all, 1, sizeof(all) - 1, f);
        all[n] = '\0';
        TEST_CHECK(strstr(all, "KEEP") != NULL);
        TEST_CHECK(strstr(all, "APPENDED") != NULL);
        fclose(f);
    }
    unlink(out_path);
}

/* SEC-09: `2>` tambem recusa overwrite (mesmo trunc). */
void test_execute_redir_err_noclobber_existing(void)
{
    char err_path[] = "/tmp/petrush_test_noclobber_err_XXXXXX";
    int fd = mkstemp(err_path);
    if (fd < 0) {
        TEST_SKIP("mkstemp falhou");
        return;
    }
    TEST_CHECK(write(fd, "KEEP\n", 5) == 5);
    close(fd);

    int status = 0;
    TEST_CHECK(parse_run_sh_both("2>", err_path, &status) != 0 || status != 0);

    FILE *f = fopen(err_path, "r");
    TEST_CHECK(f != NULL);
    if (f) {
        char buf[64] = {0};
        TEST_CHECK(fgets(buf, sizeof(buf), f) != NULL);
        TEST_CHECK(strcmp(buf, "KEEP\n") == 0);
        fclose(f);
    }
    unlink(err_path);
}

TEST_LIST = {
    { "execute_null_cmd",           test_execute_null_cmd },
    { "execute_empty_cmd",          test_execute_empty_cmd },
    { "execute_not_found_127",      test_execute_not_found_returns_127 },
    { "execute_permission_126",     test_execute_permission_denied_for_nonexec_path_returns_126 },
    { "init_shell_termios_safe",    test_init_shell_termios_is_safe },
    { "pipeline_true_pipe_true",    test_pipeline_true_pipe_true },
    { "pipeline_false_last_status", test_pipeline_false_last_status },
    { "execute_redir_out_file",     test_execute_external_redir_out_writes_file },
    { "execute_redir_err_file",     test_execute_redir_err_file },
    { "execute_redir_err_append",   test_execute_redir_err_append },
    { "execute_redir_err_merge",    test_execute_redir_err_merge_to_out },
    { "execute_redir_ampgt_both",   test_execute_redir_ampgt_both },
    { "execute_redir_err_open_fail", test_execute_redir_err_open_fail },
    { "execute_redir_out_noclobber", test_execute_redir_out_noclobber_existing },
    { "execute_redir_append_ok",     test_execute_redir_append_allows_existing },
    { "execute_redir_err_noclobber", test_execute_redir_err_noclobber_existing },
    { NULL, NULL }
};
