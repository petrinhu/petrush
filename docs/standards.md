# Standards do vault — aplicáveis ao petrush

> **Não duplicar** o texto dos manuais. A cópia canônica vive na **raiz do vault**
> `projetos_claudebrain/`. Este arquivo só **inclui** (aponta + adapta) o que vale
> para o shell C23.

## Caminhos canônicos (esta máquina)

| Manual | Caminho absoluto | Relativo a partir de `Projects/petrush/` |
|--------|------------------|------------------------------------------|
| **CONTRACT** | `/home/petrus/IDrive/Documentos/projetos_claudebrain/CONTRACT.md` | `../../CONTRACT.md` |
| **AGILE** | `/home/petrus/IDrive/Documentos/projetos_claudebrain/AGILE.md` | `../../AGILE.md` |
| **TESTES** | `/home/petrus/IDrive/Documentos/projetos_claudebrain/TESTES.md` | `../../TESTES.md` |
| **AUDITORIAS** | `/home/petrus/IDrive/Documentos/projetos_claudebrain/AUDITORIAS.md` | `../../AUDITORIAS.md` |
| **DEPLOY_CHECKLIST** | `/home/petrus/IDrive/Documentos/projetos_claudebrain/DEPLOY_CHECKLIST.md` | `../../DEPLOY_CHECKLIST.md` |

Agente **MUST** ler (ou reabrir) o manual relevante **antes** da tarefa correspondente:
código → CONTRACT + TESTES; planejamento → AGILE; review de qualidade → AUDITORIAS;
instalação setuid/irreversível → DEPLOY_CHECKLIST.

## Hierarquia de autoridade (petrush)

1. **Decisão explícita do líder** (AskUserQuestion / ordem na sessão)  
2. **CONTRACT.md** (MUST/MUST NOT RFC 2119)  
3. **TESTES.md** + **AUDITORIAS.md** (como provar qualidade)  
4. **AGILE.md** (como decompor e entregar valor)  
5. **DEPLOY_CHECKLIST.md** (só operações irreversíveis)  
6. **CLAUDE.md deste repo** (especificidades petrush: REPL, C23, TDD em tudo, AGPL, GitHub-only)  
7. Docs locais `docs/*` (architecture, security, beginner)

Conflito: sobe a hierarquia; se ainda ambíguo → perguntar ao líder.

## Mapa: o que se aplica a este projeto (C23 shell, sem Qt/web)

### CONTRACT (sempre)

- Ler contexto existente antes de editar (sec. 1).  
- Camadas Front / Mid / Back / Foundation (sec. 5) — no petrush: pragmáticas
  (`docs/architecture.md`); mid = parser/dispatcher/pudo; foundation = process/env.  
- Clean code, segurança (sem secrets, validação de input), git Conventional Commits.  
- **§11 Testing & Audit Mandate** → executa-se via TESTES.md adaptado abaixo.  
- C: preferir C23 seguro; sem `gets`, sem engolir erros; RAII mental (free paths).

### AGILE (planejamento e entrega)

- Decompor em Épico → História (Como/quero/para) → Tarefa.  
- Critérios Given-When-Then viram **testes TDD** (red primeiro).  
- DoD de história: código + testes passando + doc mínima + demonstrável + sem dívida crítica.  
- TODO.md = Product Backlog vivo; status da skill `tab_pendencias`.  
- Preferir entrega pequena e funcionando (incremento) a plano rígido.

### TESTES (obrigatório ao escrever código)

Alinhado a **TDD em tudo** (`CLAUDE.md`):

| Gate vault | Adaptação petrush |
|------------|-------------------|
| T1 unitários | **acutest** em `tests/test_*.c` — **red antes do green** |
| T2 estático | `cppcheck` + `clang-tidy` (targets CMake `lint`) |
| T4 memória | build **Sanitize** (ASan/UBSan); valgrind em `pudod` quando aplicável |
| T8 secrets | não commitar tokens; smoke **não** dumpa `env` full |
| T15 pré-CI | `cmake --build build --target verify` (e/ou `check` + `smoke`) **antes** de push de onda |
| T3 fuzz / T6 API / T10 SQL | N/A no MVP (sem rede app / sem SQL) — reavaliar se o escopo crescer |

**Todo commit de comportamento:** build limpo + testes relevantes.  
**Feature completa:** T1 + T2 + T4 (sanitize ou smoke com paths críticos) + smoke.  
**Release/tag:** `verify` + checklist §11 do CONTRACT (o que for aplicável).

### AUDITORIAS

- A2 camadas / A5 estático+dinâmico / A6 cobertura de paths críticos / A10 relatório quando o líder pedir auditoria.  
- Qt/MySQL/Web checklists do manual: **fora de escopo** do petrush salvo menção explícita.

### DEPLOY_CHECKLIST

petrush não é SaaS com blue-green. O que **conta como irreversível / privilégio**:

- Instalar `pudod` com **setuid** (`docs/security/pudod-install.md`).  
- Qualquer escrita em `/etc/petrush/` allow-list root-only.  

Antes disso: percorrer DEPLOY_CHECKLIST (backup/estado, segurança, rollback “como reverter setuid”), + auditoria de segurança do `pudo`, + aprovação explícita do líder.  
**CMake MUST NOT** aplicar setuid automaticamente (já é política do projeto).

## Inclusão no contexto do agente

No início de tarefa de **código** neste repo, o agente deve ter estes manuais no mapa mental
(e abrir o arquivo quando for aplicar regra específica — não “resumir de memória”).

Wiki GitHub e `docs/beginner-guide.md` são didáticos; **não** substituem CONTRACT/TESTES.
