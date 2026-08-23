/*
 * PLG-ABI: TU de compilacao C11 (so -c). Prova plugins/abi.h + major=1.
 * Nao linka loader/dlopen (fatia PLG-LOAD). Nao toca UX-25.
 */
#include "abi.h"

#include <stddef.h>
#include <stdint.h>

#if PETRUSH_PLUGIN_ABI_MAJOR != 1
#error "PLG-ABI: PETRUSH_PLUGIN_ABI_MAJOR deve ser 1"
#endif

int main(void)
{
    int (*f_query)(petrush_plugin_info_t *) = petrush_plugin_query;
    int (*f_init)(const petrush_plugin_host_t *) = petrush_plugin_init;
    int (*f_cmd)(int, char **) = petrush_plugin_cmd;
    void (*f_fini)(void) = petrush_plugin_fini;

    petrush_plugin_info_t info;
    petrush_plugin_host_t host;
    petrush_plugin_abi_t abi;

    (void)f_query;
    (void)f_init;
    (void)f_cmd;
    (void)f_fini;
    (void)info;
    (void)host;
    (void)abi;
    (void)PETRUSH_PLUGIN_ABI_MINOR;
    (void)PETRUSH_PLUGIN_OK;
    (void)PETRUSH_PLUGIN_ERR_ABI;
    return (int)PETRUSH_PLUGIN_ABI_MAJOR == 1 ? 0 : 1;
}
