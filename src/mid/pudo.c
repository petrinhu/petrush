/*
 * pudo.c — Implementação do builtin 'pudo'
 *
 * Objetivo: Fornecer uma interface builtin similar ao sudo,
 * com ênfase extrema em segurança.
 *
 * Arquitetura de segurança (Fase 1):
 * - Este código roda como o usuário normal (nunca privilegiado).
 * - Toda elevação de privilégio é delegada ao binário separado `pudod`.
 * - Aqui fazemos: parsing, policy client-side, logging e argv/envp
 *   limpos só no execve do filho (não mutar o ambiente do petrush).
 *
 * O pudod (em src/pudod/) é o único componente que roda com privilégios.
 * Ele re-valida TUDO.
 */

#include "petrush/pudo.h"
#include "petrush/dispatcher.h"
#include "petrush/env.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <pwd.h>
#include <syslog.h>
#include <stdarg.h>

/* ========================== CONFIGURAÇÃO ========================== */

#define PUDO_CONFIG_DIR  ".config/petrush"
#define PUDO_CONFIG_FILE "pudo.conf"

/* Path canônico do helper instalado (espelha pudod.c). */
#ifndef PUDOD_INSTALL_PATH
#define PUDOD_INSTALL_PATH "/usr/local/libexec/petrush-pudod"
#endif

/* Estrutura simples de configuração (será expandida) */
typedef struct {
    int ask_password;           /* 1 = pedir senha quando necessário */
    int log_commands;           /* 1 = logar todas as invocações */
    char **allowed_commands;    /* lista de comandos permitidos (NULL-terminated) */
    int allowed_count;
} pudo_config_t;

static pudo_config_t g_pudo_config = {0};

/* ========================== FUNÇÕES INTERNAS ========================== */

static int load_pudo_config(pudo_config_t *cfg);
static void free_pudo_config(pudo_config_t *cfg);
static int is_command_allowed(const char *cmd);
static int run_via_pudod(petrush_cmd_t *cmd);

/* ========================== LOGGING BÁSICO (auditoria) ========================== */

__attribute__((format(printf, 1, 2)))
static void pudo_log(const char *fmt, ...) /* NOLINT(clang-analyzer-security.VAList) */
{
    char buf[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);  // NOLINT(clang-analyzer-security.VAList)
    va_end(ap);

    /* Sempre para stderr do usuário */
    fputs(buf, stderr);

    /* Auditoria via syslog */
    syslog(LOG_AUTH | LOG_INFO, "pudo: %s", buf);
}

/* ========================== BUILTIN ========================== */

int builtin_pudo(petrush_cmd_t *cmd)
{
    if (!cmd || cmd->argc < 2) {
        fprintf(stderr, "pudo: uso: pudo [opções] comando [args...]\n");
        fprintf(stderr, "pudo: use 'pudo --help' para mais informações.\n");
        return 1;
    }

    /* Abre syslog para auditoria */
    openlog("petrush-pudo", LOG_PID | LOG_CONS, LOG_AUTH);

    /* Carrega configuração */
    if (load_pudo_config(&g_pudo_config) != 0) {
        fprintf(stderr, "pudo: erro ao carregar configuração\n");
        closelog();
        return 1;
    }

    /* Parsing muito básico por enquanto (vamos evoluir) */
    int arg_start = 1;

    /* Suporte básico a --help */
    if (strcmp(cmd->argv[1], "--help") == 0 || strcmp(cmd->argv[1], "-h") == 0) {
        printf("pudo — execução de comandos com privilégios (helper pudod)\n\n");
        printf("Uso: pudo [opções] comando [argumentos]\n\n");
        printf("Frontend unprivilegiado que delega para o pudod (helper setuid mínimo).\n");
        printf("Ver docs/design/pudo.md e docs/security/pudo-audit.md\n");
        free_pudo_config(&g_pudo_config);
        closelog();
        return 0;
    }

    /* SEC-01: não unsetenv no processo petrush. Filho recebe build_clean_envp(). */

    /* Verifica se o comando está na allow-list (se configurada) */
    const char *target_cmd = cmd->argv[arg_start];
    if (g_pudo_config.allowed_count > 0 && !is_command_allowed(target_cmd)) {
        pudo_log("comando negado pela allow-list: %s\n", target_cmd);
        free_pudo_config(&g_pudo_config);
        closelog();
        return 1;
    }

    /* Log da tentativa (auditoria) */
    pudo_log("invocação uid=%d cmd=%s\n", (int)getuid(), target_cmd);

    /* Executa via pudod (NEW-05 Fase 1) */
    int rc = run_via_pudod(cmd);

    free_pudo_config(&g_pudo_config);
    closelog();
    return rc;
}

