# AUD-REPORT - Relatório final de auditoria (petrush)

| Campo | Valor |
|-------|-------|
| **ID** | AUD-REPORT |
| **Onda** | W16 |
| **Data** | 2026-08-22 |
| **SHA HEAD (pré-commit)** | `618aa2c` (`618aa2cf4d3ec4b657719a47573eed25b361a49e`) |
| **Auditor** | internal-auditor (dono do livro) |
| **C-level de leitura** | Cláudio (CLO), Narciso (CISO), Caetano (CTO) |
| **Porte** | early (Pipeline-Sprint; `.bigtech-porte`) |
| **Manuais** | `Projects/petrush/AUDITORIAS.md` § AUD-REPORT; vault `AUDITORIAS.md` (escala 🔴🟠🟢); vault `TESTES.md` A10 (score, problemas, patches); `DEPLOY_CHECKLIST` (setuid = irreversível) |
| **Índice mestre** | [`README.md`](README.md) |
| **Código de produto** | **não** alterado (`src/` intocado) |
| **Push** | não |
| **Setuid `4755`** | **NÃO ENDOSSADO** |

Este arquivo é o **livro consolidado**. Capítulos-fonte em `docs/auditoria/aud-*.md`. Achado sem evidência no capítulo **não entra**. Nenhum 🔴 fica sem plano.

---

## 1. Contexto e método

### 1.1 O que o petrush é (mapa local, não Qt/web)

Shell REPL C23 (Opção A, 2026-05-27): prompt, externos + builtins, history, rc, sinais. Helper `pudod` separado (elevação só se o operador aplicar setuid à mão). Back ainda placeholder. Mid = domínio do shell. AUD-ARCH F8: o livro usa o mapa petrush.

### 1.2 Método desta consolidação

1. Ler manuais (`AUDITORIAS.md` projeto + vault; `TESTES.md` projeto + vault A10) **antes** de classificar.
2. Ler os 7 capítulos W15 por completo (não só o TODO).
3. Deduplicar (F1 = D1; C1 do SEC = risco 1 do DISC).
4. Reclassificar na escala vault **sem inflar nem minimizar** (ver §4.1).
5. Derivar score 0-100 com pesos explícitos (§2).
6. Priorizar patches P0-P3 **sem aplicar**.
7. Emitir parecer de prontidão (§9).

Não re-executei SAST/SCA/ASan: os capítulos já cruzaram T2/T4/T5/T7/T8/T12. Relatório de capítulo **não é prova isolada**; o livro só aceita o que cita path, comando ou gate TST.

### 1.3 Escopo / fora

**In:** REPL, parser, expand, process, dispatcher, pudo/pudod, history, rc, vendor linenoise, CMake, docs de segurança.  
**Out:** PoC, payload, `chmod 4755` neste host, push, AUD-DB/API/UI/FRAMEWORK, wiki NEW-23.

---

## 2. Score global 73 / 100

### 2.1 Notas por capítulo

| Capítulo | Score | Origem | Peso | Contribuição |
|----------|------:|--------|-----:|-------------:|
| AUD-SEC | 76 | derivado (§2.2) | 25% | 19.00 |
| AUD-ARCH | 70 | capítulo | 15% | 10.50 |
| AUD-QUALITY | 65 | capítulo | 15% | 9.75 |
| AUD-COV | 73 | derivado (§2.3) | 15% | 10.95 |
| AUD-DISC | 82 | capítulo (maturidade de modelagem) | 10% | 8.20 |
| AUD-DEPS | 72 | capítulo | 10% | 7.20 |
| AUD-LANG | 71 | capítulo | 10% | 7.10 |
| **Total** | **73** | arredondado de 72.7 | 100% | **72.70** |

Pesos: stake do helper privilegiado (SEC no topo); cobertura/arquitetura/qualidade no segundo terço; descoberta, deps e idioma no restante. Porte early: LANG não puxa o score para baixo só por `NULL` vs `nullptr`.

Média aritmética não ponderada: (82+70+76+65+73+72+71)/7 = 72.7. Coincide. O número **73** não é marketing.

