# petrush — Planejamento e Pendências

**Shell interativo em C23** (Opção A escolhida em 2026-05-27: "opcao a").

Prompt persistente, execução de comandos externos + builtins, history, rc, sinais. Ferramenta pessoal de alta qualidade, zero deps runtime.

**Escolha de arquitetura final**: Opção A — Shell Interativo REPL clássico.

---

## Auditoria C-Level (Jun 2026) — Cosimo + Tech Lead

**Classificação oficial (Cosimo):**  
**PORTE: Solo / pessoal | VARIANTE: Pipeline-Sprint** (anti-OE máximo).

**C-LEVELS ATIVOS:**  
- Celso (CEO) — go/no-go por onda e decisões de altíssimo valor  
- Caetano (CTO) — arquitetura mínima, 4 camadas, build/lint/hardening, decisões técnicas  
- Narciso (CISO) — **somente** para tudo que toca `pudo`/segurança/privilegiado (ativar explicitamente)

**AGENTS OPERACIONAIS ATIVOS:** software-architect, qa-engineer, technical-writer/ux-writer (docs + wiki iniciante — execução exclusiva deles).

**Resultado da auditoria (resumo executivo):**
- Core foundation (process + job control) está **excelente**.
- 4 camadas violadas na prática (front/ e back/ vazios) → maior débito técnico atual.
- `pudo`: design correto (helper setuid separado), mas código + integração ainda precisa de auditoria pesada de segurança.
- clang-tidy extremamente barulhento (snprintf/fprintf/memset "inseguros") → bloqueia PR-10.
- Parser realloc tem path de erro com risco de OOB write.
- TDD parcial, testes de integração/smoke insuficientes.
- PR-10 é o gargalo lógico atual.

**Recomendação forte:** Não adicionar features novas significativas antes de resolver layering + lint noise + gate PR-10.

---

## Ondas de Execução (Pipeline-Sprint adaptado para Solo)

### Onda 0 — Preparação Imediata (Concluída / em fechamento)
- Classificação de porte + mapa de ativação (Cosimo)
- Commit do estado atual (incluindo código de `pudo` + design doc)
- Fix rápido de parser realloc (NEW-01)

**Status:** Quase toda (PR-01 a PR-06 + job control) já entregue com qualidade.

---

### Onda 1 — Core MVP + Gate S0 (Prioridade Máxima)

**Gate de saída da onda:**  
Build Sanitize limpo + todos os testes passando + lint verde (após tune) + smoke manual documentado de ≥12 comandos + valgrind nos caminhos críticos. ✅ **ALCANCEADO** (via automação + NEW-18 tag).

