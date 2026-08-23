# GATE-CXXASM - `ctest -R 'asm_|configsh|plugin_'` (Fedora 44 clang Sanitize+Release)

**Data:** 2026-08-23T04:53:35-03:00  
**SHA HEAD (pré-commit do relatório):** `d3d6dedaea45756cd5ecf1a9ce7b012259995140`  
**Agent:** qa-engineer  
**Item:** GATE-CXXASM (W29)  
**Veredicto:** **PASS**

## Escopo

Gate de regressão cruzado C23/C++23/ASM + plugins no fim da trilha CXXASM: todos os testes cujo nome casa com `asm_`, `configsh` ou `plugin_`, em Docker `fedora:44`, clang, nos build types **Sanitize** e **Release**.

**In:**
- `ctest -R 'asm_|configsh|plugin_'` (13 labels: 10 asm_ + 1 configsh + 2 plugin_)
- Docker `registry.fedoraproject.org/fedora:44` + clang 22
- `CMAKE_BUILD_TYPE=Sanitize` (ASan+UBSan em C/C++; ASM sem sanitize)
- `CMAKE_BUILD_TYPE=Release`
- prova mode `755` (sem 4755) nos binários do gate
- user unpriv (`builder` uid=1000, CapEff=0) + `PETRUSH_TEST_HOME` sem o+w

**Out:** setuid/`pudod` privilegiado; display `:0`; push; tag; lint/clang-tidy; matrix gcc/outras distros.

## Ambiente

| Peça | Valor |
|---|---|
| Imagem | `fedora:44` / `registry.fedoraproject.org/fedora:44` (`sha256:f2d7418fa4ad…`) |
| Compilers | clang/clang++ 22.1.8 (Fedora 22.1.8-4.fc44); ASM = Clang GAS |
| CMake | 4.3.0; `PETRUSH_ASM=ON`; `ENABLE_COVERAGE=OFF` |
| Build dirs | `/work/build-sanitize`, `/work/build-release` |
| Docker opts | `-t --network=host --security-opt seccomp=unconfined`; `DISPLAY`/`WAYLAND_DISPLAY` unset |
| User | `builder` uid=1000; `CapEff=0`; `HOME`/`PETRUSH_TEST_HOME=/home/builder` mode `755` |
| ASAN_OPTIONS | `detect_leaks=1:halt_on_error=1:abort_on_error=1:detect_stack_use_after_return=1` |
| UBSAN_OPTIONS | `halt_on_error=1:print_stacktrace=1` |
| Jobs | `-j2`; `TMPDIR=/var/tmp/petrush-gate-cxxasm-scratch` |
| Log | `/var/tmp/petrush-gate-cxxasm-docker.log` + `/var/tmp/petrush-gate-cxxasm-work/out/{Sanitize,Release}/` |

## Labels `ctest -R 'asm_|configsh|plugin_'`

| # | Nome | Tipo |
|---|---|---|
| 17 | `plugin_load` | unit/integration (13 subcasos acutest) |
| 18 | `plugin_pudod_no_dl` | smoke (`plugin-pudod-no-dl.sh`) |
| 21 | `asm_memeq` | unit |
| 22 | `asm_crc32` | unit |
| 23 | `asm_parse_i64` | unit |
| 24 | `asm_hash_path` | unit |
| 25 | `asm_job_setpgid` | unit |
| 26 | `asm_utf8` | unit |
| 27 | `asm_tty` | unit (PTY; sem display `:0`) |
| 28 | `asm_wai` | unit |
| 29 | `asm_netcom` | unit (CAP=0) |
| 30 | `asm_netcom_eperm` | smoke (`netcom-eperm.sh`; depende de `petrush`) |
| 31 | `configsh` | smoke (`cxx-tui.sh`; PTY; rebuild Release isolado) |

## Execução

### Pacotes no container

```text
dnf -y install clang compiler-rt libasan libubsan gcc gcc-c++ \
  cmake make binutils gettext glibc-devel openssl-devel \
  python3 diffutils findutils which util-linux
```

### Sanitize (clang)