### 2.2 Score SEC derivado (capítulo sem nota 0-100)

AUD-SEC emitiu veredicto sem nota. O livro deriva, na mesma grade 5×20 dos irmãos:

| Dimensão | /20 | Critério |
|----------|----:|----------|
| Memory safety (T4) | 18 | Sanitize 18/18 + smoke 53/53; valgrind 0 leak/UB |
| Secrets (T8) | 19 | gitleaks limpo (78 commits nesta passada) |
| Binário (T7) | 15 | Full RELRO+PIE+NX+canary; Fortify parcial; symbols |
| Helper / privilégio | 11 | SEC-01..10 presentes; C2 fallback sudo; I1 `execvp`; 4755 latente |
| Superfície residual | 13 | I4 drift doc; I5 REPL unpriv; CVE linenoise mitigado |
| **Soma** | **76** | |

### 2.3 Score COV derivado

AUD-COV emitiu veredicto sem nota 0-100.

| Dimensão | /20 | Critério |
|----------|----:|----------|
| Suíte sob coverage | 18 | check 18/18 + smoke 53/0 |
| Agregado T1 ≥70% linhas | 16 | 76.6% (1476/1928) |
| Cada crítico ≥70% | 12 | `process` 65.4, `dispatcher` 67.7, `pudo` 68.0 |
| Helper mensurável | 10 | `pudod.c` sem `.gcda` por `_exit` |
| Tooling / branches | 17 | gcov/lcov OK; branches 65.7% informativo |
| **Soma** | **73** | |

### 2.4 Leitura do 73

Faixa 70-79 = **aprovado com ressalvas** para early: gates TST verdes no que importa (memória, secrets, SCA, smoke), dívida clara de fronteira (linenoise no Mid), complexidade, e um residual de **processo privilegiado** (Boundary B) que impede go de setuid. Não é 85+ (não está pin-ready privilegiado). Não é <60 (não há UB aberto nem CVE CRITICAL no vendor).

---

## 3. Inventário do dossiê (capítulos)

| Relatório | Data | SHA citado | Veredicto do especialista | Achado-chave |
|-----------|------|------------|---------------------------|--------------|
| [`aud-disc.md`](aud-disc.md) | 2026-08-22 | `e4cdc7b` | Modelagem pronta (82) | STRIDE+DFD; 4755 não endossado; drift das memórias W2 |
| [`aud-arch.md`](aud-arch.md) | 2026-08-22 | (W15) | APROVADO COM RESSALVAS (70) | F1 Mid+linenoise HIGH; F2 `rc_trust` |
| [`aud-sec.md`](aud-sec.md) | 2026-08-22 | `eaf37e1` | APROVAR COM RESSALVAS | C1 4755; C2 fallback sudo; SEC-01..10 presentes |
| [`aud-quality.md`](aud-quality.md) | 2026-08-22 | `e4cdc7b` | APROVADO COM RESSALVAS (65) | dispatcher 952 linhas; avg CCN 7.6; gap clang-tidy |
| [`aud-cov.md`](aud-cov.md) | 2026-08-22 | `7b15ecc` | APROVAR COM RESSALVAS | 76.6% agregado; 3 núcleos <70%; `_exit`/gcov |
| [`aud-deps.md`](aud-deps.md) | 2026-08-22 | `7b15ecc` | APROVADO COM RESSALVAS (72) | D1 = F1; 0 CRITICAL SCA; BSD-2×AGPL OK |
| [`aud-lang.md`](aud-lang.md) | 2026-08-22 | `bb8cec7` | APROVADO COM RESSALVAS (71) | gnu2x = C23; 0 VLA; `nullptr`/`bool`/`static_assert` = 0 |
| **Este livro** | 2026-08-22 | `618aa2c` | **73 / APROVADO COM RESSALVAS** | P0 setuid+Boundary B |

SHAs dos capítulos são os HEAD de cada fatia W15 (árvore avançou; conteúdo citado permanece no histórico). O livro amarra no HEAD `618aa2c`.

---

## 4. Achados consolidados

### 4.1 Política de classificação neste livro

Escala vault: 🔴 CRÍTICO · 🟠 IMPORTANTE · 🟢 COSMÉTICO.

