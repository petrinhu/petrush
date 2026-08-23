# TST-CXX - configsh PTY (`ctest -R configsh`)

**Data:** 2026-08-23T04:39:51-03:00  
**SHA HEAD (pré-commit do relatório):** `2e8e03df39eff6175bf3ec6547c5fcddb67b64bc`  
**Agent:** qa-engineer  
**Item:** TST-CXX (W26)  
**Veredicto:** **PASS**

## Escopo

Gate de regressão do binário `configsh` (CXX-TUI): dump/check/section/XDG + TUI raw em PTY, filtrável por `ctest -R configsh` (necessário também para GATE-CXXASM).

**In:**
- `add_test(NAME configsh …)` → `tests/smoke/cxx-tui.sh`
- `ctest -R configsh` no host
- smoke + `ctest -R configsh` em Docker `fedora:44` com `-t` (PTY; python3 fallback)
- prova de ausência de ncurses e mode ≠ 4755

**Out:** setuid/`pudod`, display `:0` / sessão gráfica viva, push, tag, GATE-CXXASM completo.

## Ambiente

| Peça | Valor |
|---|---|
| Host | Linux local; `build/` reconfigurado Release; clang 22 |
| Container | `fedora:44` (`f2d7418fa4ad`); `docker run --rm -t`; `DISPLAY`/`WAYLAND_DISPLAY` unset |
| Toolchain container | clang/clang++ 22.1.8, cmake 4.3.0, python3 (sem `script`; PTY via `pty.fork`) |
| Smoke | `tests/smoke/cxx-tui.sh` (TMPDIR `/var/tmp`) |
| Labels ctest | `cxx`, `configsh`, `pty` (TIMEOUT 300) |

## Execução

### Host

```text
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
ctest --test-dir build -N -R configsh   # Test #31: configsh
ctest --test-dir build -R configsh --output-on-failure
# 1/1 Test #31: configsh  Passed  1.48 sec
# 100% tests passed, 0 tests failed out of 1
```

Log: `/var/tmp/petrush-tst-cxx-host-ctest.log` + `build/Testing/Temporary/LastTest.log`.

### Docker fedora:44 (PTY)

```text
docker run --rm -t -v "$PWD":/src:ro -e TMPDIR=/var/tmp \
  -e CMAKE_BUILD_PARALLEL_LEVEL=2 -w /src fedora:44 bash -lc '…'
# dnf: clang cmake make gcc-c++ binutils python3 openssl-devel gettext
# bash tests/smoke/cxx-tui.sh     → CXX-TUI PASS (PTY quit com q)
# cmake -B /var/tmp/… + ctest -R configsh → Passed 1.37 sec
# DISPLAY=unset WAYLAND_DISPLAY=unset
# === TST-CXX DOCKER PASS ===
```

Log: `/var/tmp/petrush-tst-cxx-docker.log`.

## Contagem

| Suite | Comando | Passed | Failed | Tempo | EXIT |
|---|---|---:|---:|---:|---:|
| host ctest | `ctest -R configsh` | 1 | 0 | 1.48 s | 0 |
| docker smoke | `tests/smoke/cxx-tui.sh` | ALL | 0 | ~1 s (pós dnf) | 0 |
| docker ctest | `ctest -R configsh` (build fresco) | 1 | 0 | 1.37 s | 0 |

## Cobertura exercitada (smoke)

- ldd: sem ncurses/libtinfo; mode `755` (não 4755)
- `--dump` XDG (`[prompt]`/`[aliases]`/`[env]`/`[history]`)
- `--section prompt --dump` (não vaza outras seções)
- `--check` good (0) e malformed (≠0)
- `--help` lista `--dump`/`--check`/`--section`
- defaults quando INI ausente
- PTY: TUI raw + quit `q` (python `pty`, sem display `:0`)
- compile_commands: `-fno-exceptions` e `-fno-rtti` nos `.cpp` de `src/cxx/`

## Achados

Nenhum funcional. Antes desta fatia `ctest -R configsh` devolvia **0 testes** (só existia o custom target `cxx_tui`). Wiring: `add_test(NAME configsh COMMAND bash …/cxx-tui.sh)` em `CMakeLists.txt`.

## Limitações (honesto)

1. O teste `configsh` reconstrói o alvo numa workdir isolada (custo ~1-2 s aqui; TIMEOUT 300 para máquina fria).
2. No container mínimo não há `util-linux`/`script`; PTY usa fallback python3 (já no smoke).
3. Não cobre ASan/UBSan do `configsh`, mutation, nem GATE-CXXASM (`asm_`+`configsh`+`plugin_` juntos).
4. Não substitui CI remoto.

## Critério de saída TST-CXX

- [x] `ctest -R configsh` encontra e passa (≥1 teste)
- [x] PTY smoke verde no host
- [x] PTY smoke + ctest verdes em Docker `fedora:44` (`-t`, sem `:0`)
- [x] mode ≠ 4755; sem ncurses
- [x] Relatório em `docs/memory/tst-cxx.md`
- [ ] ✅ na tabela só após onda de auditoria / gate (impl → 🔍)

**Status sugerido no TODO:** `🔍 Pendente verificação` (execução entregue; ✅ só pós auditoria/TST da onda).

## Referências

- Smoke: `tests/smoke/cxx-tui.sh`
- Target CMake: `cxx_tui` + `add_test(NAME configsh …)`
- Pré-req: CXX-TUI (`c8e31ae`)
- Manuais: vault `TESTES.md` (T1/T14) · `CLAUDE.md` TDD · ADR-001
