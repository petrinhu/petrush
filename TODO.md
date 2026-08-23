# petrush - Planejamento e Pendências

**Shell interativo em C23** (Opção A, 2026-05-27): prompt persistente, comandos externos + builtins, history, rc, sinais. Zero deps runtime.

Tabela canônica de 9 colunas (`tab_pendencias` v1.0.2). Ordem das linhas = ordem de execução. `Onda` agrupa o que pode rodar em paralelo.

## Gate Cosimo (2026-08-14, `/bigtech` + `--reorder`)

- **Porte:** early. **Variante:** Pipeline-Sprint. Marcador: `.bigtech-porte`.
- **C-levels desta auditoria:** Celso (aceite no fim), Caetano (técnico), Narciso (só `pudo`/`pudod`).
- **Dormentes (14/08):** Capitolino, Camilo, Cosmo, Cândido, Caio, Confúcio, Cícero, Cláudio.
- **Ops:** security-engineer (relatório em `/var/tmp/petrush-audit-20260814/security.md`). Caetano/QA/Narciso em persona: 402 no spawn; recorte fechado na thread e gravado no mesmo scratch.
- **TDD:** hook `.claude/tdd-guard.json` ausente (T1 fora da tabela).
- **Setuid:** continua gate humano (`DEPLOY_CHECKLIST`). Esta passada **não** endossa 4755.

**Reorder 22/08/2026:** Cosmo (COO) ativo para cadência. Porte early + Pipeline-Sprint inalterado.

Manuais: [`TESTES.md`](TESTES.md) · [`AUDITORIAS.md`](AUDITORIAS.md). Plano UX: [`docs/plan-implementacao-roi.md`](docs/plan-implementacao-roi.md). Snapshot AppSec: `/var/tmp/petrush-audit-20260814/`.

## Notas de ordenação (WSJF qualitativo, early)

- Fundação e gates já entregues ficam no topo com `Onda —` (histórico; IDs e Status preservados).
- Topo por `Pré-requisito` primeiro; WSJF só desempata no mesmo nível. WSJF = (valor + urgência + redução de risco) / Job Size. Fundação e one-way-door sobem. Porte early: Alta/Média/Baixa, sem tabela Fibonacci SAFe.
- Mesma Onda = paralelizável (sem dependência interna). Early é um assento: serializar na prática, não inflar WIP.
- **W1** fecha o fogo já em 🔍: SEC-01 (bug vivo `unsetenv` no pai), UX-15 (WIP, só tree), NEW-20, PR-11.
- **W2** duas trilhas: UX-16 (ROI 🔍) + RES-* (pesquisa já feita, 🔍) + fundação pudo SEC-02..05 + DOC-01. NEW-22 no fim da onda (⏳, confirmar com líder; líder AFK); não trava W3.
- **W3** UX-17 (ROI 🔍) + SEC-06 (prereq SEC-05) + cluster REPL SEC-08/09/10. SEC-09 é irmão de FEAT-NOCLOBBER (mesmo patch possível; não duplicar).
- **W4** UX-18 (ROI 🔍) + SEC-07 (TOCTOU, prereq SEC-06).
- **W5..W9** UX-19..23 (fila ROI intacta, sem inverter entre si).
- **W10..W13** FEAT-* depois de UX-23 e antes de TST-*. W10 BANG+TRUE; W11 NOCLOBBER (prereq SEC-09) + UMASK; W12 READ+PARAM; W13 FEAT-TEST.
- **W14** TST-* (T1 nunca é item). Pré-req = FEAT-TEST, não UX-23.
- **W15** AUD-* (exceto relatório). **W16** AUD-REPORT. **W17** NEW-23 wiki pós-tag, último.
- Fora: UX-24/25/26; CVE-2025-9810 (já `fchmod` no vendor); setuid no host; reabrir Opção A.
- Exec-without-fork do `pudo` = residual consciente (documentar se doer), não item novo.

*Atualizado 22/08/2026: Cosmo (COO) reorder topológico + WSJF qualitativo (cadeia SEC-05→06→07 em ondas distintas; FEAT antes de TST).*

## INBOX (descobertas não priorizadas)

