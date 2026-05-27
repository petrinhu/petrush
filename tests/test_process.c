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

TEST_LIST = {
    { "execute_null_cmd",           test_execute_null_cmd },
    { "execute_empty_cmd",          test_execute_empty_cmd },
    { "execute_not_found_127",      test_execute_not_found_returns_127 },
    { "execute_permission_126",     test_execute_permission_denied_for_nonexec_path_returns_126 },
    { "init_shell_termios_safe",    test_init_shell_termios_is_safe },
    { NULL, NULL }
};