- **🔴** neste livro = risco de **escalada / compromisso de host / política que vira root**, ou **gate que, se violado, vira isso**. Memory corruption aberto também seria 🔴; **não há** nesta passada (T4 verde).
- **🟠** = fronteira, dívida que encarece troca de vendor, gap de teste em núcleo, residual unpriv, compliance de atribuição.
- **🟢** = tooling, polish, drift menor.

**Reclassificação (explícita, não maquiagem):** AUD-QUALITY marcou Q-GOD-01 / Q-CCN-01 / Q-COG-01 como 🔴 I9 (god-file / CCN). I9 do vault trata god-class como crítico de *qualidade*. Sem crash, UB ou priv-esc associado, o consolidado **não** empilha esses IDs no mesmo saco que setuid. Viram 🟠 (R-I17, R-I18). O capítulo-fonte continua válido; o livro não some com a dívida.

Coluna **Estado Auditado:** `-` aberto (não remedado) · `✓` controle revalidado no capítulo · `⚠` gate/residual (existe mitigação, não é “fechado”).

### 4.2 🔴 CRÍTICO (3)

| ID | Fonte | Descrição | Evidência | Remediação | Estado |
|----|-------|-----------|-----------|------------|--------|
| **R-C1** | AUD-SEC-C1; DISC risco 1 | **Setuid `4755` / setcap no `pudod` não endossado.** Qualquer bit setuid transforma bug residual + allow-list larga em escalada local. CMake e `pudod-install.md` **não** aplicam 4755. Host sem `/usr/local/libexec/petrush-pudod`. Artefato Release `755`. | `aud-sec.md` §4 C1, §8; `stat` do binário; ausência do helper instalado | **Não** aplicar 4755/setcap. Se um dia for cogitado: AskUserQuestion + Narciso + allow-list ≤3 inocentes + revisão linha a linha de `pudod.c`. `DEPLOY_CHECKLIST`. | ⚠ gate ativo |
| **R-C2** | AUD-SEC-C2; DISC 7.4 E | **Fallback Boundary B:** se `find_pudod_binary()` falha, o frontend avisa e faz `execve("/usr/bin/sudo")` e, se falhar, `execvp("sudo")`. Privilegio deixa o helper mínimo e reabre sudoers/PAM/tickets. | `src/mid/pudo.c` `run_via_pudod` (~L667-696); `aud-sec.md` C2; `sudo-pudo-riscos.md` meta-B | Fail-closed em **Release** sem pudod. Remover `execvp("sudo")`. Path absoluto só se o líder pedir lab. | - |
| **R-C3** | DISC 7.5 E; SEC-03 | **Allow-list com shell genérico** (`/bin/sh`, bash, busybox) reabre o modelo POSIX **já como root**. Não é bug do parser; é falha de política. Example atual é mínimo (`id`/`whoami`/`true`). | `aud-disc.md` §7.5 composição crítica; `pudo.allow.example`; shells-seguranca §12 | Manter example mínimo. Nunca listar shells. Operador que alargar `/etc/petrush/pudo.allow` assume o risco. | ⚠ latente (example OK) |

**Nenhum dos três é memory-unsafe aberto.** R-C1 é veto de processo (o código *não* instala setuid: isso é o comportamento certo). R-C2 é o único 🔴 de *código* ainda no caminho de produto. R-C3 é política.

### 4.3 🟠 IMPORTANTE (22)

#### Segurança / superfície

