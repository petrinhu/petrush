/*
 * plugin_load.c - Loader Foundation de plugins .so (PLG-LOAD)
 *
 * Fluxo: XDG/PETRUSH_PLUGIN_PATH -> realpath -> recusa world-writable
 * (ficheiro + dirs) -> allow-list path+SHA-256 -> dlopen -> query/ABI -> init.
 * Consome docs/security/plugins-threat.md. pudod NAO liga este TU. Sem 4755.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "petrush/plugin_load.h"

#include "abi.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <openssl/crypto.h>
#include <openssl/evp.h>

#ifdef PETRUSH_HAVE_ASM
#include "petrush/asm.h"
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

static int parse_sha256_hex(const char *hex, unsigned char out[PETRUSH_PLUGIN_SHA256_LEN])
{
    if (!hex || !out) {
        return -1;
    }
    for (size_t i = 0; i < PETRUSH_PLUGIN_SHA256_LEN; i++) {
        int hi = hex_nibble(hex[i * 2]);
        int lo = hex_nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return -1;
        }
        out[i] = (unsigned char)((hi << 4) | lo);
    }
    if (hex[PETRUSH_PLUGIN_SHA256_HEX_LEN] != '\0') {
        return -1;
    }
    return 0;
}

static int ct_mem_eq(const void *a, const void *b, size_t n)
{
#ifdef PETRUSH_HAVE_ASM
    return petrush_memeq_ct(a, b, n);
#else
    if (n == 0) {
        return 0;
    }
    if (!a || !b) {
        return 1;
    }
    return CRYPTO_memcmp(a, b, n) == 0 ? 0 : 1;
#endif
}

static int is_hex64(const char *s)
{
    if (!s) {
        return 0;
    }
    size_t n = strlen(s);
    if (n != PETRUSH_PLUGIN_SHA256_HEX_LEN) {
        return 0;
    }
    for (size_t i = 0; i < n; i++) {
        if (hex_nibble(s[i]) < 0) {
            return 0;
        }
    }
    return 1;
}

int petrush_plugin_path_writable_ok(const char *canon_path)
{
    if (!canon_path || canon_path[0] != '/') {
        return -1;
    }

    struct stat st;
    if (lstat(canon_path, &st) != 0) {
        return -1;
    }
    if (!S_ISREG(st.st_mode)) {
        return -1;
    }
    if ((st.st_mode & S_IWOTH) != 0) {
        return -1;
    }

    /* Walk cada diretorio prefixo do path canonico. */
    char dirbuf[PETRUSH_PLG_PATH_MAX];
    size_t len = strlen(canon_path);
    if (len == 0 || len >= sizeof(dirbuf)) {
        return -1;
    }
    memcpy(dirbuf, canon_path, len + 1);

    for (;;) {
        char *slash = strrchr(dirbuf, '/');
        if (!slash) {
            return -1;
        }
        if (slash == dirbuf) {
            /* raiz "/" */
            slash[1] = '\0';
        } else {
            *slash = '\0';
        }

        if (stat(dirbuf, &st) != 0) {
            return -1;
        }
        if (!S_ISDIR(st.st_mode)) {
            return -1;
        }
        /* Obrigatorio PLG-NARC §6/§14: recusar o+w. Sticky NAO excepciona IWOTH
         * (bloqueia /tmp e /var/tmp 1777). IWGRP+sticky fica como defesa futura. */
        if ((st.st_mode & S_IWOTH) != 0) {
            return -1;
        }

        if (dirbuf[0] == '/' && dirbuf[1] == '\0') {
            break;
        }
    }
    return 0;
}

int petrush_plugin_sha256_fd(int fd, unsigned char out[PETRUSH_PLUGIN_SHA256_LEN])
{
    if (fd < 0 || !out) {
        return -1;
    }
    if (lseek(fd, 0, SEEK_SET) < 0) {
        return -1;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return -1;
    }
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        return -1;
    }

    unsigned char buf[8192];
    for (;;) {
        ssize_t r = read(fd, buf, sizeof(buf));
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            EVP_MD_CTX_free(ctx);
            return -1;
        }
        if (r == 0) {
            break;
        }
        if (EVP_DigestUpdate(ctx, buf, (size_t)r) != 1) {
            EVP_MD_CTX_free(ctx);
            return -1;
        }
    }

    unsigned int outlen = 0;
    if (EVP_DigestFinal_ex(ctx, out, &outlen) != 1 ||
        outlen != PETRUSH_PLUGIN_SHA256_LEN) {
        EVP_MD_CTX_free(ctx);
        return -1;
    }
    EVP_MD_CTX_free(ctx);
    return 0;
}

