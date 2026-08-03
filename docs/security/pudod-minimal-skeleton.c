/*
 * pudod-minimal-skeleton.c — Esqueleto mínimo para o helper setuid do petrush
 *
 * AVISO CRÍTICO DE SEGURANÇA (NUNCA IGNORE):
 * - Este binário será instalado com setuid root (ou capabilities).
 * - Qualquer bug aqui = potencial escalada de privilégio para quem conseguir
 *   invocá-lo (mesmo indiretamente via petrush).
 * - Tamanho deve permanecer < 400 LOC totais para ser auditável.
 * - TODO o código privilegiado vive aqui. petrush NUNCA pode conter lógica root.
 *
 * Princípios:
 * - Fail secure: qualquer dúvida → negar.
 * - Re-validar TUDO no lado root (nunca confiar no petrush).
 * - Protocolo: apenas argv. Sem sockets, sem env para decisões.
 * - Logging com UID real (getuid()).
 * - Env do filho construído explicitamente, mínimo.
 *
 * Compilação exemplo (para teste, sem setuid ainda):
 *   gcc -std=c23 -O2 -Wall -Wextra -Werror -D_GNU_SOURCE \
 *       -fstack-protector-strong -fPIE -pie -z relro -z now \
 *       -o pudod pudod-minimal-skeleton.c
 *
 * Instalação (só após revisão completa + aprovação do líder supremo):
 *   sudo chown root:root pudod
 *   sudo chmod 4755 pudod
 *   sudo mv pudod /usr/local/libexec/petrush-pudod
 *
 * Allow-list: por enquanto hardcoded neste esqueleto.
 * Em produção: ler /etc/petrush/pudo.allow (root:root, 0644 ou mais restrito),
 *              verificar st_uid==0 && !(st_mode & 022) antes de usar conteúdo.
 */

#define _GNU_SOURCE  /* para fexecve */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* ========================== CONSTANTES DE SEGURANÇA ========================== */

#define PUDOD_INSTALL_PATH "/usr/local/libexec/petrush-pudod"

/* Allow-list mínima para o esqueleto.
 * Em produção SUBSTITUIR por leitura de arquivo root-only.
 * Armazenar paths JÁ canonicalizados (realpath na instalação ou no admin).
 */
static const char * const ALLOWED_COMMANDS[] = {
    "/usr/bin/id",
    "/usr/bin/whoami",
    "/bin/ls",
    "/usr/bin/apt",
    "/usr/bin/apt-get",
    "/usr/bin/dnf",
    "/usr/bin/pacman",
    "/usr/bin/systemctl",
    "/bin/cat",
    "/usr/bin/head",
    "/usr/bin/tail",
    /* Adicionar só o que for realmente necessário. Menos = melhor. */
    NULL
};

/* Limites rígidos para evitar DoS / overflow */
#define MAX_ARGS 128
#define LOG_BUF_SIZE 512

/* ========================== FUNÇÕES AUXILIARES ========================== */

static void pudod_log(int priority, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char buf[LOG_BUF_SIZE];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    /* syslog sempre (auditoria) */
    syslog(LOG_AUTH | priority, "pudod: %s", buf);

    /* stderr também (vai para o usuário que invocou) */
    fprintf(stderr, "pudod: %s\n", buf);
}