| ID | Fonte | Descrição | Evidência | Remediação | Estado |
|----|-------|-----------|-----------|------------|--------|
| **R-I1** | SEC-I1 | `execvp("sudo")` após falha de `/usr/bin/sudo` (PATH spoof, CWE-426) | `pudo.c` ~L694 | Remover; fail closed. Casa com R-C2. | - |
| **R-I2** | SEC-I2 | `build_clean_envp` propaga `TZ`/`TMPDIR`/`HOME` no caminho **sudo** (pudod descarta; B não) | `pudo.c` ~L325-356 | Env mínimo também no B, ou abolir B (R-C2) | - |
| **R-I3** | SEC-I3 | Client `pudo.conf` ausente = frontend não filtra; autoridade = pudod. Com B, sobra só sudoers. | `is_command_allowed` ~L219-223 | Fail closed no client em Release, ou documentar que B é lab | - |
| **R-I4** | SEC-I4; DISC fontes | Docs de memória desatualizados vs SEC-03/04/05/09 (`pudo-audit.md` 2026-07-01; `shells-seguranca.md` noclobber “aberto”) | paths em `docs/security/` e `docs/memory/` | Atualizar na onda DOC (não nesta fatia) | - |
| **R-I5** | SEC-I5; DISC 7.1 | Superfície REPL unpriv residual: bang, alias, `source`, glob, append `>>` | `shells-seguranca.md` + código atual | Aceito no uid do user; não é priv-esc. Monitorar. | ⚠ aceito unpriv |
| **R-I6** | SEC-I6 | Fortify parcial + símbolos não stripped (Release) | checksec T7; 367/73 symbols | Strip no pacote de produção; não bloqueia AUD-SEC | - |
| **R-I7** | DISC risco 6 | Jobs `&` / TTY / orphan (UX-23 ainda em evolução de superfície) | `job.c`; `aud-disc.md` §9 | Monitorar SIGTSTP/orphan; sem fg/bg de propósito | ⚠ |

#### Arquitetura / deps

| ID | Fonte | Descrição | Evidência | Remediação | Estado |
|----|-------|-----------|-----------|------------|--------|
| **R-I8** | ARCH F1 = DEPS D1 | **Mid importa linenoise** (`dispatcher` `ClearScreen`; `hist_expand` History*) | `src/mid/dispatcher.c`, `hist_expand.c` | Porta Front `petrush_ui_clear_screen` + history get/len alimentada no boot | - |
| **R-I9** | ARCH F2 | Mid inclui `rc_trust.h` (físico Front; semântica = trust) | `source.c` → `rc_trust.h` | Mover `rc_trust.c` para Foundation ou Mid; CMake + architecture.md | - |
| **R-I10** | ARCH F3 | Mid chama `fork`/`exec*`/`open` (bg job + pudo client) | `dispatcher.c` bg; `pudo.c` exec | Documentar exceções; opcional `petrush_spawn_background()` | - |
| **R-I11** | ARCH F4 | Foundation inclui tipos do parser (`process.h` → `parser.h`) | `include/petrush/process.h` | Aceitar early com nota, ou extrair `cmd.h` neutro | - |
| **R-I12** | ARCH F5 | Drift `architecture.md` / README Front (“Solo”; omite `job.c`, Mid completo, pudod) | `docs/architecture.md`; `src/front/README.md` | DOC: listar módulos reais; Solo → early | - |
| **R-I13** | DEPS D2 | `complete.h` inclui `linenoise.h` (contrato público vaza vendor) | `include/petrush/complete.h` | Forward-declare no `.c`; header sem vendor | - |
| **R-I14** | DEPS D3 | Fork linenoise (4 patches) pode reintroduzir CVE-2025-9810 no bump | `vendor/linenoise/README.md`; T5/T12 | Checklist de diff vs `f2558e1e` / tag a cada bump | - |
| **R-I15** | DEPS D4 | Fan-out CMake: `linenoise.c` em petrush + ≥6 testes (sintoma de R-I8) | `CMakeLists.txt` | Após porta: lib estática única; testes Mid sem vendor | - |
| **R-I16** | DEPS D5 (LOW no capítulo; **sobe a 🟠**) | Atribuição BSD-2 incompleta na superfície de distribuição (`LICENSE.md` só AGPL; README sem linenoise) | headers vendor BSD; `LICENSE.md` | `NOTICE` ou seção Third-party (Sanfilippo / Noordhuis, BSD-2, path) | - |

R-I16 sobe de LOW para 🟠: redistribuição binária BSD-2 **exige** copyright/disclaimer. Não é 🔴 (licenças combináveis, D7). Não é só polish se o projeto publica binários.

#### Qualidade / cobertura / idioma

