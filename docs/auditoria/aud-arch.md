# AUD-ARCH — Arquitetura e 4 camadas (petrush)

| Campo | Valor |
|-------|-------|
| **ID** | AUD-ARCH |
| **Data** | 2026-08-22 |
| **Auditor** | software-architect |
| **Escopo** | `docs/architecture.md`, tree `src/`, headers `include/petrush/`, includes cruzados, `CMakeLists.txt` |
| **Manuais** | CONTRACT §5, `docs/standards.md`, `AUDITORIAS.md` (AUD-ARCH), princípios 4 camadas |
| **Porte** | early (Pipeline-Sprint; `.bigtech-porte`) |
| **Código de produto** | não alterado nesta auditoria (só relatório + status TODO) |
| **Push** | não |

## 1. Método

1. Ler CONTRACT §5 (direção Front → Mid → Back ← Foundation/Infra) e adaptação petrush em `docs/standards.md` / `docs/architecture.md`.
2. Inventariar `src/{front,mid,back,foundation,pudod}` + `src/main.c`.
3. Extrair `#include "petrush/..."` e `#include "linenoise.h"` por unidade.
4. Cruzar com o checklist AUD-ARCH do repo: camadas, SOLID/DRY sob lente de fronteira, “só platform/Foundation chama libc de processo”.
5. Classificar achados; recomendar patches sem aplicar código de produto.

## 2. Inventário físico × lógico

Mapeamento canônico declarado em `docs/architecture.md` (Opção 2, camadas lógicas pragmáticas).

| Camada | Física observada | Papel declarado | Estado |
|--------|------------------|-----------------|--------|
| **Front** | `src/main.c` (raiz), `src/front/complete.c`, `highlight.c`, `rc_trust.c` | REPL, linenoise UX, highlight, trust do rc | Ativo; `main.c` fora de `src/front/` de propósito |
| **Mid** | `src/mid/{parser,dispatcher,pudo,alias,dirstack,source,hist_expand,expand,prompt}.c` | Parse, builtins, orquestração, pudo cliente | Ativo (núcleo do shell) |
| **Back** | `src/back/README.md` apenas | Persistência/config futura | Placeholder consciente (early) |
| **Foundation** | `src/foundation/{env,process,job}.c` | env wrappers, fork/exec/wait, jobs | Ativo; `job.c` existe e falta na tabela de `architecture.md` |
| **Helper** | `src/pudod/` (binário `pudod` separado) | Elevação setuid / allow-list | Isolado do lib petrush; fora do quadro de 4 camadas no doc |

### Headers públicos (`include/petrush/`) e camada anotada

| Header | Camada (comentário / uso) |
|--------|---------------------------|
| `complete.h`, `highlight.h` | Front |
| `rc_trust.h` | Fisicamente Front; semanticamente checagem de trust (ver F2) |
| `alias.h`, `dispatcher.h`, `expand.h`, `hist_expand.h`, `parser.h`, `prompt.h`, `pudo.h`, `source.h`, `dirstack.h` | Mid |
| `env.h`, `process.h`, `job.h` | Foundation |
| `petrush.h` | Versão / nome (neutro) |

## 3. Grafo de dependência (includes internos)

Direção esperada (petrush early): **Front → Mid → Foundation**; Back ainda vazio; `pudod` não inclui `petrush/*`.

```
main.c (Front/composition)
  → parser, dispatcher, process, job, env, alias, complete,
    hist_expand, prompt, rc_trust, source, linenoise

front/complete.c → dispatcher, env, highlight, linenoise
front/highlight.c → (só highlight.h + libc)
front/rc_trust.c → (só rc_trust.h + libc)

mid/dispatcher.c → process, job, env, pudo, alias, dirstack, expand, source, linenoise
mid/pudo.c       → dispatcher, env
mid/source.c     → alias, dispatcher, rc_trust   *** Mid → header Front ***
mid/prompt.c     → env
mid/expand.c     → env
mid/hist_expand.c → linenoise                    *** Mid → UI lib ***
mid/{parser,alias,dirstack}.c → (próprios + libc)

foundation/process.c → env, parser.h             *** Foundation → tipos Mid ***
foundation/{env,job}.c → (próprios + libc)

pudod/* → só headers locais do diretório pudod
```

### Ciclos dentro do Mid

`dispatcher` ↔ `pudo` / `source` (includes mútuos de headers Mid). Aceitável na mesma camada; acoplamento alto do monólito de builtins, não é violação inter-camada.

## 4. Achados

Severidade: **HIGH** = viola CONTRACT §5 de forma clara; **MEDIUM** = fronteira inconsistente ou doc defasado com risco; **LOW** = dívida / polish; **OK** = conforme.