*(vazia.)*

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
| SEC-01 | W1 | Segurança | Remover `unsetenv` no processo `petrush` após `pudo`. Confiar só em `build_clean_envp` + sanitize no `pudod`. | Alta | — | Baixa | 🔍 Pendente verificação | — |
| UX-15 | W1 | Shell | Prompt escapes `\w` `\u` `\h` `\n` `\$` `\\` em `PETRUSH_PS1`. `prompt_render` + `test_prompt` (10 casos). | Alta | — | Baixa | 🔍 Pendente verificação | — |
| NEW-20 | W1 | Shell | Pipes `\|` + redir `>` `>>` `<` (mínimo v0.2.0). Scripting/background/`2>` fora (UX-16/UX-22/UX-23). | Alta | — | Média | 🔍 Pendente verificação | — |
| PR-11 | W1 | CI | `.github/workflows/ci.yml` (Fedora 44 × clang/gcc × Debug/Release + smoke; F45 experimental). Espelho remoto. Builds pesados no container local `fedora:44`. | Alta | — | Média | 🔍 Pendente verificação | — |
| UX-16 | W2 | Shell | Redirecionamento de stderr: `2>` `2>>` `2>&1` `&>`. | Alta | UX-15 | Média | 🔍 Pendente verificação | — |
| RES-SUDO-01 | W2 | Pesquisa | Mapa defensivo sudo (1980–2026) → pudo/pudod, 20 classes, boundary A/B. Artefato: [`docs/memory/sudo-pudo-riscos.md`](docs/memory/sudo-pudo-riscos.md). | Alta | — | Baixa | 🔍 Pendente verificação | — |
| RES-SH-SEC | W2 | Pesquisa | Superfície de risco REPL unpriv vs 10 shells. Artefato: [`docs/memory/shells-seguranca.md`](docs/memory/shells-seguranca.md). | Alta | — | Baixa | 🔍 Pendente verificação | — |
| RES-SH-01 | W2 | Pesquisa | Inventário Feature × 10 shells × petrush. Artefato: [`docs/memory/shells-funcoes.md`](docs/memory/shells-funcoes.md). Cap FEAT = julgamento Caetano. | Alta | — | Baixa | 🔍 Pendente verificação | — |
| SEC-02 | W2 | Segurança | Em Release/install, path do `pudod` só absoluto (`PUDOD_INSTALL_PATH` / `/usr/local/libexec/petrush-pudod`). Fallbacks `build/pudod`, `./pudod` e `access("pudod")` só em debug. | Alta | — | Média | 🔍 Pendente verificação | — |
| SEC-05 | W2 | Segurança | Load da allow-list: se `realpath` falhar, não aceitar o literal (`pudod.c` ~L142–148). | Média | — | Baixa | 🔍 Pendente verificação | — |
| SEC-04 | W2 | Segurança | Fail closed se `argc` exceder `MAX_ARGS` / `pudod_argv[128]` (hoje trunca em silêncio). | Média | — | Baixa | 🔍 Pendente verificação | — |
| SEC-03 | W2 | Segurança | Encolher `src/pudod/pudo.allow.example` a 2–3 comandos inocentes (`id`, `whoami`, `true`). Tirar apt/dnf/systemctl/passwd/cat do exemplo ativo. | Média | — | Baixa | 🔍 Pendente verificação | — |
| DOC-01 | W2 | Docs | Atualizar `docs/architecture.md`: Front já tem `complete.c`; prosa “porte Solo” é informal (piso early). | Baixa | — | Baixa | 🔍 Pendente verificação | — |
| NEW-22 | W2 | Processo | Confirmar retroativamente o escopo da Onda 3 (NEW-20 mínimo) com o líder. Confirmado 2026-08-22: líder ordenou drenar a tabela até o fim; NEW-20 já em 🔍. | Baixa | NEW-20 | Baixa | 🔍 Pendente verificação | — |
| UX-17 | W3 | Shell | Lista sequencial `;` (`a; b` sempre roda `b`). | Média | UX-16 | Baixa | 🔍 Pendente verificação | — |
| SEC-06 | W3 | Segurança | `pudod` exige `st_uid==0` e bit de exec no alvo (`pudod.c` fstat). Ver [`docs/memory/sudo-pudo-riscos.md`](docs/memory/sudo-pudo-riscos.md). | Alta | SEC-05 | Média | 🔍 Pendente verificação | — |
| SEC-09 | W3 | Segurança | `>`/`>>` sem `O_EXCL` em `process.c` e `dispatcher.c` (irmão FEAT-NOCLOBBER; o patch pode ser o mesmo, sem duplicar). Ver [`docs/memory/shells-seguranca.md`](docs/memory/shells-seguranca.md). | Média | — | Baixa | 🔍 Pendente verificação | — |
| SEC-08 | W3 | Segurança | History linenoise: `fopen("w")` ainda segue symlink (CVE-2025-9810 residual). Ver [`docs/memory/shells-seguranca.md`](docs/memory/shells-seguranca.md). | Média | — | Baixa | 🔍 Pendente verificação | — |
| SEC-10 | W3 | Segurança | `~/.petrushrc` sem checar uid/mode do arquivo. Ver [`docs/memory/shells-seguranca.md`](docs/memory/shells-seguranca.md). | Média | — | Baixa | 🔍 Pendente verificação | — |
| UX-18 | W4 | Shell | Glob simples `*` `?` só em tokens unquoted. | Média | UX-17 | Média | 🔍 Pendente verificação | — |
| SEC-07 | W4 | Segurança | Fechar TOCTOU `realpath` → `open` no `pudod` (O_NOFOLLOW / mesmo fd). Ver [`docs/memory/sudo-pudo-riscos.md`](docs/memory/sudo-pudo-riscos.md). | Alta | SEC-06 | Média | 🔍 Pendente verificação | — |
| UX-19 | W5 | Shell | Builtins no pipe (subshell-like). | Média | UX-18 | Média | 🔍 Pendente verificação | — |
| UX-20 | W6 | Shell | Ctrl-R history search. Patch mínimo no linenoise; se a API bloquear, documentar degradação (setas + hints + `!!`) e seguir. | Média | UX-19 | Alta | 🔍 Pendente verificação | — |
| UX-21 | W7 | Shell | Syntax highlight mínimo (aspas não fechadas / token grosso; não full highlighter). | Baixa | UX-20 | Alta | 🔍 Pendente verificação | — |
| UX-22 | W8 | Shell | `source` / `.`: rodar arquivo linha a linha. | Baixa | UX-21 | Média | 🔍 Pendente verificação | — |
| UX-23 | W9 | Shell | Background `&` + job control mínimo. | Baixa | UX-22 | Alta | 🔍 Pendente verificação | — |
| FEAT-BANG | W10 | Shell | Word designators `!$` e `!^` sobre `!!`/`!n`. Sem `!str`, sem modifiers. Ver [`docs/memory/shells-funcoes.md`](docs/memory/shells-funcoes.md). | Alta | UX-23 | Baixa | 🔍 Pendente verificação | — |
| FEAT-TRUE | W10 | Shell | Builtins `true` / `false` / `:`. Sem `printf`. Ver [`docs/memory/shells-funcoes.md`](docs/memory/shells-funcoes.md). | Alta | UX-23 | Baixa | 🔍 Pendente verificação | — |
| FEAT-NOCLOBBER | W11 | Shell | Recusar overwrite em `>` se o destino existe (`O_EXCL` em process+dispatcher). Sem `set` POSIX. Mesmo patch que SEC-09: se SEC-09 já fechou O_EXCL, aqui só resta política/UX noclobber, sem retrabalho. Ver [`docs/memory/shells-funcoes.md`](docs/memory/shells-funcoes.md). | Média | FEAT-BANG, FEAT-TRUE, SEC-09 | Baixa | 🔍 Pendente verificação | — |
| FEAT-UMASK | W11 | Shell | Builtin `umask` (máscara do processo do shell). Ver [`docs/memory/shells-funcoes.md`](docs/memory/shells-funcoes.md). | Média | FEAT-BANG, FEAT-TRUE | Baixa | 🔍 Pendente verificação | — |
| FEAT-READ | W12 | Shell | Builtin `read` (1 linha → 1 variável). Sem `-a`/`-d`/timeout. Ver [`docs/memory/shells-funcoes.md`](docs/memory/shells-funcoes.md). | Alta | FEAT-UMASK, FEAT-NOCLOBBER | Média | 🔍 Pendente verificação | — |
| FEAT-PARAM | W12 | Shell | Completar UX-13: `${VAR:-}` `${VAR:+}` `${#VAR}`. Sem nameref/`${!}`/`#` `%` strip. Ver [`docs/memory/shells-funcoes.md`](docs/memory/shells-funcoes.md). | Alta | FEAT-UMASK, FEAT-NOCLOBBER | Média | 🔍 Pendente verificação | — |
| FEAT-TEST | W13 | Shell | Builtin `test` / `[` primaries curtos (`-f -d -e -z -n = != -eq -ne -lt -gt`). Sem `[[`, sem `if`/`$(( ))`. Ver [`docs/memory/shells-funcoes.md`](docs/memory/shells-funcoes.md). | Alta | FEAT-READ, FEAT-PARAM | Média | 🔍 Pendente verificação | — |
| TST-T8 | W14 | Testes | Verificação de secrets (gitleaks/trufflehog). Ver `TESTES.md`. Relatório: [`docs/memory/tst-t8-secrets.md`](docs/memory/tst-t8-secrets.md). | Alta | FEAT-TEST | Baixa | 🔍 Pendente verificação | — |
| TST-T15 | W14 | Testes | Pré-CI no container local Fedora 44 (mesma imagem do `ci.yml`): build + lint + smoke + valgrind/`verify`. GHA é o espelho remoto, não o caminho pesado. Ver `TESTES.md`. Relatório: [`docs/memory/tst-t15-preci.md`](docs/memory/tst-t15-preci.md). | Alta | FEAT-TEST | Baixa | 🔍 Pendente verificação | Release smoke 53/53 após `cmake --install` sem setuid (2026-08-22); tree ainda 51/53 (SEC-02 intacto). clang-tidy ArrayBound Release residual. |
| TST-T2 | W14 | Testes | Análise estática (cppcheck + clang-tidy). Ver `TESTES.md`. Relatório: [`docs/memory/tst-t2-estatica.md`](docs/memory/tst-t2-estatica.md). Gate re-rodado 2026-08-22: cppcheck exit 0, clang-tidy exit 0. | Alta | FEAT-TEST | Média | 🔍 Pendente verificação | — |
| TST-T4 | W14 | Testes | Análise dinâmica de memória (ASan + UBSan + valgrind). Ver `TESTES.md`. Relatório: [`docs/memory/tst-t4-asan.md`](docs/memory/tst-t4-asan.md). | Alta | FEAT-TEST | Média | 🔍 Pendente verificação | Sanitize check 18/18 + smoke 53/53; valgrind 18 unit + 8 petrush paths + pudod deny; 0 leak/UB 2026-08-22 |
| TST-T14 | W14 | Testes | Integração fim-a-fim (smoke). Ver `TESTES.md`. Relatório: [`docs/memory/tst-t14-smoke.md`](docs/memory/tst-t14-smoke.md). | Alta | FEAT-TEST | Média | 🔍 Pendente verificação | smoke 53/0 + check 18/18 PASS 2026-08-22 |
| TST-T7 | W14 | Testes | Scanning de binário (checksec / hardening-check). Ver `TESTES.md`. Relatório: `docs/memory/tst-t7-binario.md` (Release `build-preci-rel`: Full RELRO+PIE+NX+canary OK; Fortify parcial). | Média | FEAT-TEST | Baixa | 🔍 Pendente verificação | — |
| TST-T5 | W14 | Testes | Scanning de dependências (trivy/grype/OSV). Ver `TESTES.md`. Relatório: [`docs/memory/tst-t5-deps.md`](docs/memory/tst-t5-deps.md). | Média | FEAT-TEST | Baixa | 🔍 Pendente verificação | grype+trivy+osv EXIT 0; linenoise CVE-2025-9810 MEDIUM mitigado; 0 CRITICAL vendor |
| TST-T12 | W14 | Testes | Busca de CVEs nas deps/toolchain. Ver `TESTES.md`. Relatório: [`docs/memory/tst-t12-cves.md`](docs/memory/tst-t12-cves.md). | Média | FEAT-TEST | Baixa | 🔍 Pendente verificação | grype+trivy+osv EXIT 0; CVE-2025-9810 MEDIUM mitigado SEC-08; 0 CRITICAL vendor; fedora:44 grype 0 |
| TST-T3 | W14 | Testes | Fuzzing de inputs (parser / expand / prompt). Ver `TESTES.md`. Relatório: [`docs/memory/tst-t3-fuzz.md`](docs/memory/tst-t3-fuzz.md). | Média | FEAT-TEST | Alta | 🔍 Pendente verificação | libFuzzer+ASan 60s×3; crashes=0; parser 1.2M / expand 2.9M / prompt 24M runs |
| AUD-SEC | W15 | Auditoria | Segurança (memory safety, secrets, `pudod`, binário). Ver `AUDITORIAS.md`. Relatório: [`docs/auditoria/aud-sec.md`](docs/auditoria/aud-sec.md). | Alta | TST-T2, TST-T4, TST-T7, TST-T8, TST-T12 | Alta | 🔍 Pendente verificação | APROVAR COM RESSALVAS; setuid 4755 NÃO endossado; SEC-01..10 presentes; T4/T7/T8/T12 cruzados 2026-08-22 |
| AUD-DISC | W15 | Auditoria | Descoberta e modelagem de ameaça. Ver `AUDITORIAS.md`. Relatório: [`docs/auditoria/aud-disc.md`](docs/auditoria/aud-disc.md). | Alta | TST-T2, TST-T4, TST-T8, TST-T14, TST-T15 | Média | 🔍 Pendente verificação | STRIDE+DFD+ativos; cruzou sudo-pudo-riscos+shells-seguranca; setuid NÃO endossado; score modelagem 82/100; sem PoC/push 2026-08-22 |
| AUD-ARCH | W15 | Auditoria | Arquitetura e 4 camadas. Ver `AUDITORIAS.md`. Relatório: [`docs/auditoria/aud-arch.md`](docs/auditoria/aud-arch.md). | Alta | TST-T2, TST-T4, TST-T8, TST-T14, TST-T15 | Média | 🔍 Pendente verificação | score 70/100; APROVADO COM RESSALVAS; F1 Mid+linenoise HIGH; F2 rc_trust Mid→Front; sem push 2026-08-22 |
| AUD-QUALITY | W15 | Auditoria | Qualidade de código (complexidade, dead code, Rule of 3). Ver `AUDITORIAS.md`. Relatório: [`docs/auditoria/aud-quality.md`](docs/auditoria/aud-quality.md). | Média | TST-T2, TST-T14 | Média | 🔍 Pendente verificação | score 65/100; APROVADO COM RESSALVAS; lizard avg CCN 7.6 max 40; TST-T2 lint exit 0 com gap de lista; Rule of 3 em expand redirs; sem push 2026-08-22 |
| AUD-COV | W15 | Auditoria | Cobertura nos módulos críticos. Ver `AUDITORIAS.md`. Relatório: [`docs/auditoria/aud-cov.md`](docs/auditoria/aud-cov.md). | Média | TST-T14 | Média | 🔍 Pendente verificação | críticos 76.6% linhas (parser 90.9 expand 85.1 prompt 93.9; process 65.4 dispatcher 67.7 pudo 68.0 abaixo 70%); pudod.c gap `_exit`; check 18 + smoke 53; sem push 2026-08-22 |
| AUD-DEPS | W15 | Auditoria | Dependências e acoplamento (linenoise + camadas). Ver `AUDITORIAS.md`. Relatório: [`docs/auditoria/aud-deps.md`](docs/auditoria/aud-deps.md). | Baixa | TST-T5, TST-T12 | Baixa | 🔍 Pendente verificação | score 72/100; APROVADO COM RESSALVAS; Mid+linenoise HIGH (D1); cruzou T5/T12 0 CRITICAL; BSD-2×AGPL OK; sem push 2026-08-22 |
| AUD-LANG | W15 | Auditoria | Idioms C23. Ver `AUDITORIAS.md`. Relatório: [`docs/auditoria/aud-lang.md`](docs/auditoria/aud-lang.md). | Baixa | TST-T2 | Baixa | 🔍 Pendente verificação | score 71/100; APROVADO COM RESSALVAS; C23 via gnu2x (STDC 202311); 0 VLA; nullptr=0 NULL dominante; bool/static_assert ausentes; sem push 2026-08-22 |
| AUD-REPORT | W16 | Auditoria | Relatório final (score 0-100, patches). Ver `AUDITORIAS.md`. Livro: [`docs/auditoria/aud-report.md`](docs/auditoria/aud-report.md). Índice: [`docs/auditoria/README.md`](docs/auditoria/README.md). | Alta | AUD-DISC, AUD-ARCH, AUD-SEC, AUD-QUALITY, AUD-COV, AUD-DEPS, AUD-LANG | Média | 🔍 Pendente verificação | score 73/100; APROVADO COM RESSALVAS (unpriv early); setuid 4755 NÃO endossado; 3🔴 P0 (gate 4755, fallback sudo, allow-list); sem src/; sem push 2026-08-22 |
| SEC-11 | W16 | Segurança | Fail-closed se `pudod` ausente: nunca `execve("/usr/bin/sudo")` nem `execvp("sudo")` (R-C2, R-I1). Ver [`docs/auditoria/aud-report.md`](docs/auditoria/aud-report.md). | Alta | AUD-REPORT | Média | 🔍 Pendente verificação | TDD red→green; pudo_allow_sudo_fallback=0; sem execve/execvp sudo; Debug=Release |
| SEC-12 | W16 | Segurança | Recusar shell genérico na allow-list (basename sh/bash/dash/ash/busybox) após realpath. Ver [`docs/auditoria/aud-report.md`](docs/auditoria/aud-report.md). | Alta | SEC-11 | Média | 🔍 Pendente verificação | — |
| NEW-23 | W17 | Docs | Atualizar Wiki GitHub + `docs/beginner-guide.md` após a próxima tag (`v0.4` se UX-12..18; `v0.5` se até UX-23). Guia em [`docs/beginner-guide.md`](docs/beginner-guide.md); tag `v0.5.0`. Wiki GitHub = o guia (clone de wiki.git proibido nesta sessão). | Baixa | AUD-REPORT | Média | 🔍 Pendente verificação | — |
| DOC-02 | W18 | Docs | Atualizar `docs/security/pudo-audit.md` e `docs/memory/shells-seguranca.md` para SEC-03/04/05/09/11/12. Fecha R-I4. Ver [`docs/auditoria/aud-report.md`](docs/auditoria/aud-report.md) P1.1. | Média | AUD-REPORT | Baixa | 🔍 Pendente verificação | prosa↔código SEC-03/04/05/09/11/12; noclobber O_EXCL; Boundary B fechada; sem push 2026-08-22 |
| DOC-03 | W18 | Docs | NOTICE ou README Third-party: BSD-2 linenoise (Sanfilippo/Noordhuis). Fecha R-I16. Ver [`docs/auditoria/aud-report.md`](docs/auditoria/aud-report.md) P1.2. | Alta | AUD-REPORT | Baixa | 🔍 Pendente verificação | NOTICE + README Third-party; LICENSE.md intacto AGPL; sem push 2026-08-22 |
| DOC-04 | W18 | Docs | `docs/architecture.md` + README Front: `job.c`, Mid completo, `rc_trust`, pudod, Solo→early; exceções F3/F4. Fecha R-I12/R-I10/R-I11. Ver [`docs/auditoria/aud-report.md`](docs/auditoria/aud-report.md) P1.3. | Média | AUD-REPORT | Baixa | 🔍 Pendente verificação | architecture.md + front README; Solo→early; F3/F4 documentadas; sem push 2026-08-22 |
| ARCH-01 | W19 | Arquitetura | `complete.h` sem `#include "linenoise.h"` (include só no `.c`). Fecha R-I13 / P2.3. | Alta | DOC-04 | Baixa | 🔍 Pendente verificação | include só em complete.c; test_complete 4/4; rg header=0; sem push |
| ARCH-02 | W19 | Arquitetura | Mover `rc_trust` de Front para Foundation (CMake + headers + architecture.md). Fecha R-I9 / P2.2. | Alta | DOC-04 | Média | 🔍 Pendente verificação | `src/foundation/rc_trust.c`; CMake 6 refs; architecture.md F2 fechado; test_rc_trust+test_source OK; sem push 2026-08-23 |
