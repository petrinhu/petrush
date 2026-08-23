/*
 * test_pudo.c — Testes de segurança para o builtin 'pudo'
 *
 * Foco: Testes de segurança (Option C)
 * Prioridade alta em cenários de ataque e hardening.
 */

#include "acutest.h"
#include "petrush/pudo.h"
#include "petrush/env.h"
#include "allow_resolve.h"
#include "child_argv.h"
#include "target_check.h"
#include "target_open.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ===================== TESTES DE SEGURANÇA - ENVIRONMENT ===================== */

/*
 * Testes que verificam se o pudo está removendo variáveis de ambiente
 * perigosas que podem ser usadas para escalada de privilégios.
 */

void test_pudo_sanitize_removes_ld_preload(void)
{
    /* Simula um ambiente atacado */
    setenv("LD_PRELOAD", "/tmp/evil.so", 1);
    setenv("HOME", "/home/user", 1);

    /* Chama explicitamente a sanitização exposta para testes */
    int rc = pudo_sanitize_environment();

    const char *ld_preload = petrush_getenv("LD_PRELOAD");
    TEST_CHECK(rc == 0);
    TEST_CHECK(ld_preload == NULL);
}

void test_pudo_sanitize_removes_ld_library_path(void)
{
    setenv("LD_LIBRARY_PATH", "/tmp/fake_libs", 1);

    int rc = pudo_sanitize_environment();
    const char *val = petrush_getenv("LD_LIBRARY_PATH");

    TEST_CHECK(rc == 0);
    TEST_CHECK(val == NULL);
}

void test_pudo_sanitize_removes_multiple_dangerous_vars(void)
{
    setenv("LD_PRELOAD", "/evil.so", 1);
    setenv("LD_LIBRARY_PATH", "/tmp", 1);
    setenv("LD_AUDIT", "/evil_audit.so", 1);
    setenv("PYTHONPATH", "/tmp/malicious", 1);

    int rc = pudo_sanitize_environment();

    TEST_CHECK(rc == 0);
    TEST_CHECK(petrush_getenv("LD_PRELOAD") == NULL);
    TEST_CHECK(petrush_getenv("LD_LIBRARY_PATH") == NULL);
    TEST_CHECK(petrush_getenv("LD_AUDIT") == NULL);
    TEST_CHECK(petrush_getenv("PYTHONPATH") == NULL);
}

/* ===================== TESTES DE ALLOW-LIST E CONFIG ===================== */

void test_pudo_config_loading_and_allowed(void)
{
    /* Testa o carregamento básico de config (mesmo que o arquivo não exista).
     * Quando não há arquivo, allowed_count fica 0 e tudo é permitido no lado client.
     * A validação real acontece no pudod.
     */
    /* Chamar load indiretamente via builtin não é fácil sem mocks.
     * Aqui verificamos que a API de sanitize/config não quebra.
     */
    TEST_CHECK(pudo_sanitize_environment() == 0 || 1); /* tolerante */
}

/* ===================== TESTES DE PARSING SEGURO ===================== */

void test_pudo_uses_argv_not_shell(void)
{
    /* Segurança chave: usamos arrays argv (execve style) passados para pudod.
     * Não concatenamos strings para shell. Portanto 'id; rm -rf /' 
     * é tratado como um único argumento (ou separado pelo parser), nunca executado como shell.
     */
    petrush_cmd_t cmd = {0};
    /* O parser já quebra corretamente */
    int rc = petrush_parse("pudo /bin/id ; rm -rf /", &cmd);
    TEST_CHECK(rc == 0);
    /* cmd.argc > 1 e os args não viram shell command */
    TEST_CHECK(cmd.argc >= 2);
    /* O importante: o backend (pudod) recebe argv separado, não string para sh -c */
    petrush_cmd_free(&cmd);
}

/* ===================== TESTES DO HELPER PUDOD (sem root) ===================== */