### F1 — HIGH — Mid importa linenoise (lib de UI)

| | |
|--|--|
| **Onde** | `src/mid/dispatcher.c` (`#include "linenoise.h"`, `linenoiseClearScreen` em `builtin_clear`); `src/mid/hist_expand.c` (`linenoiseHistoryLen` / `linenoiseHistoryGet`) |
| **Regra** | CONTRACT §5.1: serviço/Mid **MUST NOT** importar framework/lib de UI. Princípio Front: I/O de apresentação fica no Front. |
| **Impacto** | Mid acoplado ao vendor de line-editing; troca/mock de history e `clear` exige Mid. Contamina AUD-DEPS. |
| **Mitigação sugerida** | (a) `builtin_clear` chamar API Front (`petrush_ui_clear_screen`) implementada junto de linenoise; (b) `hist_expand` ler history via porta Foundation/Back (`petrush_history_get/len`) alimentada pelo Front no boot. Sem mudar comportamento nesta auditoria. |

### F2 — MEDIUM — Mid depende de unidade fisicamente Front (`rc_trust`)

| | |
|--|--|
| **Onde** | `src/mid/source.c` → `#include "petrush/rc_trust.h"`; impl em `src/front/rc_trust.c` |
| **Regra** | Dependência unidirecional: camada inferior não sobe. |
| **Leitura** | `petrush_rc_stat_ok` é política de trust (uid/mode), não apresentação. Classificação física em Front está errada; o include Mid→Front é sintoma. |
| **Mitigação sugerida** | Mover `rc_trust.c` para `src/foundation/` (ou Mid) e atualizar `architecture.md` + CMake. Preferível a inverter a dependência. |

### F3 — MEDIUM — Mid chama `fork` / `exec*` / `open` de processo

| | |
|--|--|
| **Onde** | `src/mid/dispatcher.c` `dispatch_pipeline_background` (`fork`, `open("/dev/null")`); `src/mid/pudo.c` (`execve`/`execvp` do helper); também `open` de redirs duplicado no Mid |
| **Regra** | AUD-ARCH do repo: fronteira “só main/platform chama libc de processo”. Foundation já concentra `execute_external` / `execute_pipeline`. |
| **Contexto** | Comentário UX-23: bg job propositalmente **fora** de `execute_external`/`execute_pipeline`. `pudo` exec no cliente após sanitização é desenho de segurança (helper separado). |
| **Mitigação sugerida** | Documentar exceções nomeadas em `architecture.md` (bg spawn + pudo client exec). Opcional futuro: `petrush_spawn_background()` em Foundation. Não bloquear early. |

### F4 — MEDIUM — Foundation inclui tipos do parser (Mid)

| | |
|--|--|
| **Onde** | `include/petrush/process.h` → `#include "petrush/parser.h"` |
| **Regra** | Foundation não deveria conhecer AST de aplicação; idealmente tipos compartilhados em contrato neutro. |
| **Contexto** | `process.h` documenta de propósito **não** incluir `dispatcher.h` (bom). O AST é o “DTO” do shell. |
| **Mitigação sugerida** | Aceitar no porte early **com nota no architecture.md**, ou extrair `petrush_cmd_t` / pipeline para header neutro (`petrush/cmd.h`) sem lógica Mid. |

### F5 — MEDIUM — Drift de `docs/architecture.md` e READMEs de pasta

| Lacuna | Detalhe |
|--------|---------|
| Foundation incompleta | Tabela lista `env.c, process.c`; omite `job.c` (UX-23, ativo no CMake) |
| Mid incompleto | Omite `alias`, `dirstack`, `source`, `hist_expand`, `expand`, `prompt` (só reticências) |
| Front incompleto | Omite `rc_trust.c` |
| Helper | `src/pudod/` não entra no mapa de camadas (binário à parte) |
| `src/front/README.md` | Ainda diz “porte Solo”; canônico é **early** |
| Back | Placeholder OK e honesto |

**Mitigação:** atualizar `architecture.md` + README Front (DOC); não exige mover arquivos agora.

### F6 — LOW — DRY de redirs (`open`/`dup2`) Mid × Foundation

Lógica de aplicar redirecionamentos aparece em `dispatcher.c` e `process.c`. Rule of 3 ainda não obriga extrair (2 cópias), mas é dívida de fronteira: Mid reimplementa primitiva que Foundation já tem. Encaminhar também a AUD-QUALITY.

### F7 — OK — Direção geral respeitada

- Front (`complete`) → Mid (`dispatcher`) + Foundation (`env`): correto.
- Mid → Foundation (`process`, `job`, `env`): correto.
- Foundation **não** inclui headers Front.
- `pudod` não puxa Mid/Front.
- `main.c` como composition root na raiz de `src/`: documentado e aceitável no early.
- Back thin: alinhado a anti-over-engineering.

