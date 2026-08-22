# AUD-DEPS - Dependências e acoplamento (linenoise + 4 camadas)

| Campo | Valor |
|-------|-------|
| **ID** | AUD-DEPS |
| **Data** | 2026-08-22 |
| **Auditor** | software-architect |
| **Escopo** | vendor linenoise, grafo de includes Front/Mid/Back/Foundation, fan-out CMake, licença BSD-2 × AGPL-3.0 |
| **Pré-reqs cruzados** | TST-T5 (`docs/memory/tst-t5-deps.md`), TST-T12 (`docs/memory/tst-t12-cves.md`) |
| **Manuais** | `AUDITORIAS.md` (AUD-DEPS), CONTRACT §5 (direção de deps), `docs/architecture.md` |
| **Porte** | early (Pipeline-Sprint; `.bigtech-porte`) |
| **Código de produto** | não alterado (só relatório + status TODO) |
| **Push** | não |
| **SHA HEAD (pré-commit)** | `7b15ecca2ff6a8ccf589069acc3f6dfc6fab7a1f` |

## 1. Método

1. Inventariar dependências runtime/build/vendor (árvore + `ldd build/petrush`).
2. Cruzar **TST-T5** (SCA trivy/grype/osv) e **TST-T12** (CVE + toolchain/CI + SEC-08).
3. Extrair `#include "petrush/..."` e `#include "linenoise.h"` em `src/` + `include/`.
4. Mapear superfície API linenoise usada e alvos CMake que compilam `vendor/linenoise/linenoise.c`.
5. Checar licença BSD-2-Clause do vendor vs AGPL-3.0 do projeto (compatibilidade + atribuição).
6. Classificar achados; recomendar patches **sem** aplicar código de produto.

## 2. Inventário de dependências

| Artefato | Tipo | Versão / origem | Papel |
|----------|------|-----------------|-------|
| `vendor/linenoise/` | C embutido | linenoise **1.0** (antirez; patches locais) | Única dep de terceiro no binário `petrush` |
| libc | runtime dinâmico | glibc **2.43** (Fedora 44) | `ldd build/petrush` → só `libc.so.6` (+ vdso/ld) |
| Toolchain | build host/CI | gcc 16.1.1, clang 22.1.8, cmake 4.3.0 | Não vendored |
| `pudod` | binário irmão | só código próprio em `src/pudod/` | **Sem** linenoise |

Sem lockfile / manifesto de ecossistema. SCA de pacote gerenciado **não vê** C vendored (já documentado em T5/T12).

### Patches locais no vendor (fork consciente)

Documentados em `vendor/linenoise/README.md`:

1. SEC-08 / CVE-2025-9810: `O_NOFOLLOW` + `fchmod(0600)` em `linenoiseHistorySave`
2. API `linenoiseHistoryLen` / `linenoiseHistoryGet`
3. UX-20 Ctrl-R (`linenoiseHistorySearch` + modo em `linenoiseEditFeed`)
4. UX-21 `linenoiseSetHighlightCallback`

### Superfície linenoise consumida pelo produto (`src/` + `include/`)

| Símbolo | Onde |
|---------|------|
| `linenoise` / HistoryLoad/Save/Add/SetMaxLen | `src/main.c` (Front composition) |
| SetCompletion/Hints/FreeHints/HighlightCallback, AddCompletion, HistoryLen/Get | `src/front/complete.c` |
| `linenoiseClearScreen` | `src/mid/dispatcher.c` (`builtin_clear`) |
| `linenoiseHistoryLen` / `linenoiseHistoryGet` | `src/mid/hist_expand.c` |
| `#include "linenoise.h"` (tipos) | `include/petrush/complete.h` |

## 3. Cruzamento TST-T5 e TST-T12

| Item | Veredicto herdado | O que AUD-DEPS consome |
|------|-------------------|------------------------|
| **TST-T5** | 🔍; grype+trivy+osv EXIT 0; **0 CRITICAL** vendor; CVE-2025-9810 MEDIUM mitigado | Inventário SCA + classificação linenoise |
| **TST-T12** | 🔍; reconfirma CVE↔SEC-08; fedora:44 grype 0; Actions SBOM 0 | Toolchain/CI + prova de mitigação |

### CVE linenoise (consolidado T5+T12)

| CVE | Severidade | Afeta vendor petrush? | Status para AUD-DEPS |
|-----|------------|----------------------|----------------------|
| **CVE-2025-9810** | MEDIUM 6.8 | Sim (upstream 1.0) | Mitigado localmente (SEC-08); teste `test_linenoise_history` |
| CVE-2013-7458 | LOW (classe perms) | Classe similar | Mitigado via `fchmod` 0600 |
| CVE-2021-46481 | MEDIUM | Não (fork Jsish) | N/A |