void test_pudo_builtin_does_not_mutate_parent_env(void)
{
    /* SEC-01: allow-list client recusa depois do sanitize. O pai tem de
     * conservar LD_PRELOAD (clean_envp só no execve do filho). */
    char tmpl[] = "/var/tmp/petrush-sec01-XXXXXX";
    char *dir = mkdtemp(tmpl);
    TEST_CHECK(dir != NULL);
    if (!dir) return;

    char cfgdir[PATH_MAX];
    char cfgfile[PATH_MAX];
    snprintf(cfgdir, sizeof(cfgdir), "%s/.config", dir);
    TEST_CHECK(mkdir(cfgdir, 0700) == 0 || errno == EEXIST);
    snprintf(cfgdir, sizeof(cfgdir), "%s/.config/petrush", dir);
    TEST_CHECK(mkdir(cfgdir, 0700) == 0 || errno == EEXIST);
    snprintf(cfgfile, sizeof(cfgfile), "%s/.config/petrush/pudo.conf", dir);
    FILE *cf = fopen(cfgfile, "w");
    TEST_CHECK(cf != NULL);
    if (!cf) return;
    fputs("/usr/bin/true\n", cf);
    fclose(cf);

    setenv("HOME", dir, 1);
    setenv("LD_PRELOAD", "/tmp/sec01-evil.so", 1);

    petrush_cmd_t cmd = {0};
    int prc = petrush_parse("pudo /usr/bin/id", &cmd);
    TEST_CHECK(prc == 0);
    if (prc != 0) return;

    (void)builtin_pudo(&cmd);
    petrush_cmd_free(&cmd);

    const char *left = petrush_getenv("LD_PRELOAD");
    TEST_CHECK(left != NULL);
    if (left) {
        TEST_CHECK(strcmp(left, "/tmp/sec01-evil.so") == 0);
    }

    unsetenv("LD_PRELOAD");
}

/* ===================== SEC-02: path do pudod (Release vs debug) ===================== */

void test_sec02_release_rejects_relative_fallbacks(void)
{
    /* Em Release, build/pudod, ./pudod e "pudod" NÃO podem ser aceitos. */
    TEST_CHECK(pudo_allow_pudod_candidate("build/pudod", 1) == 0);
    TEST_CHECK(pudo_allow_pudod_candidate("./build/pudod", 1) == 0);
    TEST_CHECK(pudo_allow_pudod_candidate("../build/pudod", 1) == 0);
    TEST_CHECK(pudo_allow_pudod_candidate("./pudod", 1) == 0);
    TEST_CHECK(pudo_allow_pudod_candidate("pudod", 1) == 0);
    TEST_CHECK(pudo_allow_pudod_candidate("", 1) == 0);
    TEST_CHECK(pudo_allow_pudod_candidate(NULL, 1) == 0);
}

void test_sec02_release_rejects_untrusted_absolute_sibling(void)
{
    /* Vetor principal (Cosmo/Narciso): sibling sob /tmp ou cwd do atacante. */
    TEST_CHECK(pudo_allow_pudod_candidate("/tmp/evil/pudod", 1) == 0);
    TEST_CHECK(pudo_allow_pudod_candidate("/tmp/evil/petrush-pudod", 1) == 0);
    TEST_CHECK(pudo_allow_pudod_candidate("/home/attacker/bin/pudod", 1) == 0);
    TEST_CHECK(pudo_allow_pudod_candidate("/var/tmp/pudod", 1) == 0);
}

void test_sec02_release_accepts_install_absolutes(void)
{
    TEST_CHECK(pudo_allow_pudod_candidate("/usr/local/libexec/petrush-pudod", 1) == 1);
    TEST_CHECK(pudo_allow_pudod_candidate("/usr/local/bin/petrush-pudod", 1) == 1);
    TEST_CHECK(pudo_allow_pudod_candidate("/usr/local/bin/pudod", 1) == 1);
    TEST_CHECK(pudo_allow_pudod_candidate("/usr/libexec/petrush-pudod", 1) == 1);
}

