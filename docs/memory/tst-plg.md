# TST-PLG - harness `ctest -R plugin_` (ww / hash / ABI / path / dlopen pos-allow)

**Data:** 2026-08-23T04:49:01-03:00  
**SHA HEAD (pré-commit do relatório):** `814d8bdae74aa31277905fec016b0012771f99e9`  
**Agent:** qa-engineer  
**Item:** TST-PLG (W26)  
**Veredicto:** **PASS**

## Escopo

Gate de regressão do loader de plugins (`PLG-LOAD`) no fim da onda W26: todos os testes cujo nome casa com `plugin_`, em Docker `fedora:44`, **gcc**, build **Release**, user **unpriv**, home sem `o+w`.

**In:**
- `ctest -R plugin_` (2 labels: `plugin_load` + `plugin_pudod_no_dl`)
- 13 subcasos acutest em `tests/test_plugin_load.c`
- cobertura exigida: **ww**, **hash mismatch**, **ABI**, **path fora**, **dlopen so pos-allow**
- Docker `fedora:44` + GCC 16; `PETRUSH_ASM=ON`
- prova mode `755` (sem `4755` / setuid) em `plugin_load`, `pudod`, `.so` de teste

**Out:** setuid/`pudod` privilegiado; display `:0`; push; tag; GATE-CXXASM; custom target `plugin_abi` (exige clang `-c`; gate ABI de runtime = `plugin_load_abi_major_mismatch`).

## Ambiente

| Peca | Valor |
|---|---|
| Imagem | `fedora:44` / `registry.fedoraproject.org/fedora:44` (`sha256:f2d7418fa4ad…`) |
| Compilers | gcc/g++ 16.2.1 (Red Hat 16.2.1-2.fc44); ASM = GNU as via gcc |
| CMake | 4.3.0; `CMAKE_BUILD_TYPE=Release`; `PETRUSH_ASM=ON`; `ENABLE_COVERAGE=OFF` |
| Build dir | `/work/build` (container); host scratch `/var/tmp/petrush-tst-plg-work` |
| User | `builder` uid=1000; `CapEff=0`; `HOME=/home/builder` mode `755` |
| Env | `PETRUSH_TEST_HOME=/home/builder` (evita `/tmp`/`/var/tmp` o+w no walk de dirs) |
| Jobs | `-j2`; `TMPDIR=/var/tmp` |
| Log | `/var/tmp/petrush-tst-plg-docker.log` + `/var/tmp/petrush-tst-plg-work/out/` |

## Labels `ctest -R plugin_`

| # | Nome | Tipo |
|---|---|---|
| 17 | `plugin_load` | unit/integration (`tests/test_plugin_load.c` → 13 subcasos) |
| 18 | `plugin_pudod_no_dl` | smoke (`tests/smoke/plugin-pudod-no-dl.sh`) |

## Subcasos acutest (13/13)

| Subcaso | Requisito TST-PLG |
|---|---|
| `plugin_sha256_abc` | digest SHA-256 baseline (`abc`) |
| `plugin_ww_file` | **ww** ficheiro `o+w` |
| `plugin_ww_parent_dir` | **ww** dir pai `o+w` |
| `plugin_path_safe_ok` | path seguro (controle negativo) |
| `plugin_allow_default_deny` | **dlopen pos-allow** (ficheiro allow ausente) |
| `plugin_allow_basename_reject` | **path fora** (basename na allow-list rejeitado) |
| `plugin_allow_parse_and_find` | allow path canonico + SHA |
| `plugin_search_rejects_relative` | **path fora** (entrada relativa em `PETRUSH_PLUGIN_PATH` ignorada) |
| `plugin_load_no_allow_file` | **dlopen pos-allow** (`PETRUSH_PLG_ERR_ALLOW`, sem load) |
| `plugin_load_hash_mismatch` | **hash mismatch** (`PETRUSH_PLG_ERR_HASH`) |
| `plugin_load_ww_denied` | **ww** no load (`PETRUSH_PLG_ERR_PERM`) |
| `plugin_load_abi_major_mismatch` | **ABI** major!=1 (`PETRUSH_PLG_ERR_ABI`) |
| `plugin_load_ok` | happy path (allow+hash+ABI ok → `dlopen`/`init`) |

## Execucao

### Pacotes no container

```text
dnf -y install gcc gcc-c++ cmake make binutils gettext \
  glibc-devel openssl-devel diffutils findutils which util-linux
```

### Release (gcc, unpriv)