void petrush_plugin_sha256_to_hex(const unsigned char dig[PETRUSH_PLUGIN_SHA256_LEN],
                                  char out[PETRUSH_PLUGIN_SHA256_HEX_LEN + 1])
{
    static const char *hexd = "0123456789abcdef";
    if (!out) {
        return;
    }
    if (!dig) {
        out[0] = '\0';
        return;
    }
    for (size_t i = 0; i < PETRUSH_PLUGIN_SHA256_LEN; i++) {
        out[i * 2] = hexd[(dig[i] >> 4) & 0xf];
        out[i * 2 + 1] = hexd[dig[i] & 0xf];
    }
    out[PETRUSH_PLUGIN_SHA256_HEX_LEN] = '\0';
}

int petrush_plugin_sha256_hex_eq(const char *a, const char *b)
{
    unsigned char da[PETRUSH_PLUGIN_SHA256_LEN];
    unsigned char db[PETRUSH_PLUGIN_SHA256_LEN];
    if (parse_sha256_hex(a, da) != 0 || parse_sha256_hex(b, db) != 0) {
        return -1;
    }
    return ct_mem_eq(da, db, PETRUSH_PLUGIN_SHA256_LEN);
}

static int allow_file_mode_ok(const struct stat *st)
{
    if (!st || !S_ISREG(st->st_mode)) {
        return 0;
    }
    if ((st->st_mode & S_IWOTH) != 0) {
        return 0;
    }
    return 1;
}

int petrush_plugin_allow_parse_file(const char *path, petrush_plugin_allow_t *out)
{
    if (!out) {
        return -1;
    }
    out->n = 0;
    if (!path || !path[0]) {
        return -1;
    }

    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }
    if (!allow_file_mode_ok(&st)) {
        return -1;
    }

    FILE *fp = fopen(path, "re");
    if (!fp) {
        return -1;
    }

    char line[PETRUSH_PLG_PATH_MAX + PETRUSH_PLUGIN_SHA256_HEX_LEN + 64];
    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        char *p = line;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0' || *p == '\n' || *p == '#') {
            continue;
        }
        char *nl = strchr(p, '\n');
        if (nl) {
            *nl = '\0';
        }

        char *sp = p;
        while (*sp && *sp != ' ' && *sp != '\t') {
            sp++;
        }
        if (*sp == '\0') {
            continue; /* linha malformada: ignora */
        }
        *sp = '\0';
        sp++;
        while (*sp == ' ' || *sp == '\t') {
            sp++;
        }
        if (*sp == '\0') {
            continue;
        }
        char *end = sp;
        while (*end && *end != ' ' && *end != '\t') {
            end++;
        }
        *end = '\0';

        const char *path_tok = p;
        const char *hash_tok = sp;

        /* So path absoluto; sem globs; hash hex64 */
        if (path_tok[0] != '/' || strchr(path_tok, '*') != NULL) {
            continue;
        }
        if (!is_hex64(hash_tok)) {
            continue;
        }
        if (out->n >= PETRUSH_PLUGIN_ALLOW_MAX) {
            fclose(fp);
            out->n = 0;
            return -1;
        }

        char canon[PETRUSH_PLG_PATH_MAX];
        if (realpath(path_tok, canon) == NULL) {
            /* fail-closed na entrada: nao copia literal (espelho SEC-05) */
            continue;
        }
        size_t i = out->n;
        if (snprintf(out->entries[i].path, sizeof(out->entries[i].path), "%s",
                     canon) >= (int)sizeof(out->entries[i].path)) {
            continue;
        }
        memcpy(out->entries[i].sha256_hex, hash_tok,
               PETRUSH_PLUGIN_SHA256_HEX_LEN + 1);
        out->n++;
    }

    if (ferror(fp)) {
        fclose(fp);
        out->n = 0;
        return -1;
    }
    fclose(fp);
    return 0;
}

int petrush_plugin_allow_find(const petrush_plugin_allow_t *list,
                              const char *canon_path,
                              char sha_out[PETRUSH_PLUGIN_SHA256_HEX_LEN + 1])
{
    if (!list || !canon_path || !sha_out) {
        return -1;
    }
    for (size_t i = 0; i < list->n; i++) {
        if (strcmp(list->entries[i].path, canon_path) == 0) {
            memcpy(sha_out, list->entries[i].sha256_hex,
                   PETRUSH_PLUGIN_SHA256_HEX_LEN + 1);
            return 0;
        }
    }
    return -1;
}

