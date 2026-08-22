#!/usr/bin/env bash
# TST-T3 fallback: sem libFuzzer — driver ASan alimentado por bytes aleatórios.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${FUZZ_OUT:-/var/tmp/petrush-fuzz-fallback}"
ITERS="${FUZZ_ITERS:-20000}"
mkdir -p "$OUT"
cd "$ROOT"

cat >"$OUT/driver.c" <<'EOF'
#include "petrush/parser.h"
#include "petrush/expand.h"
#include "petrush/prompt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static uint32_t rng = 0xC0FFEEu;
static uint32_t rnd(void) { rng = rng * 1664525u + 1013904223u; return rng; }

int main(int argc, char **argv)
{
    int iters = argc > 1 ? atoi(argv[1]) : 20000;
    char buf[512];
    char pout[1024];
    for (int i = 0; i < iters; i++) {
        size_t n = (size_t)(rnd() % sizeof(buf));
        for (size_t j = 0; j < n; j++) buf[j] = (char)(rnd() & 0xFF);
        buf[n < sizeof(buf) ? n : sizeof(buf) - 1] = '\0';

        petrush_list_t list;
        if (petrush_parse_list(buf, &list) == 0)
            petrush_list_free(&list);

        char *e = expand_word(buf);
        free(e);

        prompt_render(buf, pout, sizeof(pout));
    }
    printf("fallback_asan_loop iters=%d ok\n", iters);
    return 0;
}
EOF

clang -std=gnu2x -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined \
  -I"$ROOT/include" -I"$ROOT/src" \
  -o "$OUT/driver" "$OUT/driver.c" \
  src/mid/parser.c src/mid/expand.c src/mid/prompt.c src/foundation/env.c

ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  "$OUT/driver" "$ITERS" | tee "$OUT/run.log"