/* ========================== IMPLEMENTAÇÕES (STUBS) ========================== */

static int load_pudo_config(pudo_config_t *cfg)
{
    if (!cfg) return -1;

    /* Limpa estado anterior */
    memset(cfg, 0, sizeof(*cfg));

    const char *home = petrush_getenv("HOME");
    if (!home) home = ".";

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/.config/petrush/pudo.conf", home);

    FILE *f = fopen(path, "r");
    if (!f) {
        /* Arquivo não existe: sem allow-list client-side (pudod é autoritativo) */
        return 0;
    }

    /* Contagem inicial para alocar */
    int count = 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '#' || *p == '\n') continue;
        count++;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }

    if (count == 0) {
        fclose(f);
        return 0;
    }

    cfg->allowed_commands = calloc((size_t)count + 1, sizeof(char*));
    if (!cfg->allowed_commands) {
        int saved = errno;
        fclose(f);
        errno = saved;
        return -1;
    }

    int i = 0;
    while (fgets(line, sizeof(line), f) && i < count) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '#' || *p == '\n') continue;

        /* Remove newline */
        size_t len = strlen(p);
        if (len > 0 && p[len-1] == '\n') p[len-1] = '\0';

        /* Armazena cópia */
        cfg->allowed_commands[i] = strdup(p);
        if (cfg->allowed_commands[i]) i++;
    }
    cfg->allowed_count = i;
    cfg->allowed_commands[i] = NULL;

    fclose(f);

    /* Opcional: log quando allow-list carregada */
    /* fprintf(stderr, "pudo: allow-list carregada com %d comandos\n", i); */

    return 0;
}

static void free_pudo_config(pudo_config_t *cfg)
{
    if (!cfg) return;
    if (cfg->allowed_commands) {
        for (int i = 0; i < cfg->allowed_count; i++) {
            free(cfg->allowed_commands[i]);
        }
        free(cfg->allowed_commands);
    }
    memset(cfg, 0, sizeof(*cfg));
}

static int is_command_allowed(const char *cmd)
{
    /* Se não há lista configurada, permite tudo (comportamento inicial) */
    if (g_pudo_config.allowed_count == 0) {
        return 1;
    }

    for (int i = 0; i < g_pudo_config.allowed_count; i++) {
        if (strcmp(g_pudo_config.allowed_commands[i], cmd) == 0) {
            return 1;
        }
    }
    return 0;
}

int pudo_sanitize_environment(void)
{
    /*
     * Sanitização agressiva de ambiente.
     * Esta é uma das partes mais críticas de segurança do pudo.
     *
     * Removemos todas as variáveis que podem ser usadas para
     * injeção de bibliotecas ou alteração de comportamento de programas.
     */
    const char *dangerous_ld[] = {
        "LD_PRELOAD",
        "LD_LIBRARY_PATH",
        "LD_AUDIT",
        "LD_DEBUG",
        "LD_ORIGIN_PATH",
        "LD_PROFILE",
        "LD_USE_LOAD_BIAS",
        "LD_DYNAMIC_WEAK",
        "LD_SHOW_AUXV",
        "LD_TRACE_LOADED_OBJECTS",
        NULL
    };

    const char *dangerous_lang[] = {
        "PYTHONPATH",
        "PERL5LIB",
        "PERL5OPT",
        "RUBYLIB",
        "RUBYOPT",
        "NODE_PATH",
        "GOPATH",
        "GOCACHE",
        "JAVA_TOOL_OPTIONS",
        "_JAVA_OPTIONS",
        "CLASSPATH",
        NULL
    };

    const char *dangerous_other[] = {
        "IFS",
        "CDPATH",
        "ENV",
        "BASH_ENV",
        "SHELLOPTS",
        "PS4",
        "GTK_PATH",
        "GDK_PIXBUF_MODULE_FILE",
        "QT_PLUGIN_PATH",
        "XDG_DATA_DIRS",
        NULL
    };

    int failed = 0;

    /* LD_* family */
    for (int i = 0; dangerous_ld[i]; i++) {
        if (petrush_getenv(dangerous_ld[i])) {
            if (unsetenv(dangerous_ld[i]) != 0) {
                fprintf(stderr, "pudo: falha ao remover %s\n", dangerous_ld[i]);
                failed = 1;
            }
        }
    }

    /* Linguagens interpretadas */
    for (int i = 0; dangerous_lang[i]; i++) {
        if (petrush_getenv(dangerous_lang[i])) {
            if (unsetenv(dangerous_lang[i]) != 0) {
                fprintf(stderr, "pudo: falha ao remover %s\n", dangerous_lang[i]);
                failed = 1;
            }
        }
    }

    /* Outras variáveis perigosas */
    for (int i = 0; dangerous_other[i]; i++) {
        if (petrush_getenv(dangerous_other[i])) {
            if (unsetenv(dangerous_other[i]) != 0) {
                fprintf(stderr, "pudo: falha ao remover %s\n", dangerous_other[i]);
                failed = 1;
            }
        }
    }

    return failed ? -1 : 0;
}