```text
cmake -B /work/build-sanitize -S /src \
  -DCMAKE_BUILD_TYPE=Sanitize \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DENABLE_COVERAGE=OFF -DPETRUSH_ASM=ON
cmake --build /work/build-sanitize -j2 --target \
  asm_memeq asm_crc32 asm_parse_i64 asm_hash_path asm_job_setpgid \
  asm_utf8 asm_tty asm_wai asm_netcom \
  petrush pudod plugin_load plugin_test_ok plugin_test_bad_major configsh
# prova ASan/UBSan (clang runtime estatico; ldd nao lista libasan):
nm /work/build-sanitize/petrush | grep -c __asan_init   # -> 1
nm /work/build-sanitize/petrush | grep -c __ubsan_       # -> 52
ctest --test-dir /work/build-sanitize -N -R 'asm_|configsh|plugin_'
# Total Tests: 13
ctest --test-dir /work/build-sanitize -R 'asm_|configsh|plugin_' \
  --output-on-failure --timeout 300
# 100% tests passed, 0 tests failed out of 13
# Total Test time (real) = 2.48 sec
# CTEST_Sanitize_EXIT=0
```

### Release (clang)

```text
cmake -B /work/build-release -S /src \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DENABLE_COVERAGE=OFF -DPETRUSH_ASM=ON
cmake --build /work/build-release -j2 --target \
  asm_memeq asm_crc32 asm_parse_i64 asm_hash_path asm_job_setpgid \
  asm_utf8 asm_tty asm_wai asm_netcom \
  petrush pudod plugin_load plugin_test_ok plugin_test_bad_major configsh
ctest --test-dir /work/build-release -R 'asm_|configsh|plugin_' \
  --output-on-failure --timeout 300
# 100% tests passed, 0 tests failed out of 13
# Total Test time (real) = 1.65 sec
# CTEST_Release_EXIT=0
```

## Contagem

| Suite | Comando | Passed | Failed | Tempo | EXIT | Veredicto |
|---|---|---:|---:|---:|---:|---|
| Sanitize | `ctest -R 'asm_|configsh|plugin_'` | 13 | 0 | 2.48 s | 0 | **PASS** |
| Release | `ctest -R 'asm_|configsh|plugin_'` | 13 | 0 | 1.65 s | 0 | **PASS** |
| Overall | ambos | 26 | 0 | - | 0 | **PASS** |

Breakdown por familia (cada build type): `asm_` 10/10, `plugin_` 2/2, `configsh` 1/1.

## Hardening / mode

Todos os binários do gate (petrush, pudod, configsh, plugin_load, 9 asm unitários, 2 `.so` de teste) em Sanitize e Release: `mode=755`, `NO_SETUID_OK` (nenhum 4755 / setuid / setgid).

## Achados de metodo (nao falha de produto)

1. **OpenSSL obrigatorio no configure** (`find_package(OpenSSL REQUIRED)` por PLG-LOAD). Sem `openssl-devel` o configure aborta; gate exige `LISTED >= 13` antes do ctest.
2. **Plugin path base sem o+w**: `PETRUSH_TEST_HOME=/home/builder` (755). `/tmp` e `/var/tmp` 1777 quebram o walk de dirs (PLG-NARC).
3. **`asm_netcom_eperm` precisa de `unshare` user+net**: `--security-opt seccomp=unconfined`. Nao usar `--cap-add=NET_ADMIN` (quebra `asm_netcom` unit com CAP=0).
4. **`configsh` smoke** (`cxx-tui.sh`) reconfigura/rebuilda Release isolado em scratch; precisa `python3` (PTY via `pty.fork`) e `-t` no `docker run`. Sem display `:0`.
5. **ASan no clang e estatico**: prova = `nm` por `__asan_init` / `__ubsan_`, nao `ldd`.

## Criterios de saida

- [x] `ctest -R 'asm_|configsh|plugin_'` Sanitize 13/13
- [x] `ctest -R 'asm_|configsh|plugin_'` Release 13/13
- [x] Docker fedora:44 clang
- [x] ASan/UBSan comprovados no binario Sanitize
- [x] Zero mode 4755
- [x] Sem display `:0`
- [x] Sem tag
- [x] Sem push (orquestrador)
- [x] Relatorio em `docs/memory/gate-cxxasm.md`
- [x] TODO GATE-CXXASM -> 🔍 (verificacao entregue; ✅ so pos AUD)

## Residual / fora de escopo

- Matrix gcc / Ubuntu / Arch / CachyOS: CI GHA, nao este gate.
- Mutation testing adversarial das tres familias: nao pedido.
- `plugin_abi` custom target (clang `-c` header): fora do filtro `plugin_`; cobertos por PLG-ABI / TST-PLG runtime ABI.
- Tag/release: proibidos neste item.
