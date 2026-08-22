/*
 * child_argv.c - Montagem fail-closed do argv do filho (SEC-04)
 *
 * Se (pudod_argc - 2) > PUDOD_MAX_ARGS, recusa. Nunca trunca em silencio.
 */

#include "child_argv.h"

#include <stddef.h>

int pudod_build_child_argv(char *const pudod_argv[], int pudod_argc,
                           const char *resolved,
                           char *out[], size_t out_cap)
{
    if (!pudod_argv || !resolved || !out || out_cap < 2) {
        return -1;
    }
    if (pudod_argc < 2) {
        return -1;
    }

    int user_args = pudod_argc - 2;
    if (user_args > PUDOD_MAX_ARGS) {
        return -1;
    }

    /* resolved + user_args + NULL */
    size_t need = (size_t)user_args + 2;
    if (out_cap < need) {
        return -1;
    }

    out[0] = (char *)resolved;
    for (int i = 0; i < user_args; i++) {
        if (pudod_argv[i + 2] == NULL) {
            return -1;
        }
        out[i + 1] = pudod_argv[i + 2];
    }
    out[user_args + 1] = NULL;
    return 0;
}
