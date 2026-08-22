# TST-T3 Fuzzing de inputs (parser / expand / prompt)

**Data:** 2026-08-22T20:11:46-03:00  
**SHA HEAD (pré-commit do relatório):** `fd960af07beb595425b97aff97545343e46e9fd0`  
**Agent:** qa-engineer  
**Item:** TST-T3 (W14)  
**Veredicto:** **PASS** (`crashes=0`; zero ASan/UBSan ERROR nos 3 alvos)

## Escopo

Fuzzing de input não-confiável conforme `TESTES.md` (projeto) e vault T3: parsing de linha, expansão de palavra, render de PS1.

**In:**
- `petrush_parse_list` (`src/mid/parser.c`)
- `expand_word` (`src/mid/expand.c` + `src/foundation/env.c`)
- `prompt_render` (`src/mid/prompt.c` + `src/foundation/env.c`)
- libFuzzer + ASan + UBSan, 60 s por alvo
- corpus semente em `tests/fuzz/corpus/{parser,expand,prompt}/`

**Out:** AFL++ (ausente; libFuzzer basta); fuzz de `glob_word` / dispatcher / REPL e2e; push remoto; mutation testing.

## Ambiente

| Peça | Valor |
|---|---|
| Host | Linux Fedora 44 local; `TMPDIR=/var/tmp` |
| clang | 22.1.8 (`-fsanitize=fuzzer,address,undefined` OK) |
| AFL++ | não instalado (não bloqueante) |
| Dialect | `-std=gnu2x` (igual CMake; necessário p/ `setenv`/`PATH_MAX`) |
| ASAN_OPTIONS | `detect_leaks=1:halt_on_error=1:abort_on_error=1` |
| UBSAN_OPTIONS | `halt_on_error=1:print_stacktrace=1` |
| Artefatos | `/var/tmp/petrush-fuzz/` (binários, logs, corpus_work) |

## Harness (reproduzível)

Fontes versionadas:

| Arquivo | Alvo |
|---|---|
| `tests/fuzz/fuzz_parser.c` | `LLVMFuzzerTestOneInput` → `petrush_parse_list` + `petrush_list_free` |
| `tests/fuzz/fuzz_expand.c` | → `expand_word` + `free` |
| `tests/fuzz/fuzz_prompt.c` | → `prompt_render` (buf 4096 / 2 / 1) |
| `tests/fuzz/build_and_run.sh` | build + run 30–90 s |
| `tests/fuzz/fallback_asan_loop.sh` | fallback sem libFuzzer (loop RNG + ASan) |
| `tests/fuzz/README.md` | instruções |

```bash
FUZZ_SECS=60 bash tests/fuzz/build_and_run.sh
```

## Execução (esta fatia)

```text
[TST-T3] fuzzing parser for 60s ... exit=0
[TST-T3] fuzzing expand for 60s ... exit=0
[TST-T3] fuzzing prompt for 60s ... exit=0
[TST-T3] summary FAIL=0
[TST-T3] crash_artifacts=0
```

| Alvo | Seed | INITED cov/ft | DONE cov/ft / corp | Runs (~61 s) | exec/s | Exit | crash-* |
|---|---|---|---|---|---|---|---|
| parser | 24 | 381 / 863 | 455 / 3104 / 457 | 1 199 596 | 19 665 | 0 | 0 |
| expand | 16 | 88 / 153 | 104 / 794 / 242 | 2 863 347 | 46 940 | 0 | 0 |
| prompt | 12 | 25 / 52 | 27 / 156 / 71 | 23 940 181 | 392 461 | 0 | 0 |

Logs: `/var/tmp/petrush-fuzz/{parser,expand,prompt}.log`  
Busca ASan/UBSan nos 3 logs: **0** matches (`AddressSanitizer` / `UndefinedBehavior` / `ERROR:`).  
`find artifacts -name 'crash-*|leak-*|timeout-*|oom-*'`: **0**.

## Achados

**crashes=0.** Nenhum bug novo com repro nesta janela de 60 s × 3.

## Limitações (honesto)

- Janela curta (60 s): boa smoke de fuzz, não campanha longa.
- Input sempre NUL-terminated (contrato das APIs `const char *`); bytes internos com `\0` truncam a string efetivamente.
- `expand_word` não exercita `glob_word` / `expand_cmd_argv` nesta fatia.
- Corpus inicial pequeno; libFuzzer cresceu o corpus em `corpus_work/` (scratch, não versionado).

## Critério de saída TST-T3

- [x] Harness libFuzzer mínimo em `tests/fuzz/` (parser + expand + prompt)
- [x] Run curto (30–90 s) por alvo com ASan
- [x] Relatório com `crashes=0` ou repro
- [x] TODO → 🔍 (zero crash novo)
- [x] Sem push

## Próximo

Onda de verificação formal (TST) pode carimbar `✅` após re-run opcional mais longo ou inclusão de `glob_word` se a campanha pedir.