static int add_search_dir(char dirs[][PETRUSH_PLG_PATH_MAX],
                          size_t max_dirs,
                          size_t *n,
                          const char *raw)
{
    if (!raw || !raw[0] || !n) {
        return 0;
    }
    if (raw[0] != '/') {
        return 0; /* relativo: ignora */
    }
    if (*n >= max_dirs) {
        return -1;
    }
    char canon[PETRUSH_PLG_PATH_MAX];
    if (realpath(raw, canon) != NULL) {
        if (snprintf(dirs[*n], PETRUSH_PLG_PATH_MAX, "%s", canon) >=
            PETRUSH_PLG_PATH_MAX) {
            return -1;
        }
    } else {
        /* dir ainda nao existe: guarda o absoluto literal (cap) */
        if (snprintf(dirs[*n], PETRUSH_PLG_PATH_MAX, "%s", raw) >=
            PETRUSH_PLG_PATH_MAX) {
            return -1;
        }
    }
    (*n)++;
    return 0;
}

int petrush_plugin_search_dirs(char dirs[][PETRUSH_PLG_PATH_MAX],
                               size_t max_dirs,
                               size_t *out_n)
{
    if (!dirs || !out_n || max_dirs == 0) {
        return -1;
    }
    *out_n = 0;

    const char *env = getenv("PETRUSH_PLUGIN_PATH");
    if (env && env[0]) {
        char *copy = strdup(env);
        if (!copy) {
            return -1;
        }
        char *save = NULL;
        for (char *tok = strtok_r(copy, ":", &save); tok != NULL;
             tok = strtok_r(NULL, ":", &save)) {
            if (add_search_dir(dirs, max_dirs, out_n, tok) != 0) {
                free(copy);
                return -1;
            }
        }
        free(copy);
    }

    char xdg[PETRUSH_PLG_PATH_MAX];
    const char *data = getenv("XDG_DATA_HOME");
    if (data && data[0] == '/') {
        if (snprintf(xdg, sizeof(xdg), "%s/petrush/plugins", data) >=
            (int)sizeof(xdg)) {
            return -1;
        }
    } else {
        const char *home = getenv("HOME");
        if (!home || home[0] != '/') {
            /* sem HOME: so PETRUSH_PLUGIN_PATH */
            return 0;
        }
        if (snprintf(xdg, sizeof(xdg), "%s/.local/share/petrush/plugins",
                     home) >= (int)sizeof(xdg)) {
            return -1;
        }
    }
    if (add_search_dir(dirs, max_dirs, out_n, xdg) != 0) {
        return -1;
    }
    return 0;
}

static int name_is_safe_basename(const char *name)
{
    if (!name || !name[0]) {
        return 0;
    }
    if (strchr(name, '/') != NULL || strchr(name, '\\') != NULL) {
        return 0;
    }
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return 0;
    }
    /* rejeita path traversal embutido */
    if (strstr(name, "..") != NULL) {
        return 0;
    }
    return 1;
}

int petrush_plugin_resolve(const char *name, char out_canon[PETRUSH_PLG_PATH_MAX])
{
    if (!name || !out_canon || !name_is_safe_basename(name)) {
        return -1;
    }

    char dirs[PETRUSH_PLUGIN_SEARCH_MAX][PETRUSH_PLG_PATH_MAX];
    size_t nd = 0;
    if (petrush_plugin_search_dirs(dirs, PETRUSH_PLUGIN_SEARCH_MAX, &nd) != 0) {
        return -1;
    }

    char candidate[PETRUSH_PLG_PATH_MAX];
    for (size_t i = 0; i < nd; i++) {
        int n;
        if (strlen(name) > 3 && strcmp(name + strlen(name) - 3, ".so") == 0) {
            n = snprintf(candidate, sizeof(candidate), "%s/%s", dirs[i], name);
        } else {
            n = snprintf(candidate, sizeof(candidate), "%s/%s.so", dirs[i], name);
        }
        if (n < 0 || n >= (int)sizeof(candidate)) {
            continue;
        }
        if (realpath(candidate, out_canon) != NULL) {
            return 0;
        }
    }
    return -1;
}