| ID | Fonte | Descrição | Evidência | Remediação | Estado |
|----|-------|-----------|-----------|------------|--------|
| **R-I17** | Q-GOD-01 (🔴 no capítulo → 🟠 aqui) | God-file `dispatcher.c` (952 linhas); também `pudo` 734, `parser` 651, `expand` 531, `process` 513 | `wc`; `aud-quality.md` §4 | Fatiar: `builtins_test.c`, alias, tabela + dispatch fino. Testes já existem. | - |
| **R-I18** | Q-CCN-01, Q-COG-01 | Avg CCN 7.6 (meta 5); 12 funções CCN>20; cognitive >50 em `expand_*` / `completion_cb` / `hl_scan` **fora** do gate tidy | lizard; clang-tidy pontual | Decompor hotspots; incluir arquivos no target tidy (R-I19) | - |
| **R-I19** | Q-T2-GAP | Target `clang-tidy` omite `expand.c`, `complete.c`, `highlight.c`, … TST-T2 verde ≠ complexidade sob controle | `CMakeLists.txt` L514-521 | Estender lista; `NOLINT` pontual ou extrair até ≤50 | - |
| **R-I20** | Q-DRY-01 = ARCH F6 | Redirs `open`/`dup2` duplicadas Mid × Foundation (n=2, pré-Rule-of-3) | `process.c` `apply_redirs`; `dispatcher.c` `run_builtin_with_redirs` | Extrair `petrush_apply_redirs` **antes** da 3ª cópia | - |
| **R-I21** | Q-DRY-02 | Três blocos idênticos de expand em redirs (Rule of 3 **dispara**) | `expand.c` `expand_cmd_argv` ~520-529 | Helper `expand_inplace(char **)` | - |
| **R-I22** | Q-CCN-02 | `match_op` CCN 40 (tabela densa, NLOC 26) | lizard; `highlight.c` | Tabela de literais / loop; não explodir em 10 funções | - |
| **R-I23** | AUD-COV §3 | `process` 65.4%, `dispatcher` 67.7%, `pudo` 68.0% linhas (meta 70% por crítico). Funções 0 hit: `signal_name`, `pipeline_abort`, `builtin_env`/`unalias`/`dirs`. | lcov genhtml `/var/tmp/petrush-aud-cov-report` | Testes dos builtins + ramos de erro / config pudo **sem** setuid | - |
| **R-I24** | AUD-COV §4.3 | `pudod.c` usa `_exit` (certo para setuid) e **engole** flush gcov; `main` do helper não mensurável | `.gcno` presente, `.gcda` ausente | `__gcov_dump()` antes de `_exit` **ou** harness da lógica sem o `main`. Não trocar `_exit`→`exit` no produto setuid. | - |
| **R-I25** | LANG L1 | 0 `nullptr`; ~180 `NULL` em src+include | `aud-lang.md` §3 | Política incremental: código novo usa `nullptr`; sem big-bang | - |
| **R-I26** | LANG L2 | Flags booleanas como `int` (`redir_*`, `background`, `notified`, …) | headers públicos | Fatia TDD: `bool` em flags; status 0/-1 permanece `int` | - |
| **R-I27** | LANG L3 | Zero `static_assert` nos limites (`PETRUSH_JOB_MAX`, `PUDOD_MAX_ARGS`, …) | headers + arrays estáticos | `static_assert` ao lado dos `#define` (baixo risco) | - |
| **R-I28** | LANG L4 | `.clang-tidy` sem política de idiom C23; T2 não prova modernidade | `.clang-tidy`; cppcheck `--std=c23` | Parágrafo em `docs/standards.md`; gate `rg` só se o líder quiser | - |

### 4.4 🟢 COSMÉTICO (11)