static int is_allowed(const char *resolved_path)
{
    if (!resolved_path) return 0;

    for (int i = 0; ALLOWED_COMMANDS[i] != NULL; i++) {
        if (strcmp(resolved_path, ALLOWED_COMMANDS[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Constrói envp mínimo e seguro para o comando filho.
 * NUNCA herda o env do chamador (petrush ou pudod).
 */
static char **build_clean_envp(void)
{
    /* Array estático pequeno para simplicidade do esqueleto.
     * Em versão maior poderíamos alocar dinamicamente com limites. */
    static char *envp[16];
    int idx = 0;

    envp[idx++] = "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
    envp[idx++] = "TERM=xterm-256color";
    envp[idx++] = "HOME=/root";
    envp[idx++] = "USER=root";
    envp[idx++] = "LOGNAME=root";
    envp[idx++] = "SHELL=/bin/sh";
    envp[idx++] = NULL;

    return envp;
}

/* Monta argv para o comando filho a partir de argv do pudod.
 * pudod_argv[1] = comando (já resolvido)
 * pudod_argv[2..] = argumentos do usuário
 */
static char **build_child_argv(char * const pudod_argv[], const char *resolved)
{
    /* +1 para resolved, +1 para NULL */
    static char *child_argv[MAX_ARGS + 2];
    int out_idx = 0;

    child_argv[out_idx++] = (char *)resolved;  /* argv[0] do filho */

    /* Copia a partir de pudod_argv[2] */
    for (int i = 2; pudod_argv[i] != NULL && out_idx < MAX_ARGS + 1; i++) {
        child_argv[out_idx++] = pudod_argv[i];
    }
    child_argv[out_idx] = NULL;

    return child_argv;
}

/* ========================== MAIN — LÓGICA PRIVILEGIADA ========================== */

int main(int argc, char *argv[])
{
    char resolved[PATH_MAX];
    struct stat st;
    int fd = -1;
    int rc = 1;  /* fail secure por padrão */

    /* 1. Abrir syslog cedo */
    openlog("petrush-pudod", LOG_PID | LOG_CONS, LOG_AUTH);

    /* 2. Verificar que o binário foi invocado com privilégios elevados */
    if (geteuid() != 0) {
        pudod_log(LOG_ERR, "erro: euid != 0 (binário não está setuid root ou setcap)");
        goto cleanup;
    }

    /* 3. Sanitizar imediatamente o ambiente do próprio pudod.
       Ignoramos qualquer LD_* etc que o petrush (ou atacante) tentou passar. */
    const char *dangerous[] = {
        "LD_PRELOAD", "LD_LIBRARY_PATH", "LD_AUDIT", "LD_DEBUG",
        "PYTHONPATH", "PERL5LIB", "RUBYLIB", "NODE_PATH", "GOPATH",
        "IFS", "CDPATH", "ENV", "BASH_ENV", NULL
    };
    for (int i = 0; dangerous[i]; i++) {
        unsetenv(dangerous[i]);
    }

    /* 4. Validar número de argumentos */
    if (argc < 2) {
        pudod_log(LOG_ERR, "uso: %s <comando-absoluto> [args...]", argv[0]);
        goto cleanup;
    }

    const char *user_cmd = argv[1];

    /* 5. Exigir caminho absoluto (defesa contra cwd e PATH confusion) */
    if (user_cmd[0] != '/') {
        pudod_log(LOG_ERR, "comando rejeitado: deve ser caminho absoluto: %s", user_cmd);
        goto cleanup;
    }

    /* 6. Resolver symlinks e normalizar com realpath */
    if (realpath(user_cmd, resolved) == NULL) {
        pudod_log(LOG_ERR, "realpath falhou para %s: %s", user_cmd, strerror(errno));
        goto cleanup;
    }

    /* 7. Verificar allow-list (aqui hardcoded; em prod: ler arquivo root-only) */
    if (!is_allowed(resolved)) {
        pudod_log(LOG_WARNING, "comando negado pela allow-list: %s (resolved=%s) uid=%d",
                  user_cmd, resolved, (int)getuid());
        goto cleanup;
    }

    /* 8. Verificar o arquivo no disco (após realpath) */
    fd = open(resolved, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        pudod_log(LOG_ERR, "open falhou para %s: %s", resolved, strerror(errno));
        goto cleanup;
    }

    if (fstat(fd, &st) != 0) {
        pudod_log(LOG_ERR, "fstat falhou para %s: %s", resolved, strerror(errno));
        goto cleanup;
    }

    if (!S_ISREG(st.st_mode)) {
        pudod_log(LOG_ERR, "alvo não é arquivo regular: %s", resolved);
        goto cleanup;
    }

    /* Opcional: exigir que seja legível/executável por root.
       Aqui aceitamos qualquer regular que passou na allow-list.
       Em produção mais estrito: st.st_uid == 0 && (st.st_mode & 0111) */

    /* 9. Log de auditoria com UID REAL (getuid() é o caller original) */
    pudod_log(LOG_INFO, "executando uid=%d cmd=%s (resolved=%s) argc=%d",
              (int)getuid(), user_cmd, resolved, argc - 1);

    /* 10. Preparar argv e envp para o filho */
    char **child_argv = build_child_argv(argv, resolved);
    char **child_envp = build_clean_envp();

    /* 11. Executar o comando com privilégios (fexecve reduz TOCTOU) */
#ifdef __linux__
    fexecve(fd, child_argv, child_envp);
#else
    /* fallback sem fexecve */
    close(fd);
    fd = -1;
    execve(resolved, child_argv, child_envp);
#endif

    /* Se chegou aqui, exec falhou */
    pudod_log(LOG_ERR, "execve/fexecve falhou para %s: %s", resolved, strerror(errno));
    rc = 127;
    goto cleanup;

cleanup:
    if (fd >= 0) {
        close(fd);
    }
    closelog();
    _exit(rc);  /* usar _exit em setuid para evitar atexit hooks */
}
