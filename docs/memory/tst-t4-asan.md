# TST-T4 Análise dinâmica de memória (ASan + UBSan + valgrind)

**Data:** 2026-08-22T20:05:16-03:00  
**SHA HEAD (pré-commit do relatório):** `92de708dc2a861d34b093bb6fd02fa74d60a6093`  
**Agent:** qa-engineer  
**Item:** TST-T4 (W14)  
**Veredicto:** **PASS** (zero leak/UB na suíte ASan; valgrind sem definite/indirect leak nos caminhos além de T15)

## Escopo

Análise dinâmica de memória conforme vault `TESTES.md` § T4 e `docs/standards.md` (build Sanitize + valgrind).

**In:**
- Build `CMAKE_BUILD_TYPE=Sanitize` (ASan + UBSan) em `/var/tmp/petrush-asan`
- Target `check` (18 unitários) e `smoke` (53 casos) sob ASan/UBSan
- Valgrind em build Debug separado (`/var/tmp/petrush-valgrind`): 18 unitários + caminhos REPL do `petrush` + `pudod` deny

**Out:** setuid/`pudod` privilegiado; lint/clang-tidy (já TST-T2/T15); pré-CI container (TST-T15); fuzz (TST-T3); push remoto.

## Ambiente

| Peça | Valor |
|---|---|
| Host | Linux Fedora local; `TMPDIR=/var/tmp`; build `-j2` |
| Compilers | gcc 16.x (`/usr/bin/cc`) |
| Sanitize dir | `/var/tmp/petrush-asan` (`-DCMAKE_BUILD_TYPE=Sanitize`) |
| Valgrind dir | `/var/tmp/petrush-valgrind` (`-DCMAKE_BUILD_TYPE=Debug`) |
| valgrind | 3.27.1 |
| ASan libs | `libasan.so.8`, `libubsan.so.1` (confirmado via `ldd` em `petrush`) |
| ASAN_OPTIONS | `detect_leaks=1:halt_on_error=1:abort_on_error=1:detect_stack_use_after_return=1` |
| UBSAN_OPTIONS | `halt_on_error=1:print_stacktrace=1` |
| VGOPTS | `--leak-check=full --show-leak-kinds=definite,indirect --errors-for-leak-kinds=definite,indirect --error-exitcode=99 --quiet --track-origins=yes` |
| Disco | btrfs Device unallocated ≈ 32 GiB; Mem available ≈ 21 GiB |

## Execução

### 1. Configure + build Sanitize

```text
cmake -S . -B /var/tmp/petrush-asan -DCMAKE_BUILD_TYPE=Sanitize   # EXIT=0
cmake --build /var/tmp/petrush-asan -j2 --target petrush pudod test_*  # EXIT=0
```

Nota: `cmake --build` sem target dispara `verify` ALL (lint). clang-tidy ArrayBound em `pudo.c:539` (residual T15) falhou no lint; **não** faz parte do DoD de T4. Binários Sanitize foram montados por targets explícitos.

Prova ASan ligado: `ldd /var/tmp/petrush-asan/petrush` → `libasan.so.8` + `libubsan.so.1`.

### 2. check sob ASan/UBSan

```text
cmake --build /var/tmp/petrush-asan --target check
# 100% tests passed, 0 tests failed out of 18
# Total Test time (real) = 2.35 sec
# EXIT_CHECK=0
```

Log: `/var/tmp/petrush-asan-check.log` — sem `AddressSanitizer` / `UndefinedBehavior` / `ERROR:`.

### 3. smoke sob ASan/UBSan

```text
cmake --build /var/tmp/petrush-asan --target smoke
# SMOKE SUMMARY: Passed: 53  Failed: 0
# EXIT_SMOKE=0
```

Log: `/var/tmp/petrush-asan-smoke.log` — sem hits de sanitizer.

### 4. Valgrind (gaps vs T15)

T15 só rodou valgrind em `pudod` (deny allow-list). T4 cobre o resto em Debug **sem** sanitize (ASan∩valgrind incompatível).

| Alvo | Resultado |
|---|---|
| 18× `test_*` | **18/18 PASS** (`error-exitcode=99` nunca disparou) |
| `petrush` batch: echo/pwd, expand/pipe, lists, redirs, pushd/popd, alias, bg/jobs, parse_err | **8/8 PASS** |
| `pudod /usr/bin/true` | exit 1 (deny esperado); **sem** definite/indirect leak nem Invalid R/W |

Spot-check sem `--quiet`:

```text
test_parser: All heap blocks were freed -- no leaks are possible
             ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
petrush echo: definitely lost: 0 / indirectly lost: 0 / ERROR SUMMARY: 0
```

Logs: `/var/tmp/petrush-valgrind-run.log`, `/var/tmp/petrush-valgrind-petrush.log`.

## Contagem resumo

| Camada | Ferramenta | Resultado |
|---|---|---|
| Unit (`check`) | ASan+UBSan | 18/18 PASS, 0 UB, 0 leak halt |
| Smoke | ASan+UBSan | 53/53 PASS, 0 UB, 0 leak halt |
| Unit | valgrind | 18/18 PASS, ERROR SUMMARY 0 |
| REPL paths | valgrind | 8/8 PASS, definite/indirect 0 |
| pudod deny | valgrind | deny OK, sem leak/invalid (eco T15) |

## Critério de saída TST-T4

- [x] Build Sanitize em `/var/tmp` com ASan+UBSan ativos
- [x] `check` verde sob sanitizers
- [x] `smoke` verde sob sanitizers
- [x] Valgrind além de `pudod-valgrind` (unitários + caminhos `petrush`)
- [x] Zero leak/UB observável na suíte
- [x] Relatório em `docs/memory/tst-t4-asan.md`
- [x] Status `🔍` (impl/execução entregue; ✅ só pós onda TST/AUD)

## O que NÃO cobre

- Leak **still reachable** (allocator/libc) — fora do gate `definite,indirect`
- Race/TSAN, fuzz de parser (TST-T3)
- clang-tidy ArrayBound Release residual (`pudo.c:539`) — T15/T2
- `pudod` setuid real (proibido neste gate)

## Handoff

1. AUD-SEC pode consumir este relatório como evidência de memory safety dinâmica.
2. Residual clang-tidy ArrayBound segue com T15 (não bloqueia T4).
3. Sem push nesta fatia.

## Referências

- Item: `TODO.md` → TST-T4  
- Vault: `TESTES.md` § T4  
- Projeto: `docs/standards.md` (T4 memória)  
- Irmão: `docs/memory/tst-t15-preci.md` (valgrind só em pudod)
