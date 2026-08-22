/*
 * libFuzzer harness — petrush_parse_list (TST-T3)
 * Build: see tests/fuzz/README.md
 */
#include "petrush/parser.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char *buf = malloc(size + 1);
    if (!buf) return 0;
    memcpy(buf, data, size);
    buf[size] = '\0';

    petrush_list_t list;
    if (petrush_parse_list(buf, &list) == 0)
        petrush_list_free(&list);

    free(buf);
    return 0;
}