| ID | Fonte | Descrição | Estado |
|----|-------|-----------|--------|
| **R-O1** | SEC-O1 | API `pudo_sanitize_environment` ainda muta `environ` (útil a testes) | - |
| **R-O2** | SEC-O2 | Trunc de log (`LOG_BUF_SIZE` 512) | - |
| **R-O3** | SEC-O3 | `trufflehog` / `hardening-check` ausentes (cobertos por gitleaks + checksec) | ⚠ tooling |
| **R-O4** | SEC-O4 | Vault `AUDITORIAS.md` nesta máquina é sobretudo índice; poda do projeto carrega o operacional | ⚠ processo |
| **R-O5** | Q-TOOL-01 | tokei ausente; LOC via `wc` | - |
| **R-O6** | Q-TOOL-02 | cppcheck `--suppress=unusedFunction` (dead-code cego no CI) | - |
| **R-O7** | Q-DOC-01 | `tst-t2-estatica.md` descreve gate falho antigo; TODO já `🔍` verde | - |
| **R-O8** | LANG L5 | Dialeto `gnu2x` (default extensions) em vez de `c23` estrito; `__STDC_VERSION__` 202311L | ⚠ aceito (`pudod` precisa GNU) |
| **R-O9** | LANG L6 | `typeof` / `_Generic` / `[[nodiscard]]` / `_BitInt` ausentes (sem urgência de domínio) | - |
| **R-O10** | AUD-COV tooling | gcovr ausente; `lcov --list` 2.0 com Rate corrompida (contornado) | - |
| **R-O11** | AUD-DISC | Threat Dragon não gerado; DFD textual basta no early | - |

OK explícitos (não são achados, ficam no livro para o auditor externo não reabrir):

- T4 memory: 0 leak/UB observável.
- T8 secrets: limpo.
- T5/T12: 0 CRITICAL vendor; CVE-2025-9810 mitigado (SEC-08 + `test_linenoise_history`).
- T3 fuzz: crashes=0 (parser/expand/prompt).
- T14 smoke 53/0; check 18/18.
- `ldd` petrush: só libc. `pudod` sem linenoise e sem `petrush/*`.
- BSD-2 × AGPL-3.0 combináveis (D7). Residual = atribuição (R-I16).
- 0 VLA / 0 `alloca`.
- SEC-01..10 **presentes** no tree (matriz `aud-sec.md` §3). Status TODO permanece `🔍`.

---

## 5. Cruzamento TST (gates W14)

| Gate | Status TODO | O que o livro consome |
|------|-------------|------------------------|
| TST-T2 | 🔍 | cppcheck+clang-tidy exit 0; lacuna de lista (R-I19) |
| TST-T3 | 🔍 | crashes=0; alimenta memory/quality |
| TST-T4 | 🔍 | 0 leak/UB; base do score SEC |
| TST-T5 | 🔍 | 0 CRITICAL; linenoise MEDIUM mitigado |
| TST-T7 | 🔍 | hardening OK; Fortify parcial (R-I6) |
| TST-T8 | 🔍 | 0 leaks; re-scan AUD-SEC |
| TST-T12 | 🔍 | CVE-2025-9810 ↔ SEC-08 |
| TST-T14 | 🔍 | smoke 53/0 + check 18/18 |
| TST-T15 | 🔍 | container Fedora 44; Release smoke 53/53 **após** install sem setuid; tree 51/53 = SEC-02 intacto |

Nenhum TST W14 está `✅` (regra da casa: `✅` só pós TST/AUD de fechamento). O livro **não** promove status.

---

## 6. Postura setuid / `pudod` (não negociável nesta passada)

```
petrush (unpriv) --argv absoluto--> pudod (só se euid==0)
                                      allow-list root-owned
                                      realpath + O_NOFOLLOW + fstat
                                      root-owned regular + exec
                                      envp mínimo hardcoded
                                      fexecve(fd)  [Linux]
                 --(se pudod ausente)--> /usr/bin/sudo -- …   <- R-C2
```

1. **Não** aplicar `chmod 4755` nem `setcap` sem ordem explícita do líder após revisão de `pudod.c` e allow-list mínima.
2. Install CMake continua **sem** setuid automático.
3. Smoke/CI continuam no caminho **sem** privilégio (deny + prefix sem 4755), como T15.
4. Fallback sudo (R-C2) é o obstáculo de código para qualquer conversa futura de 4755: enquanto B existir, o modelo “helper mínimo auditável” não é o único caminho.

**Este livro não endossa 4755.** Repetido de propósito (AUD-SEC, AUD-DISC, TODO Gate Cosimo, brief desta fatia).