/*
 * Constrói um envp limpo e mínimo para passar ao pudod (ou sudo fallback).
 * NÃO muta o ambiente do processo pai (petrush).
 */
static char **build_clean_envp(void)
{
    /* Array pequeno e estático é suficiente para este caso */
    static char *env[32];
    int i = 0;

    /* Copia apenas variáveis "seguras" do ambiente atual, ou define mínimas */
    const char *keep[] = { 
        "HOME", "USER", "LOGNAME", "TERM", "SHELL", "PATH", 
        "LANG", "LC_ALL", "LC_CTYPE", "TZ", "TMPDIR", "TMP", "TEMP",
        NULL 
    };

    for (int k = 0; keep[k] && i < 30; k++) {
        const char *val = petrush_getenv(keep[k]);
        if (val) {
            /* Formato "VAR=valor" — alocamos com snprintf simples */
            static char buffers[32][512];  /* limite prático */
            int b = i;
            snprintf(buffers[b], sizeof(buffers[b]), "%s=%s", keep[k], val);
            env[i++] = buffers[b];
        }
    }

    /* Adiciona algumas mínimas se ausentes */
    if (!petrush_getenv("PATH")) {
        env[i++] = "PATH=/usr/local/bin:/usr/bin:/bin";
    }

    env[i] = NULL;
    return env;
}

/* Prefixos de install confiáveis (SEC-02 Release). */
static const char *const k_pudod_install_dirs[] = {
    "/usr/local/libexec",
    "/usr/local/bin",
    "/usr/libexec",
    NULL
};

static int pudod_basename_ok(const char *base)
{
    return base && (strcmp(base, "pudod") == 0 || strcmp(base, "petrush-pudod") == 0);
}

/*
 * SEC-04: cabe o argv do helper em PUDO_HELPER_ARGV_MAX (com NULL)?
 * via_sudo: sudo + "--" + cmd->argv[1..]
 * !via_sudo: pudod + target + cmd->argv[2..]
 */
int pudo_helper_argv_fits(int cmd_argc, int via_sudo)
{
    if (cmd_argc < 2) {
        return 0;
    }

    int fixed = 2;
    int payload = via_sudo ? (cmd_argc - 1) : (cmd_argc - 2);
    if (payload < 0) {
        return 0;
    }
    /* fixed + payload + NULL <= MAX */
    if (fixed + payload + 1 > PUDO_HELPER_ARGV_MAX) {
        return 0;
    }
    return 1;
}

/*
 * SEC-02: política pura de aceitação do path do helper.
 * release_mode: 1 = só absolutos em dirs de install; 0 = debug (relativos + qualquer abs).
 * Fail closed.
 */