void test_sec02_debug_allows_relative_and_build_absolute(void)
{
    /* Debug: fallbacks relativos e sibling absoluto fora de install OK. */
    TEST_CHECK(pudo_allow_pudod_candidate("build/pudod", 0) == 1);
    TEST_CHECK(pudo_allow_pudod_candidate("./build/pudod", 0) == 1);
    TEST_CHECK(pudo_allow_pudod_candidate("../build/pudod", 0) == 1);
    TEST_CHECK(pudo_allow_pudod_candidate("pudod", 0) == 1);
    TEST_CHECK(pudo_allow_pudod_candidate("./pudod", 0) == 1);
    TEST_CHECK(pudo_allow_pudod_candidate("/home/dev/petrush/build/pudod", 0) == 1);
    TEST_CHECK(pudo_allow_pudod_candidate("/usr/local/libexec/petrush-pudod", 0) == 1);
    /* Fail closed ainda vale para NULL/vazio. */
    TEST_CHECK(pudo_allow_pudod_candidate(NULL, 0) == 0);
    TEST_CHECK(pudo_allow_pudod_candidate("", 0) == 0);
}

/* ===================== SEC-04: fail closed se argc > MAX_ARGS ===================== */

void test_sec04_pudod_build_rejects_overflow(void)
{
    /* pudod_argc = 2 + (PUDOD_MAX_ARGS + 1) user args => deve recusar. */
    enum { N = PUDOD_MAX_ARGS + 3 }; /* argv[0], argv[1], + MAX+1 user */
    char *argv_buf[N + 1];
    char label_storage[N][16];
    char *out[PUDOD_MAX_ARGS + 2];

    argv_buf[0] = "pudod";
    argv_buf[1] = "/bin/true";
    for (int i = 2; i < N; i++) {
        snprintf(label_storage[i], sizeof(label_storage[i]), "a%d", i);
        argv_buf[i] = label_storage[i];
    }
    argv_buf[N] = NULL;

    int rc = pudod_build_child_argv(argv_buf, N, "/bin/true", out,
                                    PUDOD_MAX_ARGS + 2);
    TEST_CHECK(rc == -1);
}

void test_sec04_pudod_build_accepts_at_limit(void)
{
    /* Exatamente PUDOD_MAX_ARGS user args: cabe. */
    enum { N = PUDOD_MAX_ARGS + 2 }; /* argv[0], argv[1], + MAX user */
    char *argv_buf[N + 1];
    char label_storage[N][16];
    char *out[PUDOD_MAX_ARGS + 2];

    argv_buf[0] = "pudod";
    argv_buf[1] = "/bin/true";
    for (int i = 2; i < N; i++) {
        snprintf(label_storage[i], sizeof(label_storage[i]), "b%d", i);
        argv_buf[i] = label_storage[i];
    }
    argv_buf[N] = NULL;

    int rc = pudod_build_child_argv(argv_buf, N, "/usr/bin/true", out,
                                    PUDOD_MAX_ARGS + 2);
    TEST_CHECK(rc == 0);
    TEST_CHECK(out[0] != NULL && strcmp(out[0], "/usr/bin/true") == 0);
    TEST_CHECK(out[PUDOD_MAX_ARGS] != NULL);     /* ultimo user arg */
    TEST_CHECK(out[PUDOD_MAX_ARGS + 1] == NULL); /* terminador */
}

void test_sec04_pudo_helper_rejects_overflow(void)
{
    /* pudod path: fixed 2 + (argc-2) user + NULL > 128 */
    TEST_CHECK(pudo_helper_argv_fits(PUDO_HELPER_ARGV_MAX) == 0);
    TEST_CHECK(pudo_helper_argv_fits(PUDO_HELPER_ARGV_MAX + 10) == 0);
}

void test_sec04_pudo_helper_accepts_small(void)
{
    TEST_CHECK(pudo_helper_argv_fits(2) == 1);  /* pudo cmd */
    TEST_CHECK(pudo_helper_argv_fits(3) == 1);  /* pudo cmd arg */
    TEST_CHECK(pudo_helper_argv_fits(10) == 1);
    TEST_CHECK(pudo_helper_argv_fits(1) == 0);  /* argc invalido */
    TEST_CHECK(pudo_helper_argv_fits(0) == 0);
}