**Gate deps:** sem CRITICAL no vendor próprio. Residual MEDIUM é **dívida de fork** (manter patch alinhado ao fix upstream `f2558e1e…`), não bloqueio de SCA nesta onda.

Limite honesto (T5/T12): scanners não analisam source C linha a linha; a prova do vendor é NVD/OSV + inspeção do patch + teste.

## 4. Grafo de acoplamento (includes)

Direção esperada (early): **Front → Mid → Foundation**; Back thin; `pudod` isolado.

```
main.c (Front)
  → Mid (parser, dispatcher, hist_expand, …) + Foundation + complete + linenoise

front/complete.c → Mid(dispatcher) + Foundation(env) + highlight + linenoise
front/highlight.c → (próprio)
front/rc_trust.c → (próprio)

mid/dispatcher.c → Foundation + Mid siblings + linenoise     *** UI no Mid ***
mid/hist_expand.c → linenoise                                *** UI no Mid ***
mid/source.c → Mid + rc_trust.h                              *** ver AUD-ARCH F2 ***
mid/{parser,alias,dirstack,expand,prompt,pudo}.c → Mid/Foundation (sem linenoise)

foundation/process.c → env + parser.h (tipos Mid; AUD-ARCH F4)
foundation/{env,job}.c → próprios

pudod/* → headers locais apenas (0 petrush/*, 0 linenoise)
```

### Ciclos

| Ciclo | Camada | Avaliação |
|-------|--------|-----------|
| `dispatcher` ↔ `pudo` / `source` (headers Mid) | Mid | Aceitável na mesma camada; acoplamento alto de builtins, não violação inter-camada |
| Mid ↔ linenoise | Mid → vendor UI | **Violação** de fronteira (D1) |
| Nenhum ciclo Front ↔ Foundation via linenoise | - | OK |

### Fan-out CMake (compila `linenoise.c`)

14 menções em `CMakeLists.txt`; alvos que linkam o `.c`:

| Alvo | Motivo aparente |
|------|-----------------|
| `petrush` | produto |
| `test_info`, `test_pipeline_builtin`, `test_source` | puxam `dispatcher.c` → `#include linenoise.h` |
| `test_complete`, `test_hist_expand` | exercitam Front/Mid que falam com history API |
| `test_linenoise_history` | SEC-08 / UX-20 no vendor |

Sintoma: Mid depende de UI → qualquer teste do dispatcher arrasta o vendor inteiro.

## 5. Achados

Severidade: **HIGH** = acoplamento que viola CONTRACT §5 / troca de vendor cara; **MEDIUM** = risco de manutenção ou vazamento de contrato; **LOW** = polish / atribuição; **OK** = conforme.

### D1 - HIGH - Mid importa linenoise (lib de UI)

| | |
|--|--|
| **Onde** | `src/mid/dispatcher.c` (`linenoiseClearScreen`); `src/mid/hist_expand.c` (`HistoryLen`/`HistoryGet`) |
| **Regra** | Mid não deve importar framework/lib de apresentação (CONTRACT §5; AUD-ARCH F1) |
| **Impacto deps** | Troca/mock de linenoise exige Mid; testes Mid compilam vendor; CVE do line-editor contamina camada de domínio |
| **Mitigação sugerida** | Porta Front `petrush_ui_clear_screen`; porta history (`petrush_history_get/len`) alimentada no boot. Sem mudar produto nesta fatia. |

### D2 - MEDIUM - Header Front vaza tipos do vendor

| | |
|--|--|
| **Onde** | `include/petrush/complete.h` → `#include "linenoise.h"` |
| **Impacto** | Qualquer TU que inclua `complete.h` herda a API linenoise; contrato público do petrush acopla ao layout do vendor |
| **Mitigação** | Forward-declare callbacks só em `.c`; header público sem `linenoise.h` (ou `complete_internal.h` privado) |

### D3 - MEDIUM - Fork local diverge do upstream

| | |
|--|--|
| **Onde** | 4 patches em `vendor/linenoise/` (SEC-08, History API, Ctrl-R, highlight) |
| **Impacto** | Sync futuro pode reintroduzir CVE-2025-9810 ou conflitar com fix upstream; custo de review a cada bump |
| **Mitigação** | Manter `vendor/linenoise/README.md` como changelog de patch; ao bump, diff explícito contra `f2558e1e` / tag; não “atualizar e torcer” |

### D4 - MEDIUM - Fan-out de build: linenoise em N alvos de teste

| | |
|--|--|
| **Onde** | `CMakeLists.txt` (petrush + ≥6 test executables) |
| **Impacto** | Tempo de compile; warnings do vendor relaxados em vários alvos; acoplamento de build espelha D1 |
| **Mitigação** | Após porta D1: testes Mid deixam de precisar de `linenoise.c`. Opcional: `add_library(linenoise STATIC …)` única |

### D5 - LOW - Atribuição BSD-2 incompleta na superfície de distribuição

