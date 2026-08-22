# TST-T15 Pré-CI (container Fedora 44)

**Data:** 2026-08-22T19:56:54-03:00  
**SHA HEAD (pré-commit do relatório):** `eb05fdef4812d88c6ee36c55b067db971f3f6f4f`  
**Agent:** devops-sre  
**Item:** TST-T15 (W14)  
**Veredicto:** **FAIL** (Release smoke sem install + clang-tidy Release; Debug gcc completo PASS)

## Escopo

Rodar a suíte do CI no container local (`registry.fedoraproject.org/fedora:44`), espelhando `.github/workflows/ci.yml`, antes de push. Lint **sem** engolir falha (`|| true`). Sem setuid. Sem clone. Sem push.

**In:** dnf deps, cmake Release+Debug gcc, build binários+testes, `check`, smoke, `cppcheck`, `clang-tidy`, valgrind em `pudod`.  
**Out:** setuid/`chmod u+s`, push remoto, matriz clang completa (tempo; gcc cobre o mínimo pedido).

## Ambiente

| Peça | Valor |
|---|---|
| Runtime | Docker 29.7.2 (podman ausente nesta máquina) |
| Imagem | `registry.fedoraproject.org/fedora:44` (`sha256:f2d7418f…`) |
| Mount | repo → `/src:z` (SELinux `:z` obrigatório; sem label = Permission denied) |
| Compilers no container | gcc 16.2.1 / clang 22.1.8 |
| Build dirs | `/src/build-preci-rel`, `/src/build-preci-dbg` |
| Scripts | `scripts/tst-t15-preci.sh`, `scripts/tst-t15-preci-phase2.sh` |
| Log bruto | `docs/memory/tst-t15-preci-run.log` (gitignore `*.log`) |

## Execução (exit codes)

### Fase 1 — Release gcc (mínimo)

| Step | Comando | EXIT | Nota |
|---|---|---:|---|
| dnf | `dnf -y install gcc … valgrind cppcheck clang-tools-extra …` | 0 | 35 s |
| cmake_rel | `cmake -B build-preci-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc` | 0 | |
| build_rel | `cmake --build … --target petrush pudod test_*` (18 testes) | 0 | 10 s |
| check_rel | `cmake --build … --target check` | 0 | 18/18 |
| smoke_rel | `bash tests/smoke/pudo-smoke.sh build-preci-rel/petrush` | **1** | 51 PASS / **2 FAIL** |
| cppcheck_rel | `cmake --build … --target cppcheck` | 0 | sem `\|\| true` |
| clang_tidy_rel | `cmake --build … --target clang-tidy` | **2** | sem `\|\| true` |
| valgrind_rel | `valgrind --leak-check=full --error-exitcode=1 --quiet build-preci-rel/pudod /usr/bin/true` | 1 | deny allow-list (esperado); sem relatório de leak |

### Fase 2 — Debug gcc + Release instalado (sem setuid)

| Step | Comando | EXIT | Nota |
|---|---|---:|---|
| cmake/build/check_dbg | Debug gcc | 0 | 18/18 |
| smoke_dbg | smoke em `build-preci-dbg/petrush` | 0 | **53/53** |
| cppcheck_dbg | target cppcheck | 0 | |
| clang_tidy_dbg | target clang-tidy | 0 | |
| install_rel | `cmake --install build-preci-rel --prefix /usr/local` | 0 | **sem** setuid |
| smoke_rel_installed | smoke em `/usr/local/bin/petrush` | 0 | **53/53** |
| valgrind_installed | valgrind em `/usr/local/bin/pudod` | 1 | deny allow-list; sem leak report |

## Falhas (causa raiz)

### 1. Smoke Release sem install (espelha GHA)

`pudo` em Release (`NDEBUG`) só aceita `pudod` sob dirs de install (SEC-02: `/usr/local/bin`, `/usr/local/libexec`, …). Sibling em `build-preci-rel/pudod` é rejeitado → fallback sudo → texto não casa o expect do smoke.

```text
pudo: aviso: pudod não encontrado, usando sudo como fallback
FAIL: pudo execution / pudo false  (2 casos)
```

**Confirmação GHA** (run [32572058696](https://github.com/petrinhu/petrush/actions/runs/32572058696), SHA `680e0191`):  
`build-and-test (gcc, Release)` e `(clang, Release)` falham no step **Smoke integrado**; Debug gcc/clang passam smoke.

**Mitigação medida:** `cmake --install … --prefix /usr/local` **sem setuid** → smoke Release 53/53.

### 2. clang-tidy Release = ArrayBound em `pudo.c:539`

```text
src/mid/pudo.c:539:9: error: Potential out of bound access to 'self'
  with tainted index [clang-analyzer-security.ArrayBound,-warnings-as-errors]
n = readlink("/proc/self/exe", self, sizeof(self) - 1);
self[n] = '\0';
```

Com `bufsiz = sizeof(self)-1`, `n` máximo é `PATH_MAX-1` e `self[n]` é o último índice válido (falso positivo clássico do analyzer vs contrato do `readlink`). Em **Debug** o mesmo target sai 0 (compile_commands/`NDEBUG` muda o grafo analisado).

## Contagem resumo

| Suite | Release (tree) | Debug | Release (install, sem setuid) |
|---|---|---|---|
| check (ctest) | 18/18 PASS | 18/18 PASS | (n/a) |
| smoke | 51/53 FAIL | 53/53 PASS | 53/53 PASS |
| cppcheck | PASS (0) | PASS (0) | — |
| clang-tidy | FAIL (2) | PASS (0) | — |
| valgrind pudod | deny exit 1 (OK) | — | deny exit 1 (OK) |

## Critério de saída TST-T15

- [x] Container Fedora 44 local executado (docker; podman indisponível)
- [x] Deps dnf espelhando `ci.yml`
- [x] Release gcc: build + check + smoke + cppcheck + clang-tidy + valgrind
- [x] Debug gcc: build + check + smoke + lint
- [x] Lint sem `|| true`
- [x] Sem setuid
- [x] Relatório em `docs/memory/tst-t15-preci.md`
- [ ] Tudo verde — **NÃO** (Release smoke tree + clang-tidy Release)
- [ ] Status `🔍` — **bloqueado** até remediação (CI Release smoke e/ou ArrayBound)

## Handoff sugerido

1. **CI / SEC-02:** ou instalar `pudod` no job Release antes do smoke (sem setuid), ou restringir smoke duro a Debug, ou alargar política de sibling só para path do próprio binário sob teste.  
2. **clang-tidy:** silenciar FP `ArrayBound` em `self[n]` pós-`readlink` (assert `n < sizeof self` / cast tamanho) e re-rodar target Release.  
3. Re-rodar este script (`scripts/tst-t15-preci*.sh`) até overall verde → aí sim `🔍`.

## Referências

- Item: `TODO.md` → TST-T15  
- `TESTES.md` (projeto) § TST-T15  
- Workflow: `.github/workflows/ci.yml`  
- GHA vermelho Release: run 32572058696  
- Sibling SEC-02: `src/mid/pudo.c` `pudo_allow_pudod_candidate` / `find_pudod_binary`