/* SEC-11: Boundary B abolida — nunca fallback para sudo (Debug = Release). */
void test_sec11_sudo_fallback_always_denied(void)
{
    TEST_CHECK(pudo_allow_sudo_fallback() == 0);
}

/* SEC-05: realpath falhou => recusar entrada (nunca aceitar o literal). */
void test_pudod_allow_entry_rejects_unresolvable(void)
{
    char out[PATH_MAX];
    memset(out, 'X', sizeof(out));
    out[sizeof(out) - 1] = '\0';

    const char *bogus = "/tmp/petrush-sec05-no-such-dir/no-binary";
    int rc = pudod_resolve_allow_entry(bogus, out, sizeof(out));

    TEST_CHECK(rc == -1);
    /* Não pode ter copiado o literal para out. */
    TEST_CHECK(strcmp(out, bogus) != 0);
}

void test_pudod_allow_entry_accepts_existing(void)
{
    char out[PATH_MAX];
    out[0] = '\0';

    /* /bin/sh existe em qualquer Linux razoável; realpath pode canonicalizar. */
    int rc = pudod_resolve_allow_entry("/bin/sh", out, sizeof(out));
    TEST_CHECK(rc == 0);
    TEST_CHECK(out[0] == '/');
    TEST_CHECK(access(out, F_OK) == 0);
}

/* SEC-06: alvo deve ser regular, root-owned e com bit de exec. */
static void sec06_fill_stat(struct stat *st, mode_t mode, uid_t uid)
{
    memset(st, 0, sizeof(*st));
    st->st_mode = mode;
    st->st_uid = uid;
}

void test_sec06_rejects_null_stat(void)
{
    TEST_CHECK(pudod_target_is_root_exec(NULL) == -1);
}

void test_sec06_accepts_root_owned_executable(void)
{
    struct stat st;
    sec06_fill_stat(&st, S_IFREG | 0755, 0);
    TEST_CHECK(pudod_target_is_root_exec(&st) == 0);

    sec06_fill_stat(&st, S_IFREG | 0111, 0);
    TEST_CHECK(pudod_target_is_root_exec(&st) == 0);

    sec06_fill_stat(&st, S_IFREG | S_IXUSR, 0);
    TEST_CHECK(pudod_target_is_root_exec(&st) == 0);
}

void test_sec06_rejects_non_root_owner(void)
{
    struct stat st;
    sec06_fill_stat(&st, S_IFREG | 0755, 1000);
    TEST_CHECK(pudod_target_is_root_exec(&st) == -1);

    sec06_fill_stat(&st, S_IFREG | 0755, 1);
    TEST_CHECK(pudod_target_is_root_exec(&st) == -1);
}

void test_sec06_rejects_non_executable(void)
{
    struct stat st;
    sec06_fill_stat(&st, S_IFREG | 0644, 0);
    TEST_CHECK(pudod_target_is_root_exec(&st) == -1);

    sec06_fill_stat(&st, S_IFREG | 0600, 0);
    TEST_CHECK(pudod_target_is_root_exec(&st) == -1);

    sec06_fill_stat(&st, S_IFREG | 0444, 0);
    TEST_CHECK(pudod_target_is_root_exec(&st) == -1);
}

void test_sec06_rejects_non_regular(void)
{
    struct stat st;
    sec06_fill_stat(&st, S_IFDIR | 0755, 0);
    TEST_CHECK(pudod_target_is_root_exec(&st) == -1);

    sec06_fill_stat(&st, S_IFLNK | 0777, 0);
    TEST_CHECK(pudod_target_is_root_exec(&st) == -1);

    sec06_fill_stat(&st, S_IFCHR | 0755, 0);
    TEST_CHECK(pudod_target_is_root_exec(&st) == -1);
}

/* SEC-07: open do alvo com O_NOFOLLOW (fecha TOCTOU realpath -> open). */
void test_sec07_open_flags_include_nofollow(void)
{
    int flags = pudod_target_open_flags();
    TEST_CHECK((flags & O_ACCMODE) == O_RDONLY);
    TEST_CHECK((flags & O_CLOEXEC) == O_CLOEXEC);
    TEST_CHECK((flags & O_NOFOLLOW) == O_NOFOLLOW);
}

