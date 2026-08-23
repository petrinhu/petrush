/*
 * Plugin de teste ABI major=1 (PLG-LOAD).
 * Compilado como MODULE (.so); nao e produto.
 */

#include "abi.h"

#include <string.h>

static int g_inited;

int petrush_plugin_query(petrush_plugin_info_t *out)
{
    if (!out) {
        return PETRUSH_PLUGIN_ERR_ARG;
    }
    out->abi_major = PETRUSH_PLUGIN_ABI_MAJOR;
    out->abi_minor = PETRUSH_PLUGIN_ABI_MINOR;
    out->name = "ok_plugin";
    out->version = "0.0.1";
    return PETRUSH_PLUGIN_OK;
}

int petrush_plugin_init(const petrush_plugin_host_t *host)
{
    if (!host) {
        return PETRUSH_PLUGIN_ERR_ARG;
    }
    if (host->abi_major != PETRUSH_PLUGIN_ABI_MAJOR) {
        return PETRUSH_PLUGIN_ERR_ABI;
    }
    if (g_inited) {
        return PETRUSH_PLUGIN_ERR_STATE;
    }
    g_inited = 1;
    return PETRUSH_PLUGIN_OK;
}

int petrush_plugin_cmd(int argc, char **argv)
{
    (void)argv;
    if (!g_inited) {
        return PETRUSH_PLUGIN_ERR_STATE;
    }
    if (argc < 0) {
        return PETRUSH_PLUGIN_ERR_ARG;
    }
    return PETRUSH_PLUGIN_OK;
}

void petrush_plugin_fini(void)
{
    g_inited = 0;
}
