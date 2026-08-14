# petrush - Planejamento e Pendências

**Shell interativo em C23** (Opção A, 2026-05-27): prompt persistente, comandos externos + builtins, history, rc, sinais. Zero deps runtime.

Tabela canônica de 9 colunas (`tab_pendencias` v1.0.2). Ordem das linhas = ordem de execução. `Onda` agrupa o que pode rodar em paralelo.

## Gate Cosimo (2026-08-14, `/bigtech` + `--reorder`)

- **Porte:** early. **Variante:** Pipeline-Sprint. Marcador: `.bigtech-porte`.
- **C-levels desta auditoria:** Celso (aceite no fim), Caetano (técnico), Narciso (só `pudo`/`pudod`).
- **Dormentes:** Capitolino, Camilo, Cosmo, Cândido, Caio, Confúcio, Cícero, Cláudio.
- **Ops:** security-engineer (relatório em `/var/tmp/petrush-audit-20260814/security.md`). Caetano/QA/Narciso em persona: 402 no spawn; recorte fechado na thread e gravado no mesmo scratch.
- **TDD:** hook `.claude/tdd-guard.json` ausente (T1 fora da tabela).
- **Setuid:** continua gate humano (`DEPLOY_CHECKLIST`). Esta passada **não** endossa 4755.

Manuais: [`TESTES.md`](TESTES.md) · [`AUDITORIAS.md`](AUDITORIAS.md). Plano UX: [`docs/plan-implementacao-roi.md`](docs/plan-implementacao-roi.md). Snapshot AppSec: `/var/tmp/petrush-audit-20260814/`.

