# TST-T3 — Fuzz harnesses (parser / expand / prompt)

Harnesses libFuzzer + ASan/UBSan sobre APIs Mid:

| Binário | API | Fontes |
|---|---|---|
| `fuzz_parser` | `petrush_parse_list` | `src/mid/parser.c` |
| `fuzz_expand` | `expand_word` | `src/mid/expand.c` + `src/foundation/env.c` |
| `fuzz_prompt` | `prompt_render` | `src/mid/prompt.c` + `src/foundation/env.c` |

## Pré-requisito

```bash
clang -fsanitize=fuzzer,address -O1 -o /tmp/fcheck -x c - <<'EOF'
int LLVMFuzzerTestOneInput(const unsigned char *d, unsigned long n){(void)d;(void)n;return 0;}
EOF
```

AFL++ não é obrigatório se libFuzzer existir.

## Rodar (reproduzível)

```bash
# 60s por alvo (default); artefatos em /var/tmp/petrush-fuzz
FUZZ_SECS=60 bash tests/fuzz/build_and_run.sh
```

Corpus semente: `tests/fuzz/corpus/{parser,expand,prompt}/`.

## Sem libFuzzer

Fallback ASan-only (loop curto de bytes aleatórios):

```bash
bash tests/fuzz/fallback_asan_loop.sh
```

## Critério TST-T3

- Zero `crash-*` / `leak-*` / ASan ERROR nos logs → PASS (item → 🔍).
- Crash → gravar repro em `docs/memory/tst-t3-fuzz.md` e abrir bugfix.