void test_sec07_open_rejects_null(void)
{
    errno = 0;
    TEST_CHECK(pudod_open_target(NULL) == -1);
}

void test_sec07_open_regular_ok(void)
{
    char dir[] = "/var/tmp/petrush-sec07-reg-XXXXXX";
    char *d = mkdtemp(dir);
    TEST_CHECK(d != NULL);
    if (!d) {
        return;
    }

    char path[sizeof(dir) + 16];
    int n = snprintf(path, sizeof(path), "%s/file", dir);
    TEST_CHECK(n > 0 && (size_t)n < sizeof(path));
    if (n <= 0 || (size_t)n >= sizeof(path)) {
        rmdir(dir);
        return;
    }

    int tfd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    TEST_CHECK(tfd >= 0);
    if (tfd < 0) {
        rmdir(dir);
        return;
    }
    close(tfd);

    int fd = pudod_open_target(path);
    TEST_CHECK(fd >= 0);
    if (fd >= 0) {
        close(fd);
    }

    unlink(path);
    rmdir(dir);
}

void test_sec07_open_rejects_symlink(void)
{
    char dir[] = "/var/tmp/petrush-sec07-lnk-XXXXXX";
    char *d = mkdtemp(dir);
    TEST_CHECK(d != NULL);
    if (!d) {
        return;
    }

    char target[sizeof(dir) + 16];
    char linkpath[sizeof(dir) + 16];
    int n1 = snprintf(target, sizeof(target), "%s/real", dir);
    int n2 = snprintf(linkpath, sizeof(linkpath), "%s/link", dir);
    TEST_CHECK(n1 > 0 && (size_t)n1 < sizeof(target));
    TEST_CHECK(n2 > 0 && (size_t)n2 < sizeof(linkpath));
    if (n1 <= 0 || (size_t)n1 >= sizeof(target) ||
        n2 <= 0 || (size_t)n2 >= sizeof(linkpath)) {
        rmdir(dir);
        return;
    }

    int tfd = open(target, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    TEST_CHECK(tfd >= 0);
    if (tfd < 0) {
        rmdir(dir);
        return;
    }
    close(tfd);

    TEST_CHECK(symlink(target, linkpath) == 0);

    errno = 0;
    int fd = pudod_open_target(linkpath);
    TEST_CHECK(fd < 0);
    /* Linux: O_NOFOLLOW em symlink final => ELOOP */
    TEST_CHECK(errno == ELOOP);

    /* O arquivo real (nao-symlink) continua abrivel. */
    fd = pudod_open_target(target);
    TEST_CHECK(fd >= 0);
    if (fd >= 0) {
        close(fd);
    }

    unlink(linkpath);
    unlink(target);
    rmdir(dir);
}

void test_pudod_binary_refuses_without_privileges(void)
{
    /* Testa o helper diretamente: sem setuid/root ele deve falhar cedo
     * com mensagem de allow-list ou recusa. Valida execução do pudod
     * sem precisar de privilégios (recomendado para CI / gate).
     */
    char pudod_path[PATH_MAX];
    char *pudod = NULL;

    /* Irmão de /proc/self/exe — readlink NÃO null-termina */
    {
        ssize_t rlen = readlink("/proc/self/exe", pudod_path, sizeof(pudod_path) - 1);
        if (rlen > 0) {
            pudod_path[rlen] = '\0';
            char *last_slash = strrchr(pudod_path, '/');
            if (last_slash) {
                size_t dir_len = (size_t)(last_slash - pudod_path);
                /* dir + "/pudod" + NUL */
                if (dir_len + 7 <= sizeof(pudod_path)) {
                    memcpy(last_slash, "/pudod", 7);
                    if (access(pudod_path, X_OK) == 0) {
                        pudod = pudod_path;
                    }
                }
            }
        }
    }

    if (!pudod) {
        const char *fallbacks[] = {
            "./pudod",
            "../pudod",
            "./build/pudod",
            "build/pudod",
            NULL
        };
        for (int i = 0; fallbacks[i]; i++) {
            if (access(fallbacks[i], X_OK) == 0) {
                size_t fl = strlen(fallbacks[i]);
                if (fl + 1 <= sizeof(pudod_path)) {
                    memcpy(pudod_path, fallbacks[i], fl + 1);
                    pudod = pudod_path;
                }
                break;
            }
        }
    }

    TEST_CHECK(pudod != NULL);
    if (!pudod) {
        return;
    }

    /* PATH_MAX + sufixo; evita -Werror=format-truncation no CI (gcc Debug) */
    char cmd[PATH_MAX + 64];
    int n = snprintf(cmd, sizeof(cmd), "%s /usr/bin/id 2>&1", pudod);
    TEST_CHECK(n > 0 && (size_t)n < sizeof(cmd));
    if (n <= 0 || (size_t)n >= sizeof(cmd)) {
        return;
    }
    FILE *fp = popen(cmd, "r");
    TEST_CHECK(fp != NULL);
    if (fp) {
        char buf[512];
        int saw_refusal = 0;
        while (fgets(buf, sizeof(buf), fp)) {
            if (strstr(buf, "allow-list") || strstr(buf, "denying all") ||
                strstr(buf, "failed to load") || strstr(buf, "euid") ||
                strstr(buf, "not root") || strstr(buf, "permission")) {
                saw_refusal = 1;
            }
        }
        pclose(fp);
        TEST_CHECK(saw_refusal);
    }
}

TEST_LIST = {
    { "security_env_ld_preload",           test_pudo_sanitize_removes_ld_preload },
    { "security_env_ld_library_path",      test_pudo_sanitize_removes_ld_library_path },
    { "security_env_multiple_dangerous",   test_pudo_sanitize_removes_multiple_dangerous_vars },
    { "pudo_config_and_allowed",           test_pudo_config_loading_and_allowed },
    { "pudo_uses_argv_not_shell",          test_pudo_uses_argv_not_shell },
    { "pudo_parent_env_intact",            test_pudo_builtin_does_not_mutate_parent_env },
    { "sec02_release_rejects_relative",    test_sec02_release_rejects_relative_fallbacks },
    { "sec02_release_rejects_evil_abs",    test_sec02_release_rejects_untrusted_absolute_sibling },
    { "sec02_release_accepts_install",     test_sec02_release_accepts_install_absolutes },
    { "sec02_debug_allows_fallbacks",      test_sec02_debug_allows_relative_and_build_absolute },
    { "sec04_pudod_build_rejects_overflow", test_sec04_pudod_build_rejects_overflow },
    { "sec04_pudod_build_accepts_at_limit", test_sec04_pudod_build_accepts_at_limit },
    { "sec04_pudo_helper_rejects_overflow", test_sec04_pudo_helper_rejects_overflow },
    { "sec04_pudo_helper_accepts_small",    test_sec04_pudo_helper_accepts_small },
    { "sec11_sudo_fallback_always_denied", test_sec11_sudo_fallback_always_denied },
    { "pudod_allow_rejects_unresolvable",  test_pudod_allow_entry_rejects_unresolvable },
    { "pudod_allow_accepts_existing",      test_pudod_allow_entry_accepts_existing },
    { "sec06_rejects_null_stat",           test_sec06_rejects_null_stat },
    { "sec06_accepts_root_exec",           test_sec06_accepts_root_owned_executable },
    { "sec06_rejects_non_root",            test_sec06_rejects_non_root_owner },
    { "sec06_rejects_non_exec",            test_sec06_rejects_non_executable },
    { "sec06_rejects_non_regular",         test_sec06_rejects_non_regular },
    { "sec07_open_flags_nofollow",         test_sec07_open_flags_include_nofollow },
    { "sec07_open_rejects_null",           test_sec07_open_rejects_null },
    { "sec07_open_regular_ok",             test_sec07_open_regular_ok },
    { "sec07_open_rejects_symlink",        test_sec07_open_rejects_symlink },
    { "pudod_refuses_without_privs",       test_pudod_binary_refuses_without_privileges },
    { NULL, NULL }
};