---

## 7. Patches priorizados (não aplicados)

Formato A10: unificado, ordenado, reversível salvo onde dito. **Nenhuma linha de `src/` mudou nesta fatia.**

### P0 - privilégio / processo (bloqueia 4755 e produção privilegiada)

| # | Patch | Fecha | Esforço | Notas |
|---|-------|-------|---------|-------|
| P0.1 | **Não fazer:** `chmod 4755` / `setcap` | R-C1 | zero | Gate. Só o líder revoga. |
| P0.2 | Release: se pudod ausente, **falhar fechado** (não chamar sudo) | R-C2 | S/M | Lab pode ficar atrás de `#ifndef NDEBUG` se o líder pedir |
| P0.3 | Remover `execvp("sudo")` | R-I1, R-C2 | S | Mesmo PR que P0.2 |
| P0.4 | Política: allow-list sem `/bin/sh` / bash / busybox | R-C3 | S (doc+example) | Example já mínimo; reforçar no install doc |

### P1 - higiene que o auditor externo lê primeiro

| # | Patch | Fecha | Esforço |
|---|-------|-------|---------|
| P1.1 | Atualizar `docs/security/pudo-audit.md` e `docs/memory/shells-seguranca.md` para SEC-03/04/05/09 | R-I4 | S (DOC) |
| P1.2 | `NOTICE` (ou README Third-party) BSD-2 linenoise | R-I16 | S (DOC) |
| P1.3 | `docs/architecture.md` + README Front: `job.c`, Mid completo, `rc_trust`, pudod, Solo→early; exceções F3/F4 | R-I12, R-I10, R-I11 | S (DOC) |

### P2 - fronteira (CONTRACT §5)

| # | Patch | Fecha | Esforço |
|---|-------|-------|---------|
| P2.1 | Porta Front: clear screen + history get/len; Mid sem `#include "linenoise.h"` | R-I8, R-I15 | M |
| P2.2 | Mover `rc_trust` para Foundation (ou Mid) | R-I9 | S |
| P2.3 | `complete.h` sem include de linenoise | R-I13 | S |
| P2.4 | (opcional) `add_library(linenoise STATIC …)` após P2.1 | R-I15 | S |

### P3 - qualidade / cobertura / idioma (early, não bloqueia unpriv)

| # | Patch | Fecha | Esforço |
|---|-------|-------|---------|
| P3.1 | Estender target clang-tidy a expand/complete/highlight/alias/source/prompt/hist_expand/dirstack/rc_trust | R-I19, R-I18 | S (build) |
| P3.2 | Testes `builtin_env` / `unalias` / `dirs` + `pipeline_abort` / `signal_name` | R-I23 | S |
| P3.3 | Casos negativos `pudo.c` sem setuid (config ausente, candidato rejeitado) | R-I23 | S |
| P3.4 | `expand_inplace` nos 3 redirs (Rule of 3) | R-I21 | S |
| P3.5 | `static_assert` nos limites | R-I27 | S |
| P3.6 | Extrair `petrush_apply_redirs` (antes da 3ª cópia) | R-I20 | M |
| P3.7 | Fatiar `dispatcher.c` | R-I17 | L |
| P3.8 | Decompor `expand_brace` / `glob_word` / `completion_cb` | R-I18 | M |
| P3.9 | Política `nullptr`/`bool` incremental + parágrafo C23 em `standards.md` | R-I25, R-I26, R-I28 | S |
| P3.10 | `__gcov_dump` antes de `_exit` **ou** unit do corpo pudod (não trocar `_exit` no produto) | R-I24 | M |
| P3.11 | Checklist de bump vendor vs fix CVE-2025-9810 | R-I14 | S (proc) |
| P3.12 | Isolar `pudo_sanitize_environment` de builds de produto | R-O1 | S |

Reversibilidade: P0.2 muda comportamento de lab (quem depende do fallback sudo vai notar). P2/P3 two-way se a suíte ficar verde. P3.7 não remover API pública.

---

## 8. Matriz SEC-01..10 (herdada, não reaberta)