int pudo_allow_pudod_candidate(const char *path, int release_mode)
{
    if (!path || path[0] == '\0') {
        return 0;
    }

    if (release_mode) {
        /* Relativo: nunca em Release. */
        if (path[0] != '/') {
            return 0;
        }

        /* Match exato do path canônico / lista de install. */
        if (strcmp(path, PUDOD_INSTALL_PATH) == 0) {
            return 1;
        }
        {
            static const char *const exact[] = {
                "/usr/local/libexec/petrush-pudod",
                "/usr/local/bin/petrush-pudod",
                "/usr/local/bin/pudod",
                "/usr/libexec/petrush-pudod",
                NULL
            };
            for (int i = 0; exact[i]; i++) {
                if (strcmp(path, exact[i]) == 0) {
                    return 1;
                }
            }
        }

        /* Sibling sob dir de install: basename pudod|petrush-pudod. */
        {
            const char *base = strrchr(path, '/');
            if (!base || base == path) {
                return 0;
            }
            base++;
            if (!pudod_basename_ok(base)) {
                return 0;
            }
            size_t dir_len = (size_t)(base - path - 1);
            for (int i = 0; k_pudod_install_dirs[i]; i++) {
                size_t want = strlen(k_pudod_install_dirs[i]);
                if (dir_len == want &&
                    strncmp(path, k_pudod_install_dirs[i], want) == 0) {
                    return 1;
                }
            }
        }
        return 0;
    }

    /* Debug: absolutos OK (build tree sibling); relativos só da lista conhecida. */
    if (path[0] == '/') {
        return 1;
    }
    {
        static const char *const debug_rel[] = {
            "build/pudod",
            "./build/pudod",
            "../build/pudod",
            "./pudod",
            "pudod",
            NULL
        };
        for (int i = 0; debug_rel[i]; i++) {
            if (strcmp(path, debug_rel[i]) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

/*
 * Candidato absoluto: access + realpath + allow. Copia o path aceito em out.
 * Fail closed se não houver absoluto utilizável. Nao muda pudo_allow_*.
 */
static const char *try_abs_candidate(const char *path, char *out, size_t out_sz,
                                     int release_mode)
{
    char resolved[PATH_MAX];
    const char *cand;

    if (!path || path[0] == '\0' || !out || out_sz == 0) {
        return NULL;
    }
    if (access(path, X_OK) != 0) {
        return NULL;
    }
    cand = path;
    if (realpath(path, resolved)) {
        cand = resolved;
    }
    if (cand[0] != '/') {
        return NULL;
    }
    if (!pudo_allow_pudod_candidate(cand, release_mode)) {
        return NULL;
    }
    if (snprintf(out, out_sz, "%s", cand) >= (int)out_sz) {
        return NULL;
    }
    return out;
}

/*
 * Encontra o binário pudod em locais conhecidos (dev + install).
 * Retorna caminho utilizável ou NULL.
 *
 * SEC-02: em Release (NDEBUG), só absolutos confiáveis (install / sibling
 * sob dir de install). Fallbacks relativos e access("pudod") só em debug.
 * Sibling e install permanecem loops separados (nao unificar).
 */
static const char *find_pudod_binary(void)
{
    static char found[PATH_MAX];
    char self[PATH_MAX];
    ssize_t n;
#ifdef NDEBUG
    const int release_mode = 1;
#else
    const int release_mode = 0;
#endif

    /* Sibling de /proc/self/exe — vetor principal em Release (Cosmo/Narciso). */
    n = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (n > 0) {
        self[n] = '\0';
        char *slash = strrchr(self, '/');
        if (slash) {
            *slash = '\0';
            static const char *const names[] = { "pudod", "petrush-pudod", NULL };
            for (int i = 0; names[i]; i++) {
                char cand[PATH_MAX];
                if (snprintf(cand, sizeof(cand), "%s/%s", self, names[i]) >=
                    (int)sizeof(cand)) {
                    continue;
                }
                if (try_abs_candidate(cand, found, sizeof(found), release_mode)) {
                    return found;
                }
            }
        }
    }

#ifndef NDEBUG
    /* Desenvolvimento: caminhos relativos comuns (só debug). */
    {
        const char *dev_paths[] = {
            "build/pudod",
            "./build/pudod",
            "../build/pudod",
            NULL
        };
        for (int i = 0; dev_paths[i]; i++) {
            if (access(dev_paths[i], X_OK) != 0) {
                continue;
            }
            if (realpath(dev_paths[i], found)) {
                if (pudo_allow_pudod_candidate(found, release_mode)) {
                    return found;
                }
            } else if (pudo_allow_pudod_candidate(dev_paths[i], release_mode)) {
                return dev_paths[i];
            }
        }
    }
#endif

    /* Instalação padrão (sempre absoluto). Loop separado do sibling (SEC-02). */
    {
        const char *install_paths[] = {
            PUDOD_INSTALL_PATH,
            "/usr/local/libexec/petrush-pudod",
            "/usr/local/bin/petrush-pudod",
            "/usr/local/bin/pudod",
            "/usr/libexec/petrush-pudod",
            NULL
        };
        for (int i = 0; install_paths[i]; i++) {
            if (try_abs_candidate(install_paths[i], found, sizeof(found),
                                  release_mode)) {
                return found;
            }
        }
    }

#ifndef NDEBUG
    /* Fallback relativo no cwd: só debug. */
    if (access("pudod", X_OK) == 0 &&
        pudo_allow_pudod_candidate("pudod", release_mode)) {
        return "pudod";
    }
#endif

    return NULL;
}

/*
 * Resolve um nome de comando para caminho absoluto (melhor esforço no lado unpriv).
 * Usado para passar argv[1] absoluto para o pudod.
 */
static char *resolve_target_absolute(const char *name)
{
    if (!name) return NULL;

    if (strchr(name, '/')) {
        /* Já tem barra — verificar se executável */
        if (access(name, X_OK) == 0) {
            char *abs = realpath(name, NULL);
            return abs ? abs : strdup(name);
        }
        return NULL;
    }

    /* Procurar no PATH */
    const char *path_env = petrush_getenv("PATH");
    if (!path_env) path_env = "/usr/local/bin:/usr/bin:/bin";

    char path_copy[4096];
    snprintf(path_copy, sizeof(path_copy), "%s", path_env);

    char *dir = strtok(path_copy, ":");
    char full[PATH_MAX];
    while (dir) {
        snprintf(full, sizeof(full), "%s/%s", dir, name);
        if (access(full, X_OK) == 0) {
            return realpath(full, NULL);
        }
        dir = strtok(NULL, ":");
    }
    return NULL;
}

static int run_via_pudod(petrush_cmd_t *cmd)
{
    /*
     * Fase 1: Delega execução privilegiada ao pudod (helper setuid mínimo).
     *
     * Protocolo:
     *   pudod <caminho-absoluto-do-comando> [arg1] [arg2] ...
     *
     * O pudod é responsável por TODA validação privilegiada + allow-list + exec.
     * petrush faz checagens client-side para UX e env sanitize.
     */

    const char *pudod = find_pudod_binary();
    if (!pudod) {
        /* Fallback conservador: usa sudo do sistema.
         * Recomendado apenas se pudod indisponível. Prefira pudod (helper mínimo) em produção.
         */
        fprintf(stderr, "pudo: aviso: pudod não encontrado, usando sudo como fallback\n");

        /* SEC-04: fail closed se argc nao cabe em sudo_argv[128] */
        if (!pudo_helper_argv_fits(cmd->argc, 1)) {
            fprintf(stderr,
                    "pudo: too many arguments (limit %d), refusing truncated exec\n",
                    PUDO_HELPER_ARGV_MAX - 3);
            return 1;
        }

        char *sudo_argv[PUDO_HELPER_ARGV_MAX];
        int sidx = 0;
        sudo_argv[sidx++] = "/usr/bin/sudo";
        sudo_argv[sidx++] = "--";
        for (int i = 1; i < cmd->argc; i++) {
            sudo_argv[sidx++] = cmd->argv[i];
        }
        sudo_argv[sidx] = NULL;

        char **clean = build_clean_envp();
        execve("/usr/bin/sudo", sudo_argv, clean);
        /* fallback mais bruto */
        execvp("sudo", sudo_argv);
        perror("pudo: falha no fallback sudo");
        return 127;
    }

    if (cmd->argc < 2) return 127;

    /* SEC-04: fail closed se argc nao cabe em pudod_argv[128] */
    if (!pudo_helper_argv_fits(cmd->argc, 0)) {
        fprintf(stderr,
                "pudo: too many arguments (limit %d), refusing truncated exec\n",
                PUDO_HELPER_ARGV_MAX - 3);
        return 1;
    }

    /* Resolve o comando alvo para absoluto quando possível */
    char *abs_target = resolve_target_absolute(cmd->argv[1]);
    const char *target_to_pass = abs_target ? abs_target : cmd->argv[1];

    /* Monta argv para pudod: pudod <target-abs> [resto dos args do usuário] */
    char *pudod_argv[PUDO_HELPER_ARGV_MAX];
    int idx = 0;

    pudod_argv[idx++] = (char *)pudod;
    pudod_argv[idx++] = (char *)target_to_pass;

    for (int i = 2; i < cmd->argc; i++) {
        pudod_argv[idx++] = cmd->argv[i];
    }
    pudod_argv[idx] = NULL;

    /* Ambiente limpo */
    char **clean_env = build_clean_envp();

    execve(pudod, pudod_argv, clean_env);

    /* Falha */
    perror("pudo: falha ao executar pudod");
    free(abs_target);
    return 127;
}