| ID | Onda | Grupo | Descrição Técnica | Prioridade | Pré-requisito | Dificuldade | Status | Estado Auditado |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| NEW-03 | — | Fundação | Decidir 4 camadas: pragmático (Opção 2): lógicas em `docs/architecture.md`; código em mid/foundation; front/back placeholders. | Alta | — | Média | ✅ Concluído | — |
| NEW-01 | — | Fundação | Fix parser realloc argv NULL terminator (risco OOB em falha de memória). | Alta | — | Baixa | ✅ Concluído | — |
| PR-07 | — | MVP | ENV + `~/.petrushrc` + history persistente (`load_rc`, linenoiseHistory) + TDD parcial (`test_env`). | Alta | NEW-03 | Média | ✅ Concluído | — |
| PR-08 | — | MVP | Builtins restantes (`export`, `unset`, `env`, `history`, `clear`) + Rule of 3. | Alta | PR-07 | Média | ✅ Concluído | — |
| PR-09 | — | MVP | Sinais robustos no REPL (SIGINT graceful + linenoise + EINTR). | Alta | PR-07 | Média | ✅ Concluído | — |
| NEW-05 | — | Segurança | Auditoria `pudo` + helper setuid mínimo (`pudod`, allow-list, env limpo). Setuid em sistema ainda é do líder (`DEPLOY_CHECKLIST`). Snapshot 2026-08-14: residual `unsetenv` no pai, path relativo, example amplo, truncamento de argv. | Alta | — | Alta | ✅ Concluído | ⚠ |
| NEW-02 | — | Qualidade | Tune `.clang-tidy` + expand target para todo `src/` (supressões C seguro documentadas). | Média | NEW-03 | Média | ✅ Concluído | — |
| NEW-04 | — | Testes | TDD + unitários (dispatcher, env, signals, rc, history, integração). | Alta | PR-08, PR-09 | Alta | ✅ Concluído | — |
| NEW-14 | — | Testes | Smoke ≥12 comandos (builtins/externos/erros 126-127/sinais/`pudo`); target CMake `smoke`. | Alta | NEW-04 | Média | ✅ Concluído | — |
| PR-10 | — | Gate | Gate S0: Sanitize + ctest + lint + smoke + valgrind (`verify` ALL). | Alta | NEW-02, NEW-14, NEW-05 | Alta | ✅ Concluído | — |
| NEW-16 | — | Build | CMake: isolamento de testes, targets `smoke` / `verify` / `pudod-valgrind`. | Média | PR-10 | Média | ✅ Concluído | — |
| NEW-18 | — | Release | Tag S0 `v0.1.0` + README/TODO/CHANGELOG. | Alta | PR-10 | Baixa | ✅ Concluído | — |
| PR-12 | — | Docs | README final (pt-br + en, build, exemplos, não-objetivos v0.1). | Média | NEW-18 | Baixa | ✅ Concluído | — |
| NEW-10 | — | CI | Expandir cppcheck/clang-tidy + valgrind no workflow (entregue em paralelo com PR-11). | Média | NEW-18 | Média | ✅ Concluído | — |
| NEW-12 | — | Docs | Design doc de `pudo` versionado em `docs/design/pudo.md`. | Baixa | NEW-05 | Baixa | ✅ Concluído | — |
| NEW-13 | — | Docs | Wiki GitHub + `docs/beginner-guide.md` para iniciante (jargão, passo a passo). Via technical-writer/ux-writer. Publicada 2026-08-03. | Média | PR-12 | Alta | ✅ Concluído | — |
| NEW-19 | — | Roadmap | Builtin diagnóstico `info` (placeholder v0.1). | Baixa | NEW-18 | Baixa | ✅ Concluído | — |
| NEW-21 | — | Processo | Re-avaliação de porte. Early/Pipeline-Sprint mantido. | Baixa | NEW-18 | Baixa | ✅ Concluído | — |
| CB-01 | — | Ops | Host canônico = só GitHub. Codeberg/Forgejo fora de uso (política 2026-07-25). Não há pendência de arquivo no site. | Baixa | — | Baixa | ✅ Concluído | — |
| UX-15 | W1 | Shell | Prompt escapes `\w` `\u` `\h` `\n` `\$` `\\` em `PETRUSH_PS1`. `prompt_render` + `test_prompt` (10 casos). | Alta | — | Baixa | 🔍 Pendente verificação | — |
| PR-11 | W1 | CI | `.github/workflows/ci.yml` (Fedora 44 × clang/gcc × Debug/Release + smoke; F45 experimental). Espelho remoto. Builds pesados no container local `fedora:44`. | Alta | — | Média | 🔍 Pendente verificação | — |
| NEW-20 | W1 | Shell | Pipes `\|` + redir `>` `>>` `<` (mínimo v0.2.0). Scripting/background/`2>` fora (UX-16/UX-22/UX-23). | Alta | — | Média | 🔍 Pendente verificação | — |
| SEC-01 | W1 | Segurança | Remover `unsetenv` no processo `petrush` após `pudo` (`src/mid/pudo.c` `sanitize_environment` / `pudo_sanitize_environment`). Confiar só em `build_clean_envp` + sanitize no `pudod`. Residual do `pudo-audit.md` (claim “sem mutação persistente” é falso). | Alta | — | Baixa | ⏳ Pendente | — |
| UX-16 | W2 | Shell | Redirecionamento de stderr: `2>` `2>>` `2>&1` `&>`. | Alta | UX-15 | Média | ⏳ Pendente | — |
| NEW-22 | W2 | Processo | Confirmar retroativamente o escopo da Onda 3 (NEW-20 mínimo) com o líder. | Baixa | NEW-20 | Baixa | ⏳ Pendente | — |
| SEC-02 | W2 | Segurança | Em Release/install, path do `pudod` só absoluto (`PUDOD_INSTALL_PATH` / `/usr/local/libexec/petrush-pudod`). Fallbacks `build/pudod`, `./pudod` e `access("pudod")` só em debug. | Alta | — | Média | ⏳ Pendente | — |
| SEC-03 | W2 | Segurança | Encolher `src/pudod/pudo.allow.example` a 2–3 comandos inocentes (`id`, `whoami`, `true`). Tirar apt/dnf/systemctl/passwd/cat do exemplo ativo. | Média | — | Baixa | ⏳ Pendente | — |
| SEC-04 | W2 | Segurança | Fail closed se `argc` exceder `MAX_ARGS` / `pudod_argv[128]` (hoje trunca em silêncio). | Média | — | Baixa | ⏳ Pendente | — |
| SEC-05 | W2 | Segurança | Load da allow-list: se `realpath` falhar, não aceitar o literal (`pudod.c` ~L142–148). | Média | — | Baixa | ⏳ Pendente | — |
| DOC-01 | W2 | Docs | Atualizar `docs/architecture.md`: Front já tem `complete.c`; prosa “porte Solo” é informal (piso early). | Baixa | — | Baixa | ⏳ Pendente | — |
| UX-17 | W3 | Shell | Lista sequencial `;` (`a; b` sempre roda `b`). | Média | UX-16 | Baixa | ⏳ Pendente | — |
| UX-18 | W4 | Shell | Glob simples `*` `?` só em tokens unquoted. | Média | UX-17 | Média | ⏳ Pendente | — |
| UX-19 | W5 | Shell | Builtins no pipe (subshell-like). | Média | UX-18 | Média | ⏳ Pendente | — |
| UX-20 | W6 | Shell | Ctrl-R history search. Patch mínimo no linenoise; se a API bloquear, documentar degradação (setas + hints + `!!`) e seguir. | Média | UX-19 | Alta | ⏳ Pendente | — |
| UX-21 | W7 | Shell | Syntax highlight mínimo (aspas não fechadas / token grosso; não full highlighter). | Baixa | UX-20 | Alta | ⏳ Pendente | — |
| UX-22 | W8 | Shell | `source` / `.`: rodar arquivo linha a linha. | Baixa | UX-21 | Média | ⏳ Pendente | — |
| UX-23 | W9 | Shell | Background `&` + job control mínimo. | Baixa | UX-22 | Alta | ⏳ Pendente | — |
| TST-T2 | W10 | Testes | Análise estática (cppcheck + clang-tidy). Ver `TESTES.md`. | Alta | UX-23 | Média | ⏳ Pendente | — |
| TST-T3 | W10 | Testes | Fuzzing de inputs (parser / expand / prompt). Ver `TESTES.md`. | Média | UX-23 | Alta | ⏳ Pendente | — |
| TST-T4 | W10 | Testes | Análise dinâmica de memória (ASan + UBSan + valgrind). Ver `TESTES.md`. | Alta | UX-23 | Média | ⏳ Pendente | — |
| TST-T5 | W10 | Testes | Scanning de dependências (trivy/grype). Ver `TESTES.md`. | Média | UX-23 | Baixa | ⏳ Pendente | — |
| TST-T7 | W10 | Testes | Scanning de binário (checksec / hardening-check). Ver `TESTES.md`. | Média | UX-23 | Baixa | ⏳ Pendente | — |
| TST-T8 | W10 | Testes | Verificação de secrets (gitleaks/trufflehog). Ver `TESTES.md`. | Alta | UX-23 | Baixa | ⏳ Pendente | — |
| TST-T12 | W10 | Testes | Busca de CVEs nas deps/toolchain. Ver `TESTES.md`. | Média | UX-23 | Baixa | ⏳ Pendente | — |
| TST-T14 | W10 | Testes | Integração fim-a-fim (smoke). Ver `TESTES.md`. | Alta | UX-23 | Média | ⏳ Pendente | — |
| TST-T15 | W10 | Testes | Pré-CI no container local Fedora 44 (mesma imagem do `ci.yml`): build + lint + smoke + valgrind/`verify`. GHA é o espelho remoto, não o caminho pesado. Ver `TESTES.md`. | Alta | UX-23 | Baixa | ⏳ Pendente | — |
| AUD-DISC | W11 | Auditoria | Descoberta e modelagem de ameaça. Ver `AUDITORIAS.md`. | Alta | TST-T2, TST-T4, TST-T8, TST-T14, TST-T15 | Média | ⏳ Pendente | — |
| AUD-ARCH | W11 | Auditoria | Arquitetura e 4 camadas. Ver `AUDITORIAS.md`. | Alta | TST-T2, TST-T4, TST-T8, TST-T14, TST-T15 | Média | ⏳ Pendente | — |
| AUD-SEC | W11 | Auditoria | Segurança (memory safety, secrets, `pudod`, binário). Ver `AUDITORIAS.md`. | Alta | TST-T2, TST-T4, TST-T7, TST-T8, TST-T12 | Alta | ⏳ Pendente | — |
| AUD-QUALITY | W11 | Auditoria | Qualidade de código (complexidade, dead code, Rule of 3). Ver `AUDITORIAS.md`. | Média | TST-T2, TST-T14 | Média | ⏳ Pendente | — |
| AUD-COV | W11 | Auditoria | Cobertura nos módulos críticos. Ver `AUDITORIAS.md`. | Média | TST-T14 | Média | ⏳ Pendente | — |
| AUD-DEPS | W11 | Auditoria | Dependências e acoplamento (linenoise + camadas). Ver `AUDITORIAS.md`. | Baixa | TST-T5, TST-T12 | Baixa | ⏳ Pendente | — |
| AUD-LANG | W11 | Auditoria | Idioms C23. Ver `AUDITORIAS.md`. | Baixa | TST-T2 | Baixa | ⏳ Pendente | — |
| AUD-REPORT | W12 | Auditoria | Relatório final (score 0-100, patches). Ver `AUDITORIAS.md`. | Alta | AUD-DISC, AUD-ARCH, AUD-SEC, AUD-QUALITY, AUD-COV, AUD-DEPS, AUD-LANG | Média | ⏳ Pendente | — |
| NEW-23 | W13 | Docs | Atualizar Wiki GitHub + `docs/beginner-guide.md` após a próxima tag (`v0.4` se UX-12..18; `v0.5` se até UX-23). Execução via technical-writer/ux-writer. | Baixa | AUD-REPORT | Média | ⏳ Pendente | — |

## INBOX (descobertas não priorizadas)

*(vazia.)*

## Notas de ordenação (WSJF qualitativo, early)

- Fundação e gates já entregues ficam no topo com `Onda —` (histórico; IDs e Status preservados).
- **W1** fecha o fogo: UX-15 (WIP, só tree), PR-11 e NEW-20 (`🔍`), mais SEC-01 (bug vivo: `unsetenv` no pai).
- **W2** mistura UX-16 + cluster pré-setuid (SEC-02..05) + DOC-01 + NEW-22. Não bloqueia a fila ROI.
- **W3..W9** = UX-17..23 (ROI). Gate entre fatias: container local Fedora 44, não GHA pesado.
- **W10** TST-\* (T1 nunca é item). Snapshot 14/08 não fecha esses IDs (ferramentas canônicas não rodaram).
- **W11..W12** AUD-\*. **W13** NEW-23 wiki pós-tag.
- Fora: UX-24/25/26; CVE-2025-9810 (já `fchmod` no vendor); setuid no host; reabrir Opção A.
- Exec-without-fork do `pudo` = residual consciente (documentar se doer), não item novo.

*Atualizado 14/08/2026: `/bigtech` auditoria snapshot + itens SEC-01..05 / DOC-01.*