static int default_allow_path(char out[PETRUSH_PLG_PATH_MAX])
{
    const char *env = getenv("PETRUSH_PLUGIN_ALLOW");
    if (env && env[0] == '/') {
        if (snprintf(out, PETRUSH_PLG_PATH_MAX, "%s", env) >= PETRUSH_PLG_PATH_MAX) {
            return -1;
        }
        return 0;
    }
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0] == '/') {
        if (snprintf(out, PETRUSH_PLG_PATH_MAX, "%s/petrush/plugins.allow",
                     xdg) >= PETRUSH_PLG_PATH_MAX) {
            return -1;
        }
        return 0;
    }
    const char *home = getenv("HOME");
    if (!home || home[0] != '/') {
        return -1;
    }
    if (snprintf(out, PETRUSH_PLG_PATH_MAX, "%s/.config/petrush/plugins.allow",
                 home) >= PETRUSH_PLG_PATH_MAX) {
        return -1;
    }
    return 0;
}

static void log_deny(const char *reason, const char *path)
{
    /* log local minimo; sem segredos. stderr do shell unpriv. */
    if (path && path[0]) {
        fprintf(stderr, "petrush: plugin deny (%s): %s\n", reason, path);
    } else {
        fprintf(stderr, "petrush: plugin deny (%s)\n", reason);
    }
}

/*
 * ISO C forbids converting object pointer (void* de dlsym) para function
 * pointer (-Werror=pedantic). POSIX garante mesmo tamanho: copiar bits.
 */
static void petrush_dlsym_fn(void *handle, const char *symbol, void *fn_out)
{
    void *sym = dlsym(handle, symbol);
    memcpy(fn_out, &sym, sizeof(sym));
}