| | |
|--|--|
| **Onde** | `LICENSE.md` = só AGPL-3.0; `README.md` **não** menciona linenoise/BSD; aviso BSD vive nos headers do vendor + `vendor/linenoise/README.md` |
| **Regra BSD-2** | Redistribuição em binário deve reproduzir copyright/disclaimer |
| **Mitigação** | `NOTICE` (ou seção “Third-party” no README) citando Salvatore Sanfilippo / Pieter Noordhuis, BSD-2-Clause, path `vendor/linenoise/` |

### D6 - OK - Runtime mínimo e isolamento pudod

- `ldd`: apenas libc.
- `pudod` sem linenoise e sem `petrush/*`.
- Back thin: sem deps extras.

### D7 - OK - Licença BSD-2-Clause × AGPL-3.0

Permissiva BSD-2 é **combinável** com AGPL-3.0 do petrush (código petrush permanece AGPL; vendor mantém BSD). Não há copyleft inverso do linenoise. Residual = atribuição (D5), não incompatibilidade.

### D8 - OK / INFO - Ciclos Mid internos

`dispatcher` ↔ `pudo`/`source`: acoplamento de monólito de builtins (AUD-QUALITY), não ciclo de deps externas.

## 6. Score e veredito

| Dimensão | Nota (0-20) | Nota |
|----------|-------------|------|
| Inventário / superfície externa | 18 | Uma dep vendored; libc only; pudod limpo |
| Cruzamento CVE (T5/T12) | 17 | 0 CRITICAL; MEDIUM mitigado + teste; SCA cego a C (limitação conhecida) |
| Acoplamento camadas ↔ vendor | 10 | D1 HIGH + D2 MEDIUM |
| Ciclos / fan-out build | 13 | Sem ciclo inter-camada via vendor; fan-out CMake (D4) |
| Licença / atribuição | 14 | Compatível (D7); atribuição de distribuição fraca (D5) |
| **Total** | **72 / 100** | |

**Veredito:** **APROVADO COM RESSALVAS** para porte early.

Não há explosão de deps nem CRITICAL aberto. O problema central de AUD-DEPS é **acoplamento Mid↔linenoise** (e o fan-out/build que dele decorre), já sinalizado por AUD-ARCH F1. Nenhuma mudança de produto nesta fatia.

**Não marca ✅** (só 🔍). ✅ fica para onda TST/AUD consolidada / AUD-REPORT.

## 7. Patches recomendados (ordem; não aplicados)

1. **API (produto, fatia futura)** - Remover `#include "linenoise.h"` do Mid: clear + history via portas (fecha D1; reduz D4).
2. **API header** - `complete.h` sem include direto de linenoise (D2).
3. **DOC** - `NOTICE` ou seção Third-party no README (D5); opcional alinhar `architecture.md` com nota “linenoise só Front”.
4. **BUILD (opcional)** - `linenoise` como lib estática única no CMake após (1).
5. **MANUTENÇÃO** - Ao tocar vendor: checklist de diff vs upstream fix CVE-2025-9810 (D3).

## 8. Relação com outros itens

| ID | Relação |
|----|---------|
| **TST-T5** | Pré-req; inventário SCA + classificação CVE |
| **TST-T12** | Pré-req; CVE↔SEC-08 + CI |
| **AUD-ARCH** | F1 = D1; F2 (`rc_trust`) é acoplamento de camada, fora do vendor |
| **AUD-SEC** | SEC-08 / history TOCTOU |
| **AUD-QUALITY** | Encaminhou linenoise Mid para esta AUD |
| **AUD-REPORT** | Consolidar score 72 + patches 1-5 |

## 9. Evidências

- Includes: varredura `#include "` em `src/` e `include/petrush/`
- Uso linenoise: `rg linenoise src include`
- CMake: 14 hits `vendor/linenoise`; alvos listados na §4
- Runtime: `ldd build/petrush` → libc only
- Licença: cabeçalhos BSD em `vendor/linenoise/linenoise.{c,h}`; projeto `LICENSE.md` AGPL-3.0
- T5/T12: `docs/memory/tst-t5-deps.md`, `docs/memory/tst-t12-cves.md`
- Critério: `Projects/petrush/AUDITORIAS.md` § AUD-DEPS

## 10. Checklist de saída AUD-DEPS

- [x] Inventário vendor + runtime + build
- [x] Grafo de includes / acoplamento camadas
- [x] Cruzamento explícito TST-T5 e TST-T12
- [x] Licença vendor × projeto
- [x] Achados classificados + score
- [x] Relatório em `docs/auditoria/aud-deps.md`
- [ ] Status TODO `🔍` (este commit)
- [ ] ✅ só após julgamento do orquestrador / fechamento AUD

*Fim AUD-DEPS. Sem alteração de código de produto. Sem push. Sem em-dash.*
