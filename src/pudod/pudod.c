/*
 * pudod.c — Helper setuid mínimo para o builtin 'pudo' do petrush
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
 * Allow-list: carregada de /etc/petrush/pudo.allow (deve ser root-owned, sem write para outros).
 * Verificações de permissão e dono são OBRIGATÓRIAS antes de confiar no conteúdo.
 */

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
#include <stdarg.h>

#include "allow_resolve.h"
#include "child_argv.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* ========================== CONSTANTES DE SEGURANÇA ========================== */

#define PUDOD_INSTALL_PATH "/usr/local/libexec/petrush-pudod"
#define ALLOW_LIST_FILE "/etc/petrush/pudo.allow"

/* Allow-list carregada de arquivo seguro (root owned, restrito).
 * Máximo de comandos permitidos.
 */
#define MAX_ALLOWED 64
static char allowed_paths[MAX_ALLOWED][PATH_MAX];
static int allowed_count = 0;

/* Limites rígidos para evitar DoS / overflow (MAX_ARGS = PUDOD_MAX_ARGS) */
#define LOG_BUF_SIZE 512

/* ========================== FUNÇÕES AUXILIARES ========================== */

__attribute__((format(printf, 2, 3)))
static void pudod_log(int priority, const char *fmt, ...) /* NOLINT(clang-analyzer-security.VAList) */
{
    char buf[LOG_BUF_SIZE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);  /* NOLINT */
    va_end(ap);

    /* syslog sempre (auditoria) */
    syslog(LOG_AUTH | priority, "pudod: %s", buf);

    /* stderr também (vai para o usuário que invocou) */
    fprintf(stderr, "pudod: %s\n", buf);
}

/* Carrega a allow-list de /etc/petrush/pudo.allow com verificações rigorosas de segurança.
 * Retorna 0 em sucesso, -1 em falha (fail secure: não carrega nada).
 */
static int load_allow_list(void)
{
    int fd = open(ALLOW_LIST_FILE, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        pudod_log(LOG_ERR, "allow-list file not found or unreadable: %s", ALLOW_LIST_FILE);
        return -1;
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        pudod_log(LOG_ERR, "fstat failed on allow-list");
        close(fd);
        return -1;
    }

    if (st.st_uid != 0) {
        pudod_log(LOG_ERR, "allow-list must be owned by root");
        close(fd);
        return -1;
    }

    if (st.st_mode & 022) {
        pudod_log(LOG_ERR, "allow-list must not be writable by group/other");
        close(fd);
        return -1;
    }

    FILE *f = fdopen(fd, "r");
    if (!f) {
        close(fd);
        return -1;
    }

    char line[PATH_MAX];
    allowed_count = 0;

    while (fgets(line, sizeof(line), f) && allowed_count < MAX_ALLOWED) {
        /* trim leading whitespace */
        char *p = line;
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;

        if (*p == '\0' || *p == '#') continue; /* empty or comment */

        /* remove trailing whitespace/newline */
        char *end = p + strlen(p) - 1;
        while (end > p && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
            *end-- = '\0';
        }

        if (*p == '/' && allowed_count < MAX_ALLOWED) {
            char canonical[PATH_MAX];
            /* SEC-05: realpath falhou => skip (fail closed; nunca aceitar literal). */
            if (pudod_resolve_allow_entry(p, canonical, sizeof(canonical)) == 0) {
                size_t len = strlen(canonical);
                if (len >= PATH_MAX) len = PATH_MAX - 1;
                memcpy(allowed_paths[allowed_count], canonical, len);
                allowed_paths[allowed_count][len] = '\0';
                allowed_count++;
            } else {
                pudod_log(LOG_WARNING,
                          "allow-list entry skipped (realpath failed): %s", p);
            }
        }
    }

    fclose(f);
    pudod_log(LOG_INFO, "loaded %d allowed commands from %s", allowed_count, ALLOW_LIST_FILE);
    return 0;
}

static int is_allowed(const char *resolved_path)
{
    if (!resolved_path) return 0;

    for (int i = 0; i < allowed_count; i++) {
        if (strcmp(resolved_path, allowed_paths[i]) == 0) {
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

/* ========================== MAIN — LÓGICA PRIVILEGIADA ========================== */

int main(int argc, char *argv[])
{
    char resolved[PATH_MAX];
    struct stat st;
    int fd = -1;
    int rc = 1;  /* fail secure por padrão */

    /* 1. Abrir syslog cedo */
    openlog("petrush-pudod", LOG_PID | LOG_CONS, LOG_AUTH);

    /* 2. Carregar allow-list com verificações de segurança (fail secure) */
    if (load_allow_list() != 0) {
        pudod_log(LOG_ERR, "failed to load secure allow-list, denying all");
        goto cleanup;
    }

    /* 4. Verificar que o binário foi invocado com privilégios elevados */
    if (geteuid() != 0) {
        pudod_log(LOG_ERR, "erro: euid != 0 (binário não está setuid root ou setcap)");
        goto cleanup;
    }

    /* 5. Sanitizar imediatamente o ambiente do próprio pudod.
       Ignoramos qualquer LD_* etc que o petrush (ou atacante) tentou passar. */
    const char *dangerous[] = {
        "LD_PRELOAD", "LD_LIBRARY_PATH", "LD_AUDIT", "LD_DEBUG",
        "LD_ORIGIN_PATH", "LD_PROFILE",
        "PYTHONPATH", "PERL5LIB", "PERL5OPT", "RUBYLIB", "RUBYOPT", "NODE_PATH", "GOPATH",
        "JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "CLASSPATH",
        "IFS", "CDPATH", "ENV", "BASH_ENV", "SHELLOPTS",
        "GTK_PATH", "GDK_PIXBUF_MODULE_FILE", "QT_PLUGIN_PATH",
        NULL
    };
    for (int i = 0; dangerous[i]; i++) {
        unsetenv(dangerous[i]);
    }

    /* 6. Validar número de argumentos */
    if (argc < 2) {
        pudod_log(LOG_ERR, "uso: %s <comando-absoluto> [args...]", argv[0]);
        goto cleanup;
    }

    const char *user_cmd = argv[1];

    /* 7. Exigir caminho absoluto (defesa contra cwd e PATH confusion) */
    if (user_cmd[0] != '/') {
        pudod_log(LOG_ERR, "comando rejeitado: deve ser caminho absoluto: %s", user_cmd);
        goto cleanup;
    }

    /* 8. Resolver symlinks e normalizar com realpath */
    if (realpath(user_cmd, resolved) == NULL) {
        pudod_log(LOG_ERR, "realpath falhou para %s: %s", user_cmd, strerror(errno));
        goto cleanup;
    }

    /* 9. Verificar allow-list */
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

    /* 10. Preparar argv e envp para o filho (SEC-04: fail closed se overflow) */
    static char *child_argv_buf[PUDOD_MAX_ARGS + 2];
    if (pudod_build_child_argv(argv, argc, resolved, child_argv_buf,
                               PUDOD_MAX_ARGS + 2) != 0) {
        pudod_log(LOG_ERR, "argc exceeds MAX_ARGS (%d), refusing truncated exec",
                  PUDOD_MAX_ARGS);
        goto cleanup;
    }
    char **child_argv = child_argv_buf;
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