### F8 — OK — Adaptação de nomenclatura Back ≠ domínio universal

Nos princípios universais, Back = domínio. No petrush, Mid concentra parser/builtins (o “domínio do shell”) e Back reserva persistência. Isso está **explícito** em `docs/architecture.md` / `docs/standards.md`. Não é violação se o mapa local for a fonte de verdade; AUD-REPORT deve usar o mapa petrush, não o de Qt/web.

## 5. SOLID / DRY (lente arquitetural)

| Princípio | Avaliação curta |
|-----------|-----------------|
| **S** | Módulos Mid razoavelmente fatiados (parser ≠ expand ≠ pudo). `dispatcher.c` concentra muitos builtins (god-file relativo; AUD-QUALITY). |
| **O/D** | Pouca inversão real (C sem interfaces ricas). Hook `pipeline_child_hook_t` em `process.h` é o ponto DI consciente Mid↔Foundation. |
| **DRY** | F6 (redirs). History acessado via linenoise em Front e Mid (F1). |

## 6. Checklist CONTRACT §5.2 (adaptado C23)

| Item | Resultado |
|------|-----------|
| Unidade pertence a exatamente uma camada? | **Parcial** — `rc_trust` físico Front / uso Mid (F2); `hist_expand` Mid com API linenoise |
| Sem dependência para cima? | **Falha parcial** — F1 (UI), F2 (rc_trust) |
| Chamadas cross-layer via contrato/header? | **OK pragmático** — headers `include/petrush/`; sem include relativo `../` |
| Domínio livre de UI/DB? | **Falha parcial** — Mid+linenoise (F1); sem DB |

## 7. Score e veredito

| Dimensão | Nota (0–20) | Nota |
|----------|-------------|------|
| Separação física/lógica early | 16 | Pastas e CMake refletem Front/Mid/Foundation; Back placeholder honesto |
| Direção de includes | 12 | F1+F2 pesam; resto limpo |
| Alinhamento docs ↔ tree | 11 | architecture.md defasado (F5) |
| Fronteira de processo | 13 | Foundation correta; exceções Mid documentáveis (F3) |
| Isolamento pudod | 18 | Binário e headers próprios |
| **Total** | **70 / 100** | |

**Veredito:** **APROVADO COM RESSALVAS** para porte early. Não há distributed-monolith nem Mid→DB. Há acoplamento Mid↔linenoise e classificação errada de `rc_trust`, mais drift documental. Nenhuma mudança de produto aplicada nesta fatia.

**Não marca ✅** (só 🔍). ✅ fica para onda TST/AUD consolidada / AUD-REPORT.

## 8. Patches recomendados (ordem)

1. **DOC** — Atualizar `docs/architecture.md`: listar `job.c`, Mid completo, `rc_trust`, nota `pudod`, exceções F3/F4. Corrigir `src/front/README.md` (Solo → early).
2. **MOVE+DOC** — Reclassificar `rc_trust` para Foundation (ou Mid); ajustar CMake (F2).
3. **API** — Porta de history + clear sem `#include "linenoise.h"` no Mid (F1).
4. **Opcional** — `petrush_spawn_background` em Foundation; extrair `cmd` types neutros (F3/F4).
5. Encaminhar F6 e tamanho de `dispatcher.c` para **AUD-QUALITY** / **AUD-DEPS**.

## 9. Evidências (comandos / paths)

- Tree: `src/front|mid|back|foundation|pudod`, `include/petrush/*.h`
- Includes: varredura `#include "petrush/` e `linenoise` em `src/` + `include/`
- Processo: `fork`/`exec*`/`waitpid` em `foundation/process.c`, `foundation/job.c`, `mid/dispatcher.c`, `mid/pudo.c`, `pudod/pudod.c`
- Build list: `CMakeLists.txt` linhas das sources petrush/pudod
- Critério: `Projects/petrush/AUDITORIAS.md` § AUD-ARCH; vault `CONTRACT.md` §5

## 10. Relação com outros AUD-*

| ID | Relação |
|----|---------|
| AUD-DEPS | F1 (linenoise no Mid), ciclos Mid |
| AUD-QUALITY | F6, god-file dispatcher |
| AUD-SEC | pudo/pudod (fronteira de processo; não aprofundado aqui) |
| AUD-DISC | superfície REPL/pudo (entrada para ameaça) |
| AUD-REPORT | consolidar score 70 e patches 1–4 |

---

*Fim AUD-ARCH. Sem push. Sem alteração de código de produto.*
