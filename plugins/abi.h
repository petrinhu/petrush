/*
 * abi.h - Contrato C11 da ABI de plugins .so de terceiros (PLG-ABI).
 *
 * Esta fatia so define o header. Loader (dlopen/dlsym) = PLG-LOAD.
 * O main do petrush NAO chama dlopen aqui. Sem 4755. Nao reabre UX-25
 * (OMZ-style skip permanece; isto e ABI de .so versionado).
 *
 * Compatibilidade: major do plugin == PETRUSH_PLUGIN_ABI_MAJOR do host;
 * minor do plugin <= PETRUSH_PLUGIN_ABI_MINOR do host.
 *
 * Cada .so exporta os quatro simbolos abaixo (ou a vtable petrush_plugin_abi
 * com os mesmos ponteiros) para o loader futuro resolver via dlsym.
 */

#ifndef PETRUSH_PLUGIN_ABI_H
#define PETRUSH_PLUGIN_ABI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Versao da ABI do host. Bump major = quebra; bump minor = extensao. */
#define PETRUSH_PLUGIN_ABI_MAJOR 1
#define PETRUSH_PLUGIN_ABI_MINOR 0

/* Codigos de retorno tipicos das entry points. */
#define PETRUSH_PLUGIN_OK         0
#define PETRUSH_PLUGIN_ERR_ABI   (-1) /* major incompativel / ABI recusada */
#define PETRUSH_PLUGIN_ERR_ARG   (-2) /* argumento NULL / argc invalido */
#define PETRUSH_PLUGIN_ERR_STATE (-3) /* init duplicado / cmd sem init */
#define PETRUSH_PLUGIN_ERR_IO    (-4) /* falha de I/O interna do plugin */

/*
 * Metadados preenchidos por query (antes ou depois de init).
 * name/version: NUL-terminated; owned pelo plugin (estaveis ate fini).
 */
typedef struct petrush_plugin_info {
    uint32_t abi_major;
    uint32_t abi_minor;
    const char *name;
    const char *version;
} petrush_plugin_info_t;

/*
 * Contexto do host passado em init.
 * opaque reservado ao loader (PLG-LOAD); plugins tratam como read-only.
 */
typedef struct petrush_plugin_host {
    uint32_t abi_major;
    uint32_t abi_minor;
    void *opaque;
} petrush_plugin_host_t;

/*
 * Entry points exportadas por cada .so (nomes estaveis para dlsym).
 *
 * query: preenche *out com major/minor/name/version. out nao-NULL.
 *        Pode ser chamado antes de init. Retorna PETRUSH_PLUGIN_OK ou ERR_*.
 * init:  arranque; host nao-NULL e host->abi_major == MAJOR.
 *        Retorna OK ou ERR_ABI/ERR_ARG/ERR_STATE.
 * cmd:   despacho de comando (argc/argv estilo main; argc>=0).
 *        Requer init previo bem-sucedido. Retorna OK ou ERR_*.
 * fini:  teardown. Seguro se nunca houve init. Sem retorno.
 */
int petrush_plugin_query(petrush_plugin_info_t *out);
int petrush_plugin_init(const petrush_plugin_host_t *host);
int petrush_plugin_cmd(int argc, char **argv);
void petrush_plugin_fini(void);

/*
 * Vtable opcional: plugin pode exportar um simbolo
 * `petrush_plugin_abi` deste tipo em vez de (ou alem de) os 4 simbolos.
 * major/minor na vtable devem espelhar PETRUSH_PLUGIN_ABI_* do plugin.
 */
typedef struct petrush_plugin_abi {
    uint32_t major;
    uint32_t minor;
    int (*query)(petrush_plugin_info_t *out);
    int (*init)(const petrush_plugin_host_t *host);
    int (*cmd)(int argc, char **argv);
    void (*fini)(void);
} petrush_plugin_abi_t;

#ifdef __cplusplus
}
#endif

#endif /* PETRUSH_PLUGIN_ABI_H */
