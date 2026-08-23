/*
 * Plugin de teste com ABI major=2 (host=1) — deve falhar no loader (PLG-LOAD).
 */

#include "abi.h"

int petrush_plugin_query(petrush_plugin_info_t *out)
{
    if (!out) {
        return PETRUSH_PLUGIN_ERR_ARG;
    }
    out->abi_major = 2;
    out->abi_minor = 0;
    out->name = "bad_major";
    out->version = "0.0.1";
    return PETRUSH_PLUGIN_OK;
}

int petrush_plugin_init(const petrush_plugin_host_t *host)
{
    (void)host;
    return PETRUSH_PLUGIN_OK;
}

int petrush_plugin_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return PETRUSH_PLUGIN_OK;
}

void petrush_plugin_fini(void)
{
}
