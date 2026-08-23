/*
 * plugin_load.h - Loader Foundation de plugins .so (PLG-LOAD)
 *
 * XDG + PETRUSH_PLUGIN_PATH, allow-list path+SHA-256, recusa world-writable,
 * dlopen so apos checks. Consome docs/security/plugins-threat.md.
 * pudod NAO usa este header. Sem 4755.
 */

#ifndef PETRUSH_PLUGIN_LOAD_H
#define PETRUSH_PLUGIN_LOAD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PETRUSH_PLUGIN_SHA256_LEN     32
#define PETRUSH_PLUGIN_SHA256_HEX_LEN 64
#define PETRUSH_PLUGIN_ALLOW_MAX      64
#define PETRUSH_PLUGIN_SEARCH_MAX     32
#define PETRUSH_PLUGIN_MAX_BYTES      (16u * 1024u * 1024u)
#define PETRUSH_PLG_PATH_MAX          4096

#define PETRUSH_PLG_OK            0
#define PETRUSH_PLG_ERR_ARG    (-1)
#define PETRUSH_PLG_ERR_NOTFOUND (-2)
#define PETRUSH_PLG_ERR_PERM   (-3) /* world-writable / mode inseguro */
#define PETRUSH_PLG_ERR_ALLOW  (-4) /* fora da allow-list / lista ausente */
#define PETRUSH_PLG_ERR_HASH   (-5)
#define PETRUSH_PLG_ERR_ABI    (-6)
#define PETRUSH_PLG_ERR_DLOPEN (-7)
#define PETRUSH_PLG_ERR_IO     (-8)
#define PETRUSH_PLG_ERR_SIZE   (-9)

/*
 * Canon path: ficheiro regular sem S_IWOTH; cada dir prefixo sem S_IWOTH;
 * dirs com S_IWGRP so ok se sticky (S_ISVTX). Erro de stat => -1.
 * Retorna 0 se seguro, -1 se inseguro ou erro (fail-closed).
 */
int petrush_plugin_path_writable_ok(const char *canon_path);

/* SHA-256 do conteudo a partir do offset 0 do fd. out = 32 bytes. */
int petrush_plugin_sha256_fd(int fd, unsigned char out[PETRUSH_PLUGIN_SHA256_LEN]);

void petrush_plugin_sha256_to_hex(const unsigned char dig[PETRUSH_PLUGIN_SHA256_LEN],
                                  char out[PETRUSH_PLUGIN_SHA256_HEX_LEN + 1]);

/*
 * Compara dois digests hex64 em tempo constante (memeq_ct / CRYPTO_memcmp).
 * 0 = iguais, 1 = diferem, -1 = input invalido.
 */
int petrush_plugin_sha256_hex_eq(const char *a, const char *b);

typedef struct petrush_plugin_allow_entry {
    char path[PETRUSH_PLG_PATH_MAX];
    char sha256_hex[PETRUSH_PLUGIN_SHA256_HEX_LEN + 1];
} petrush_plugin_allow_entry_t;

typedef struct petrush_plugin_allow {
    petrush_plugin_allow_entry_t entries[PETRUSH_PLUGIN_ALLOW_MAX];
    size_t n;
} petrush_plugin_allow_t;

/*
 * Parse allow-list (path absoluto + sha256 hex64). Default deny se path NULL
 * ou ficheiro ausente. Basename sozinho / glob = rejeitado. Lista o+w = -1.
 */
int petrush_plugin_allow_parse_file(const char *path, petrush_plugin_allow_t *out);

int petrush_plugin_allow_find(const petrush_plugin_allow_t *list,
                              const char *canon_path,
                              char sha_out[PETRUSH_PLUGIN_SHA256_HEX_LEN + 1]);

/*
 * Dirs de busca: PETRUSH_PLUGIN_PATH (so absolutos) + XDG data
 * ($XDG_DATA_HOME/petrush/plugins ou ~/.local/share/petrush/plugins).
 * Elemento relativo => ignorado (nao entra na lista). Cap SEARCH_MAX.
 */
int petrush_plugin_search_dirs(char dirs[][PETRUSH_PLG_PATH_MAX],
                               size_t max_dirs,
                               size_t *out_n);

/*
 * Resolve basename (sem '/') sob search dirs via realpath.
 * Retorna 0 e grava path canonico; -1 se nao achar / invalido.
 */
int petrush_plugin_resolve(const char *name, char out_canon[PETRUSH_PLG_PATH_MAX]);

typedef struct petrush_plugin {
    void *handle;
    char path[PETRUSH_PLG_PATH_MAX];
    char sha256_hex[PETRUSH_PLUGIN_SHA256_HEX_LEN + 1];
    char plug_name[128];
    char version[64];
    uint32_t abi_major;
    uint32_t abi_minor;
    int (*cmd)(int argc, char **argv);
    void (*fini)(void);
    int initialized;
} petrush_plugin_t;

/*
 * resolve -> ww -> allow -> SHA-256(fd) -> dlopen -> query -> major check -> init.
 * allow_path NULL => $PETRUSH_PLUGIN_ALLOW ou XDG config plugins.allow.
 * Nao eleva; nao fala com pudod.
 */
int petrush_plugin_load(const char *name,
                        const char *allow_path,
                        petrush_plugin_t *out);

int petrush_plugin_unload(petrush_plugin_t *p);

#ifdef __cplusplus
}
#endif

#endif /* PETRUSH_PLUGIN_LOAD_H */
