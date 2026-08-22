/*
 * libFuzzer harness — prompt_render (TST-T3)
 * Build: see tests/fuzz/README.md
 */
#include "petrush/prompt.h"

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

    char out[4096];
    prompt_render(buf, out, sizeof(out));

    /* also exercise tiny and empty buffers */
    char tiny[2];
    prompt_render(buf, tiny, sizeof(tiny));
    prompt_render(buf, out, 1);

    free(buf);
    return 0;
}