```text
useradd -m -u 1000 builder; chmod 755 /home/builder
runuser -u builder -- env HOME=/home/builder PETRUSH_TEST_HOME=/home/builder …

cmake -S /src -B /work/build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
  -DPETRUSH_ASM=ON -DENABLE_COVERAGE=OFF
cmake --build /work/build -j2 --target \
  plugin_load plugin_test_ok plugin_test_bad_major pudod

stat -c '%a %n' plugin_load pudod test_plugins/*.so
# 755 em todos; sem setuid; nenhum 4755

/work/build/plugin_load --verbose
# Count of run unit tests: 13
# Count of failed unit tests: 0

ctest --test-dir /work/build -R 'plugin_' --output-on-failure
# 1/2 plugin_load ........ Passed
# 2/2 plugin_pudod_no_dl .. Passed
# 100% tests passed, 0 tests failed out of 2
```

### Host (regressao rapida)

```text
ctest --test-dir build -R 'plugin_' --output-on-failure
# 100% tests passed, 0 tests failed out of 2
```

Log host: `/var/tmp/petrush-tst-plg-host-ctest.log`.

## Contagem

| Suite | Comando | Passed | Failed | Tempo | EXIT |
|---|---|---:|---:|---:|---:|
| docker acutest | `plugin_load --verbose` | 13 | 0 | ~0.05 s | 0 |
| docker ctest | `ctest -R plugin_` | 2 | 0 | 0.07 s | 0 |
| host ctest | `ctest -R plugin_` | 2 | 0 | 0.06 s | 0 |

## Mapa de requisitos (REQ_OK)

| Requisito | Evidencia |
|---|---|
| ww | `plugin_ww_file`, `plugin_ww_parent_dir`, `plugin_load_ww_denied` |
| hash mismatch | `plugin_load_hash_mismatch` (deny `hash-mismatch`) |
| ABI | `plugin_load_abi_major_mismatch` (deny `abi-major`, major=2 vs host=1) |
| path fora | `plugin_search_rejects_relative` + `plugin_allow_basename_reject` |
| dlopen so pos-allow | `plugin_load_no_allow_file` / `plugin_allow_default_deny` falham **antes** do `dlopen`; `plugin_load_ok` so passa apos allow+hash |

Ordem no fonte `src/foundation/plugin_load.c` (`petrush_plugin_load`):

1. `realpath` / resolve  
2. walk world-writable (ficheiro + dirs)  
3. allow-list parse + `petrush_plugin_allow_find` (L574)  
4. `open`/`fstat` + recheck `S_IWOTH`  
5. `petrush_plugin_sha256_fd` (chamada L603) + hex eq  
6. **so entao** `dlopen` (L622) → `query`/`init`

Invariante pudod: `plugin_pudod_no_dl` confirma fontes + binario sem `dlopen`/`libdl`.

## Achados

Nenhum funcional. Armadilha operacional (ja vista em PLG-LOAD): se `HOME`/`PETRUSH_TEST_HOME` cair sob `/tmp` ou `/var/tmp` (1777), o walk de dirs marca o path como world-writable e varios casos “positivos” falham com `PETRUSH_PLG_ERR_PERM` em vez do codigo esperado. Mitigacao: user com home `755` + `PETRUSH_TEST_HOME` absoluto fora de tmp.

## Limitacoes (honesto)

1. Custom target `plugin_abi` (`plg-abi-header.sh` + clang `-std=c11 -c`) **nao** entrou neste gate (imagem gcc-only). ABI de **runtime** no loader esta coberta. Header ABI ja foi gate de `PLG-ABI`.
2. Nao cobre ASan/UBSan do `plugin_load`, mutation adversarial, nem GATE-CXXASM (`asm_`+`configsh`+`plugin_` juntos).
3. TOCTOU residual POSIX (`dlopen` por path apos hash do fd) permanece documentado em `docs/security/plugins-threat.md`; o teste prova a ordem dos checks, nao elimina a janela do kernel.
4. Nao substitui CI remoto.

## Criterio de saida TST-PLG

- [x] `ctest -R plugin_` encontra e passa (2/2) em Docker `fedora:44` gcc
- [x] 13/13 subcasos acutest verdes (verbose)
- [x] Requisitos ww / hash / ABI / path fora / dlopen pos-allow mapeados e OK
- [x] mode ≠ 4755; suite unpriv (`CapEff=0`)
- [x] Relatorio em `docs/memory/tst-plg.md`
- [ ] ✅ na tabela so apos onda de auditoria / GATE (execucao → 🔍)

**Status sugerido no TODO:** `🔍 Pendente verificacao` (execucao entregue; ✅ so pos auditoria/TST da onda).

## Referencias

- Threat model: `docs/security/plugins-threat.md` (PLG-NARC)
- Loader: `src/foundation/plugin_load.c`
- Testes: `tests/test_plugin_load.c`, `tests/smoke/plugin-pudod-no-dl.sh`
- Manual: vault `TESTES.md` (T1 unitario + smoke de invariante)
- Predecessores: PLG-LOAD, PLG-ABI, PLG-NARC