int petrush_plugin_load(const char *name,
                        const char *allow_path,
                        petrush_plugin_t *out)
{
    if (!name || !out) {
        return PETRUSH_PLG_ERR_ARG;
    }
    memset(out, 0, sizeof(*out));

    char canon[PETRUSH_PLG_PATH_MAX];
    if (petrush_plugin_resolve(name, canon) != 0) {
        log_deny("notfound", name);
        return PETRUSH_PLG_ERR_NOTFOUND;
    }

    if (petrush_plugin_path_writable_ok(canon) != 0) {
        log_deny("world-writable", canon);
        return PETRUSH_PLG_ERR_PERM;
    }

    char apath[PETRUSH_PLG_PATH_MAX];
    const char *ause = allow_path;
    if (!ause) {
        if (default_allow_path(apath) != 0) {
            log_deny("allow-missing", NULL);
            return PETRUSH_PLG_ERR_ALLOW;
        }
        ause = apath;
    }

    petrush_plugin_allow_t allow;
    if (petrush_plugin_allow_parse_file(ause, &allow) != 0) {
        log_deny("allow-unreadable", ause);
        return PETRUSH_PLG_ERR_ALLOW;
    }

    char expect_hex[PETRUSH_PLUGIN_SHA256_HEX_LEN + 1];
    if (petrush_plugin_allow_find(&allow, canon, expect_hex) != 0) {
        log_deny("not-allowlisted", canon);
        return PETRUSH_PLG_ERR_ALLOW;
    }

    int fd = open(canon, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        log_deny("open", canon);
        return PETRUSH_PLG_ERR_IO;
    }

    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        close(fd);
        log_deny("fstat", canon);
        return PETRUSH_PLG_ERR_IO;
    }
    if ((st.st_mode & S_IWOTH) != 0) {
        close(fd);
        log_deny("world-writable", canon);
        return PETRUSH_PLG_ERR_PERM;
    }
    if ((unsigned long long)st.st_size > (unsigned long long)PETRUSH_PLUGIN_MAX_BYTES) {
        close(fd);
        log_deny("too-large", canon);
        return PETRUSH_PLG_ERR_SIZE;
    }

    unsigned char dig[PETRUSH_PLUGIN_SHA256_LEN];
    if (petrush_plugin_sha256_fd(fd, dig) != 0) {
        close(fd);
        log_deny("sha256", canon);
        return PETRUSH_PLG_ERR_HASH;
    }
    close(fd);

    char got_hex[PETRUSH_PLUGIN_SHA256_HEX_LEN + 1];
    petrush_plugin_sha256_to_hex(dig, got_hex);
    if (petrush_plugin_sha256_hex_eq(got_hex, expect_hex) != 0) {
        log_deny("hash-mismatch", canon);
        return PETRUSH_PLG_ERR_HASH;
    }

    /*
     * TOCTOU residual (POSIX dlopen e por path): hasheamos o fd aberto;
     * dlopen relê o path. Mitigacao: checks ww nos dirs + hash pinning.
     * Documentado em plugins-threat.md §11.
     */
    void *handle = dlopen(canon, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        log_deny("dlopen", canon);
        return PETRUSH_PLG_ERR_DLOPEN;
    }

    typedef int (*query_fn)(petrush_plugin_info_t *);
    typedef int (*init_fn)(const petrush_plugin_host_t *);
    typedef int (*cmd_fn)(int, char **);
    typedef void (*fini_fn)(void);

    _Static_assert(sizeof(void *) == sizeof(query_fn), "dlsym fn size");
    _Static_assert(sizeof(void *) == sizeof(init_fn), "dlsym fn size");
    _Static_assert(sizeof(void *) == sizeof(cmd_fn), "dlsym fn size");
    _Static_assert(sizeof(void *) == sizeof(fini_fn), "dlsym fn size");

    query_fn query = NULL;
    init_fn init = NULL;
    cmd_fn cmd = NULL;
    fini_fn fini = NULL;
    petrush_dlsym_fn(handle, "petrush_plugin_query", &query);
    petrush_dlsym_fn(handle, "petrush_plugin_init", &init);
    petrush_dlsym_fn(handle, "petrush_plugin_cmd", &cmd);
    petrush_dlsym_fn(handle, "petrush_plugin_fini", &fini);

    if (!query || !init || !cmd || !fini) {
        /* tenta vtable (object ptr -> object ptr: cast ISO C ok) */
        union {
            void *obj;
            petrush_plugin_abi_t *vt;
        } u_abi;
        u_abi.obj = dlsym(handle, "petrush_plugin_abi");
        petrush_plugin_abi_t *vt = u_abi.vt;
        if (!vt || !vt->query || !vt->init || !vt->cmd || !vt->fini) {
            dlclose(handle);
            log_deny("dlsym", canon);
            return PETRUSH_PLG_ERR_DLOPEN;
        }
        query = vt->query;
        init = vt->init;
        cmd = vt->cmd;
        fini = vt->fini;
    }

    petrush_plugin_info_t info;
    memset(&info, 0, sizeof(info));
    if (query(&info) != PETRUSH_PLUGIN_OK) {
        dlclose(handle);
        log_deny("query", canon);
        return PETRUSH_PLG_ERR_ABI;
    }
    if (info.abi_major != PETRUSH_PLUGIN_ABI_MAJOR) {
        dlclose(handle);
        log_deny("abi-major", canon);
        return PETRUSH_PLG_ERR_ABI;
    }
    if (info.abi_minor > PETRUSH_PLUGIN_ABI_MINOR) {
        /* minor do plugin > host: extensao desconhecida => recusa */
        dlclose(handle);
        log_deny("abi-minor", canon);
        return PETRUSH_PLG_ERR_ABI;
    }

    petrush_plugin_host_t host;
    host.abi_major = PETRUSH_PLUGIN_ABI_MAJOR;
    host.abi_minor = PETRUSH_PLUGIN_ABI_MINOR;
    host.opaque = NULL;

    if (init(&host) != PETRUSH_PLUGIN_OK) {
        if (fini) {
            fini();
        }
        dlclose(handle);
        log_deny("init", canon);
        return PETRUSH_PLG_ERR_ABI;
    }

    if (snprintf(out->path, sizeof(out->path), "%s", canon) >= (int)sizeof(out->path)) {
        fini();
        dlclose(handle);
        return PETRUSH_PLG_ERR_IO;
    }
    memcpy(out->sha256_hex, got_hex, sizeof(out->sha256_hex));
    if (info.name) {
        (void)snprintf(out->plug_name, sizeof(out->plug_name), "%s", info.name);
    }
    if (info.version) {
        (void)snprintf(out->version, sizeof(out->version), "%s", info.version);
    }
    out->abi_major = info.abi_major;
    out->abi_minor = info.abi_minor;
    out->handle = handle;
    out->cmd = cmd;
    out->fini = fini;
    out->initialized = 1;

    fprintf(stderr, "petrush: plugin allow: %s sha256=%.16s... abi=%u.%u name=%s\n",
            canon, got_hex, out->abi_major, out->abi_minor,
            out->plug_name[0] ? out->plug_name : "?");
    return PETRUSH_PLG_OK;
}

int petrush_plugin_unload(petrush_plugin_t *p)
{
    if (!p) {
        return -1;
    }
    if (p->initialized && p->fini) {
        p->fini();
        p->initialized = 0;
    }
    if (p->handle) {
        dlclose(p->handle);
        p->handle = NULL;
    }
    p->cmd = NULL;
    p->fini = NULL;
    return 0;
}