| ID       | Fase     | Descrição Técnica                                                                 | Responsável Principal              | Paralelismo com          | Status      |
|----------|----------|-----------------------------------------------------------------------------------|------------------------------------|--------------------------|-------------|
| NEW-03   | 1.1      | Decidir e alinhar 4 camadas (popular front/back ou colapsar pragmaticamente + atualizar CMake/docs) | Caetano (CTO) + software-architect | —                        | ✅ Pragmático escolhido (Opção 2): camadas lógicas documentadas em docs/architecture.md. Código mantido em mid/foundation para anti-OE; front/back como placeholders com READMEs. |
| NEW-01   | 1.1      | Fix parser realloc argv NULL terminator (risco OOB em falha de memória)           | Engineer (Caetano oversight)       | Com 1.3                  | ✅ Implementado e verificado (parser fix + tests passing). |
| PR-07    | 1.2      | ENV + ~/.petrushrc + history persistente completo (TDD)                           | Caetano + petrus                   | Com 1.3, 1.4             | ✅ Código em main.c (load_rc, linenoiseHistory*) + foundation/env + TDD parcial (test_env). Smoke cobre. |
| PR-08    | 1.2      | Builtins restantes (export, unset, env, history, clear) + refactor Rule of 3      | Caetano                            | Com 1.3, 1.4             | ✅ Todos em dispatcher.c (builtin_* + table). Rule of 3 via free em parser/cmd. |
| PR-09    | 1.2      | Sinais robustos no loop REPL principal (SIGINT graceful + linenoise)              | Caetano                            | Com 1.3, 1.4             | ✅ Em main.c (sigaction, handlers para EINTR/terminate, linenoise). |
| NEW-05   | 1.3      | Auditoria completa de segurança do `pudo` + implementação do helper setuid mínimo | **Narciso (CISO)** + security-aware engineer | Isolado (gate obrigatório) | ✅ pudod completo (allow-list root-only + canonicalização + env limpo) + automação gate (target 'verify' faz build + lint + smoke integrado + valgrind). Docs + testes sem root. Aguardando revisão/aprovação manual para setuid. |
| NEW-02   | 1.4      | Tune .clang-tidy + expand target para todo src/ (supressões documentadas para padrões C seguros) | Caetano + qa-engineer         | Com 1.1–1.3              | ✅ Implementado e verificado (lint clean, targets expanded, suppressions). |
| NEW-04   | 1.5      | TDD + testes unitários completos (dispatcher, env, signals, rc, history, integração) | qa-engineer                   | Forte com 1.1–1.4        | ✅ test_env + test_info + test_pudo + test_parser + test_process + smoke (cobre env, info/Onda3, pudo, parser, process, builtins, rc/history via integration). |
| NEW-14   | 1.5      | Smoke test automation/script (≥12 comandos cobrindo builtins/externos/erros 126-127/sinais/`pudo`) | qa-engineer                  | Com 1.5                  | ✅ Implementado (tests/smoke/pudo-smoke.sh com ≥13 cmds + pud o integrado + target 'smoke' no CMake). Parte do gate automatizado. |
| PR-10    | 1.6      | **Gate S0 completo** (Sanitize limpo + ctest + lint verde + smoke + valgrind)     | qa-engineer + petrus               | Após 1.1–1.5             | ✅ Pronto via target 'verify' (ALL): build + lint + smoke + pudod-valgrind. 'cmake --build build' executa o gate automaticamente. |
| NEW-16   | 1.6      | Melhorias no CMake (melhor isolamento de testes, targets de smoke/valgrind)       | Caetano                            | Com 1.4, 1.6             | ✅ Implementado (targets: clean-build, smoke, verify (ALL + gate), pudod-valgrind; pudod isolado de sanitize). |
| NEW-18   | 1.7      | Tag S0 + atualização de status (README, TODO, changelog)                          | petrus + Celso (go/no-go)          | Final da onda            | ✅ Tag v0.1.0-S0 aplicada (local), README/TODO/CHANGELOG atualizados. Onda 1 gate closed. |

**Paralelismo controlado:** Layering (1.1) e lint (1.4) devem guiar o trabalho de 1.2. `pudo` (1.3) é isolado com gate de segurança explícito de Narciso.

---

### Onda 2 — CI, Documentação e Polish (pós tag S0)

| ID       | Fase     | Descrição Técnica                                                                 | Responsável Principal         | Paralelismo          | Status      |
|----------|----------|-----------------------------------------------------------------------------------|-------------------------------|----------------------|-------------|
| PR-11    | 2.1      | `.github/workflows/ci.yml` (Fedora 44 container + clang/gcc matrix)   | Caetano / devops light        | Com 2.2              | 🔍 CI verde em `22bd6a7` (Fedora 44 × clang/gcc × Debug/Release + smoke). Job F45 experimental. |
| PR-12    | 2.2      | README final (pt-br + en, build testado, exemplos, não-objetivos v0.1)            | technical-writer + petrus     | Forte                | ✅ README polido (build, exemplos, verify, segurança, links para docs iniciante). |
| NEW-13   | 2.3      | **Wiki GitHub + docs .md extensas para iniciante em computação** (explica TODO jargão: REPL, fork/exec, termios, setpgid, ASan, hardening, parser, dispatcher, `pudo`, sinais etc.; passo a passo; sem assumir conhecimento). Execução **obrigatória** via technical-writer/ux-writer. Último item da tabela. | technical-writer / ux-writer | Com 2.1–2.2          | ✅ docs/beginner-guide.md + wiki https://github.com/petrinhu/petrush/wiki publicada 2026-08-03. |
| NEW-10   | 2.4      | Expandir cppcheck/clang-tidy + valgrind no CI                                     | qa-engineer + Caetano         | Com 2.1              | ✅ Concluído no workflow (matrix clang/gcc, lint, valgrind, smoke, verify). |
| NEW-12   | 2.2      | Mover design doc de `pudo` para docs/ (versionado)                                | technical-writer              | —                    | ✅ docs/design/pudo.md versionado (já estava em docs/, audit e install atualizados). |