Controles **presentes** (`aud-sec.md` §3). O livro não marca `✅` nos IDs SEC-* da tabela (continuam `🔍`).

| ID | Controle | Residual para o livro |
|----|----------|------------------------|
| SEC-01 | sem `unsetenv` no pai | API de teste ainda muta (R-O1) |
| SEC-02 | Release: path install absoluto | Debug ainda relativos; tree smoke 51/53 esperado |
| SEC-03 | example allow mínima | política humana (R-C3) |
| SEC-04 | argc > MAX fail closed | - |
| SEC-05 | `realpath` fail → skip literal | - |
| SEC-06 | alvo regular root+exec | - |
| SEC-07 | `O_NOFOLLOW` + `fexecve` | fallback non-Linux `execve(path)` |
| SEC-08 | history `O_NOFOLLOW` + `fchmod` 0600 | CVE-2025-9810 mitigado |
| SEC-09 | `>` / `2>` com `O_EXCL` | `>>` append esperado |
| SEC-10 | rc uid/mode | - |

---

## 9. Parecer de prontidão

| Pergunta | Resposta |
|----------|----------|
| Livro completo e coeso? | **Sim** (índice + 7 capítulos + este consolidado) |
| Score 0-100 entregue? | **73** |
| Patches unificados? | **Sim** (P0-P3) |
| Pronto para auditor externo (caminho **unpriv**)? | **Sim, com ressalvas listadas** |
| Pronto para `chmod 4755`? | **Não** |
| Pronto para tag/release **privilegiada** / pin de produção com helper setuid? | **Não** (P0 aberto) |
| Pronto para uso REPL local / CI sem setuid? | **Sim, porte early**, dívida P2/P3 não bloqueia |
| Go/no-go 4755 | **NO-GO** |

**Veredicto único:** **APROVADO COM RESSALVAS** (early, unpriv). **NO-GO** de setuid.

Não marca `✅` em AUD-REPORT. Status da tabela: `🔍 Pendente verificação` (impl do artefato; `✅` só após julgamento do orquestrador).

---

## 10. O que este livro não é

- Não é pentest. Sem PoC.
- Não é autorização de install privilegiado.
- Não é quitação dos IDs SEC/UX/FEAT/TST (permanecem `🔍`).
- Não é atualização da wiki (NEW-23, W17).
- Não reescreve `src/`.

---

## 11. Checklist de saída AUD-REPORT

- [x] Manuais lidos (projeto + vault AUDITORIAS/TESTES; DEPLOY_CHECKLIST no eixo setuid)
- [x] Sete capítulos W15 lidos por completo
- [x] Índice mestre em `docs/auditoria/README.md`
- [x] Livro em `docs/auditoria/aud-report.md`
- [x] Score 0-100 com pesos e derivadas SEC/COV
- [x] Achados CRÍTICO / IMPORTANTE / COSMÉTICO + Estado Auditado
- [x] Dedup F1=D1; QUALITY 🔴 I9 reclassificado a 🟠 no livro
- [x] 3 🔴 com plano P0
- [x] Setuid 4755 **não** endossado
- [x] Patches P0-P3 (não aplicados)
- [x] `src/` intocado
- [x] Sem push
- [ ] Status TODO `🔍` (este commit)
- [ ] `✅` só após julgamento do orquestrador

---

## 12. Referências

- Capítulos: `docs/auditoria/aud-{disc,arch,sec,quality,cov,deps,lang}.md`
- `AUDITORIAS.md` (projeto) § AUD-REPORT; vault `AUDITORIAS.md` (escala)
- `TESTES.md` (projeto) T2-T5, T7-T8, T12, T14-T15; vault `TESTES.md` A10
- `docs/security/pudo-audit.md`, `pudod-install.md`
- `docs/memory/sudo-pudo-riscos.md`, `shells-seguranca.md`, `tst-t*.md`
- `TODO.md` W14-W17; Gate Cosimo (4755 humano)
- CONTRACT §5 (direção de deps); `docs/architecture.md` (mapa local)

*Livro AUD-REPORT. Sem alteração de código de produto. Sem push. Sem endosso de setuid. Sem em-dash.*
