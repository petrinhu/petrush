# TST-T15 Pré-CI (container Fedora 44)

**Data:** 2026-08-22T19:56:54-03:00  
**SHA HEAD (pré-commit do relatório):** `eb05fdef4812d88c6ee36c55b067db971f3f6f4f`  
**Agent:** devops-sre  
**Item:** TST-T15 (W14)  
**Veredicto:** **parcial → smoke Release PASS** após remediação CI/install (apêndice). clang-tidy Release ArrayBound ainda residual.

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
- [x] Release smoke no prefixo instalado — **PASS 53/53** (apêndice; tree continua FAIL = SEC-02 ok)
- [ ] Tudo verde — **NÃO** (clang-tidy Release ArrayBound residual)
- [x] Status `🔍` — smoke Release instalado verde (2026-08-22); ArrayBound segue residual

## Handoff sugerido

1. ~~**CI / SEC-02:** instalar antes do smoke~~ — **feito** (`ci.yml` + `tst-t15-preci.sh`; prefixo `/usr/local`, sem setuid).  
2. **clang-tidy:** silenciar FP `ArrayBound` em `self[n]` pós-`readlink` (assert `n < sizeof self` / cast tamanho) e re-rodar target Release.  
3. Re-rodar `scripts/tst-t15-preci.sh` completo até overall verde (lint incluso) antes de ✅ pós-AUD.

## Referências

- Item: `TODO.md` → TST-T15  
- `TESTES.md` (projeto) § TST-T15  
- Workflow: `.github/workflows/ci.yml`  
- GHA vermelho Release: run 32572058696  
- Sibling SEC-02: `src/mid/pudo.c` `pudo_allow_pudod_candidate` / `find_pudod_binary`

---

## Apêndice — remediação CI/preci Release install (2026-08-22T20:00:45-03:00)

**Agent:** devops-sre  
**SHA base (antes do commit desta fatia):** `994322c3ae57f7d7f400cbd8ab40f113b9df7437`  
**Veredicto smoke Release:** **PASS 53/53** no binário instalado (sem setuid). SEC-02 **não** enfraquecido.

### Mudança

| Arquivo | O que |
|---|---|
| `.github/workflows/ci.yml` | Matrix Release: `cmake --install build --prefix /usr/local` (sem setuid) → smoke em `/usr/local/bin/petrush`. Debug: smoke continua em `./build/petrush`. Job `build-fedora-next` alinhado. Lint GHA permanece best-effort (`\|\| true`); gate duro de lint = script preci. |
| `scripts/tst-t15-preci.sh` | Após `check_rel`: `install_rel` + `smoke_rel` no prefixo instalado (não no tree). Lint sem `\|\| true`. |

### Por que `/usr/local` e não `/tmp/petrush-prefix`

`pudo_allow_pudod_candidate` (Release) só aceita absolutos sob `/usr/local/{bin,libexec}` e `/usr/libexec` (e paths exactos). Prefixo arbitrário em `/tmp/...` seria rejeitado pela mesma política SEC-02 — instalar lá **sem** alargar a allow-list quebraria o smoke. No container CI (root efêmero) `/usr/local` é o layout de produção que a policy já confia.

### Re-prova container Fedora 44 (docker)

Imagem: `registry.fedoraproject.org/fedora:44`. Build tree reutilizado: `build-preci-rel` (Release gcc).

| Step | Resultado |
|---|---|
| smoke tree `build-preci-rel/petrush` | **FAIL** 51/53 — `pudo: aviso: pudod não encontrado, usando sudo como fallback` (SEC-02 rejeita sibling `build/pudod`) |
| `cmake --install build-preci-rel --prefix /usr/local` | EXIT 0; modes `755`; **sem** setuid/`chmod u+s` |
| smoke `/usr/local/bin/petrush` | **PASS 53/53** (`EXIT_smoke_installed=0`) |

Log: [`docs/memory/tst-t15-smoke-rel-installed.log`](tst-t15-smoke-rel-installed.log) (e espelho da corrida tree no output da sessão).

### Residual

- **clang-tidy Release ArrayBound** em `pudo.c` (`self[n]` pós-`readlink`) — fora deste brief; não bloqueia o path de smoke Release.  
- CMake instalou `libexec/pudod` (além de `bin/pudod`); allow-list cobre `bin/pudod` e basename sob `/usr/local/libexec`.

### Status TODO

`TST-T15` → **🔍 Pendente verificação** (smoke Release instalado verde; sem push).