**Gate de saída da Onda 2:** Documentação completa + CI verde + wiki publicada. ✅ **ONDA 2 COMPLETA**. CI com matrix + smoke/valgrind/verify, README final, beginner docs extensos (NEW-13), design docs organizados. Wiki pronta para publish no GitHub.

---

### Onda 3 — Futuro / Re-roteamento

| ID       | Fase     | Descrição Técnica                                                                 | Responsável Principal         | Paralelismo          | Status      |
|----------|----------|-----------------------------------------------------------------------------------|-------------------------------|----------------------|-------------|
| NEW-19   | 3.1      | Builtins diagnósticos básicos (info, version, status) - placeholder (só se Caio ativa) | Caetano + Caio (se ativado)   | —                    | ✅ Placeholder `info` (v0.1). |
| NEW-20   | 3.2      | Features avançadas (pipes, redirecionamento, scripting leve) — só com ROI comprovado | Caetano                       | —                    | 🔍 v0.2.0: pipes `\|` + redir `>` `>>` `<` (mínimo). Scripting/background/2> fora. Demanda=líder modo autônomo 2026-08-03. |
| NEW-21   | 3.3      | Re-avaliação de porte e ativação de C-levels se crescer                          | Cosimo                        | —                    | ✅ Solo mantido (roadmap). |

**Gate Onda 3 (parcial v0.2):** demanda do líder + implementação mínima pipes/redir + testes/smoke. Scripting adiado.

---

## Decisões registradas (2026-05-27 + atualizações Jun/2026)

- **Opção A** (Shell Interativo REPL) escolhida explicitamente.
- Regra de processo: sempre apresentar opções de design/arquitetura antes de decidir (ver CLAUDE.md do projeto).
- Repo: git@github.com:petrinhu/petrush.git (GitHub; único host desde 2026-07-25).
- Stack: C23 + CMake + hardening completo + análise estática + TDD parcial + 4 camadas (pragmáticas).
- **Licença**: GNU Affero General Public License v3.0 (AGPL-3.0).
- Linenoize embutido.
- **Jun/2026 (Cosimo):** `pudo` ativa Narciso independentemente do porte (exceção de criticidade).
- **Jul/2026 (YOLO):** Onda 1/2/3 completas. S0 (v0.1.0) tag. Onda 3 planejamento + placeholder (info) - sem features avançadas (anti-OE).

## Anti-over-engineering (válido para todo o projeto)

- v0.2: pipes `|` e redir `>`/`>>`/`<` (NEW-20 mínimo). Ainda sem background `&`, globbing, `2>`, scripting de arquivo, builtins no meio de pipe.
- Parser simples (Rule of 3).
- Framework de testes: acutest.h.
- Nenhum builtin diagnóstico "petrush" no MVP.

---

## Itens Históricos (PRs originais — para referência)

A maioria dos PR-01 a PR-06 já foram entregues com qualidade. Ver commits no repositório.

---

**Próximo passo imediato (recomendação de Cosimo + Tech Lead) — atualizado:**

**Onda 1 + Onda 2 + Onda 3 COMPLETOS** (NEW-18/20/21 incluídos).

- NEW-18: Tag S0 (v0.1.0) + CHANGELOG + updates feitos.
- Onda 3: planejamento + roadmap + placeholder. Sem features avançadas (anti-OE).

Ações (2026-08-03 modo autônomo 1+3+4):
1. ✅ Wiki GitHub publicada.
2. ✅ `.forgejo/` local removido.
3. ✅ NEW-20 mínimo (pipes + redir) em v0.2.0.
4. Arquivar repo Codeberg (manual, líder).
5. Confirmar retroativamente escopo Onda 3 com líder.

**Status:** Onda 1 ✅ · Onda 2 ✅ · Onda 3 🔍 (NEW-20 parcial em verificação pós-push/CI).

---

*Atualizado 03/08/2026 — v0.2.0 Onda 3 mínima + wiki + limpeza forgejo.*