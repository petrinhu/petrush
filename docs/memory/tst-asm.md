# TST-ASM - harness `tests/asm/` (`ctest -R asm_`)

**Data:** 2026-08-23T04:44:43-03:00  
**SHA HEAD (pré-commit do relatório):** `a86c9a425d2bfb46f12642da1c682c99386f0c9c`  
**Agent:** qa-engineer  
**Item:** TST-ASM (W23)  
**Veredicto:** **PASS**

## Escopo

Gate de regressão do harness ASM completo no fim da onda W23: todos os testes cujo nome casa com `asm_`, em Docker `fedora:44`, clang, nos build types **Sanitize** e **Release**.

**In:**
- harness `tests/asm/` (já existente)
- `ctest -R asm_` (10 testes: 9 unit + `asm_netcom_eperm`)
- Docker `registry.fedoraproject.org/fedora:44` + clang 22
- `CMAKE_BUILD_TYPE=Sanitize` (ASan+UBSan em C/C++; ASM sem sanitize)
- `CMAKE_BUILD_TYPE=Release`
- prova mode `755` (sem 4755) nos binários ASM + `petrush`

**Out:** setuid/`pudod` privilegiado; display `:0`; push; tag; GATE-CXXASM; lint/clang-tidy.

## Ambiente

| Peça | Valor |
|---|---|
| Imagem | `registry.fedoraproject.org/fedora:44` (`sha256:f2d7418fa4ad…`) |
| Compilers | clang/clang++ 22.1.8 (Fedora 22.1.8-4.fc44); ASM = Clang GAS |
| CMake | 4.3.0; `PETRUSH_ASM=ON`; `ENABLE_COVERAGE=OFF` |
| Build dirs | `/var/tmp/petrush-tst-asm-san`, `/var/tmp/petrush-tst-asm-rel` |
| Docker opts | `--network=host --security-opt seccomp=unconfined` (libera `unshare` user+net **sem** CAP_NET_ADMIN no container) |
| ASAN_OPTIONS | `detect_leaks=1:halt_on_error=1:abort_on_error=1:detect_stack_use_after_return=1` |
| UBSAN_OPTIONS | `halt_on_error=1:print_stacktrace=1` |
| Jobs | `-j2`; `TMPDIR=/var/tmp` |
| Log | `/var/tmp/petrush-tst-asm-docker.log` |

## Labels `ctest -R asm_`

| # | Nome | Tipo |
|---|---|---|
| 21 | `asm_memeq` | unit (`tests/asm/test_memeq.c`) |
| 22 | `asm_crc32` | unit |
| 23 | `asm_parse_i64` | unit |
| 24 | `asm_hash_path` | unit |
| 25 | `asm_job_setpgid` | unit |
| 26 | `asm_utf8` | unit |
| 27 | `asm_tty` | unit (PTY; sem display `:0`) |
| 28 | `asm_wai` | unit (overlay sysfs) |
| 29 | `asm_netcom` | unit (overlay + CAP=0) |
| 30 | `asm_netcom_eperm` | smoke (`tests/smoke/netcom-eperm.sh`; depende de `petrush`) |

## Execução

### Pacotes no container

```text
dnf -y install gcc gcc-c++ clang compiler-rt cmake make \
  binutils gettext libasan libubsan openssl-devel glibc-devel \
  libcap util-linux diffutils findutils which
```

### Sanitize (clang)

```text
cmake -B /build-san -S /src \
  -DCMAKE_BUILD_TYPE=Sanitize \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DENABLE_COVERAGE=OFF -DPETRUSH_ASM=ON
cmake --build /build-san -j2 --target \
  asm_memeq asm_crc32 asm_parse_i64 asm_hash_path asm_job_setpgid \
  asm_utf8 asm_tty asm_wai asm_netcom petrush
# prova ASan/UBSan (clang liga runtime estático; ldd nao mostra libasan):
nm /build-san/petrush | grep -c __asan_init   # -> 1
nm /build-san/petrush | grep -c __ubsan_       # -> 52
ctest --test-dir /build-san -R 'asm_' --output-on-failure --timeout 30
# 100% tests passed, 0 tests failed out of 10
# CTEST_SANITIZE_EXIT=0
```

### Release (clang)

```text
cmake -B /build-rel -S /src \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DENABLE_COVERAGE=OFF -DPETRUSH_ASM=ON
cmake --build /build-rel -j2 --target \
  asm_memeq asm_crc32 asm_parse_i64 asm_hash_path asm_job_setpgid \
  asm_utf8 asm_tty asm_wai asm_netcom petrush
ctest --test-dir /build-rel -R 'asm_' --output-on-failure --timeout 30
# 100% tests passed, 0 tests failed out of 10
# CTEST_RELEASE_EXIT=0
```

## Contagem

| Suite | Comando | Passed | Failed | Tempo | EXIT | Veredicto |
|---|---|---:|---:|---:|---:|---|
| Sanitize | `ctest -R asm_` | 10 | 0 | 0.82 s | 0 | **PASS** |
| Release | `ctest -R asm_` | 10 | 0 | 0.12 s | 0 | **PASS** |
| Overall | ambos | 20 | 0 | - | 0 | **PASS** |

## Hardening / mode

Todos os binários ASM + `petrush` em ambos os builds: `mode=755`, `NO_SETUID_OK` (nenhum 4755 / setuid / setgid).

## Achados de método (nao falha de produto)

1. **OpenSSL obrigatorio no configure** (`find_package(OpenSSL REQUIRED)` por PLG-LOAD). Sem `openssl-devel` o configure aborta; um `ctest` em tree vazia devolve EXIT=0 com "No tests were found" (falso PASS). Gate: exigir `asm_tests_listed >= 10` + configure EXIT=0.
2. **ASan no clang e estatico**: `ldd` nao lista `libasan`; prova correta = `nm`/`strings` por `__asan_init`. Sob `set -o pipefail`, `strings \| grep -q` pode falhar por SIGPIPE mesmo com match (falso negativo).
3. **`asm_netcom_eperm` precisa de `unshare` user+net** no Docker. Default seccomp bloqueia. Solucao: `--security-opt seccomp=unconfined` (sem CAP_NET_ADMIN). Dar `--cap-add=NET_ADMIN` quebra `asm_netcom` unit (asserts `have_cap_net_admin() == 0`).

## Criterios de saida

- [x] Harness `tests/asm/` exercitado via `ctest -R asm_`
- [x] Docker fedora:44 clang **Sanitize** 10/10
- [x] Docker fedora:44 clang **Release** 10/10
- [x] ASan/UBSan comprovados no binario Sanitize
- [x] Zero mode 4755
- [x] Sem push (orquestrador)
- [x] Relatorio em `docs/memory/tst-asm.md`
- [x] TODO TST-ASM -> 🔍 (impl/verificacao entregue; ✅ so pos AUD)

## Residual / fora de escopo

- `asm_glob` / `test_glob` nao entram no filtro `asm_` (nome diferente); cobertos nas fatias ASM-GLOB.
- Matrix gcc / outras distros: GATE-CXXASM / CI GHA.
- Mutation testing adversarial do harness ASM: nao pedido neste item.
