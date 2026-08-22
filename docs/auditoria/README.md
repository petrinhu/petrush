# Dossiê de auditoria petrush (índice mestre)

| Campo | Valor |
|-------|-------|
| **Item** | AUD-REPORT (W16) |
| **Data** | 2026-08-22 |
| **SHA HEAD (pré-commit do livro)** | `618aa2c` |
| **Auditor** | internal-auditor (dono do livro) |
| **Porte** | early (Pipeline-Sprint; `.bigtech-porte`) |
| **Manuais** | `AUDITORIAS.md` (projeto) § AUD-REPORT; vault `AUDITORIAS.md` (escala 🔴🟠🟢); vault `TESTES.md` A10 |
| **Código de produto** | não alterado (`src/` intocado) |
| **Push** | não |
| **Setuid `4755`** | **NÃO ENDOSSADO** |

Livro consolidado: [`aud-report.md`](aud-report.md).

Este `README.md` é o **índice mestre** do dossiê (escopo, sumário, contagem, mapa de capítulos). Não existe `00_indice_mestre.md` à parte: o contrato desta fatia pediu o índice aqui, alinhado aos capítulos já nomeados `aud-*.md`.

---

## 1. Escopo

petrush é shell REPL C23 (Opção A) + helper `pudod` opcional. Porte early: livro proporcional (arquitetura, segurança, qualidade, cobertura, deps, idioma, descoberta). Fora: AUD-DB, AUD-API, AUD-UI, AUD-FRAMEWORK (poda do `AUDITORIAS.md` do projeto).

**In:** capítulos W15 abaixo + cruzamento TST-T2/T3/T4/T5/T7/T8/T12/T14/T15.  
**Out:** PoC ofensiva, aplicar setuid/setcap no host, push remoto, mudança de `src/`.

---

## 2. Sumário executivo

| Eixo | Resultado |
|------|-----------|
| **Score global** | **73 / 100** |
| **Veredicto** | **APROVADO COM RESSALVAS** para uso **unpriv** / REPL local (porte early) |
| **Setuid `4755` / setcap** | **NO-GO.** Não endossado nesta passada. Gate humano (`DEPLOY_CHECKLIST` + `docs/security/`). |
| **Produção privilegiada** | **NO-GO** até P0 (fail-closed sem `pudod` em Release) + revisão do líder |
| **Memory / secrets / SCA** | T4 0 leak/UB; T8 0 leaks; T5/T12 0 CRITICAL vendor; CVE-2025-9810 MEDIUM mitigado (SEC-08) |
| **Binário Release** | Full RELRO + PIE + NX + canary; Fortify parcial; artefato `pudod` local `755` (sem bit setuid) |
| **Cobertura T1** | Agregado críticos **76.6%** linhas (passa ≥70%); `process` / `dispatcher` / `pudo` logo abaixo |
| **SEC-01..10** | Presentes no código vivo (status TODO permanece `🔍`) |

O livro é honesto: lista o que falhou. Não maquia god-file, fallback `sudo`, nem o gap de gcov em `pudod.c` (`_exit`). Nenhum 🔴 fica sem plano de remediação. Achado sem evidência no capítulo-fonte **não entra**.

---

## 3. Contagem de achados (após dedup)

Classificação vault: 🔴 CRÍTICO / 🟠 IMPORTANTE / 🟢 COSMÉTICO.

| Severidade | Qtd | Nota de consolidação |
|------------|-----|----------------------|
| 🔴 CRÍTICO | **3** | Processo/privilégio: gate 4755, fallback Boundary B, política de allow-list. **Não** há 🔴 de memory corruption aberto. |
| 🟠 IMPORTANTE | **22** | Arquitetura (Mid+linenoise), qualidade/cobertura, residuals SEC-I*, idioma C23, atribuição BSD-2 |
| 🟢 COSMÉTICO | **11** | Tooling, drift menor de doc, gnu2x vs c23 estrito |
| **Total** | **36** | Dedup entre capítulos (F1 = D1, etc.) |

Reclassificação consciente (sem inflar): AUD-QUALITY marcou god-files/CCN como 🔴 I9 de manutenibilidade. No livro, **sem crash/UB/priv-esc associado**, esses itens sobem como 🟠. O achado não some; a escala de segurança não é misturada com tamanho de arquivo. Detalhe e IDs: [`aud-report.md`](aud-report.md) §4.

Estado Auditado (coluna do livro): `-` aberto / `✓` controle presente e revalidado / `⚠` residual ou gate.

---

## 4. Mapa capítulo → especialista → veredicto

| Capítulo | Arquivo | Especialista | Score | Veredicto |
|----------|---------|--------------|-------|-----------|
| AUD-DISC | [`aud-disc.md`](aud-disc.md) | security-engineer | 82 | Superfície + STRIDE prontos; setuid não endossado |
| AUD-ARCH | [`aud-arch.md`](aud-arch.md) | software-architect | 70 | APROVADO COM RESSALVAS (F1 Mid+linenoise) |
| AUD-SEC | [`aud-sec.md`](aud-sec.md) | security-engineer | 76 (derivado no livro) | APROVAR COM RESSALVAS; 4755 não endossado |
| AUD-QUALITY | [`aud-quality.md`](aud-quality.md) | tech-lead | 65 | APROVADO COM RESSALVAS (CCN / god-file / gap tidy) |
| AUD-COV | [`aud-cov.md`](aud-cov.md) | qa-engineer | 73 (derivado no livro) | APROVAR COM RESSALVAS (agregado OK; 3 núcleos <70%) |
| AUD-DEPS | [`aud-deps.md`](aud-deps.md) | software-architect | 72 | APROVADO COM RESSALVAS (D1 = F1) |
| AUD-LANG | [`aud-lang.md`](aud-lang.md) | backend-engineer | 71 | APROVADO COM RESSALVAS (C23 no compile; idioms fracos) |
| **AUD-REPORT** | [`aud-report.md`](aud-report.md) | internal-auditor | **73** | Este livro |

Pesos do score global (stake early + helper privilegiado opcional): SEC 25 · ARCH 15 · QUALITY 15 · COV 15 · DISC 10 · DEPS 10 · LANG 10. Aritmética em [`aud-report.md`](aud-report.md) §2.

---

## 5. Roadmap de remediação (P0 → P3)

Não aplicado nesta fatia (livro só). Ordem para o orquestrador / próxima onda de código:

| Pri | O quê | Fecha |
|-----|-------|-------|
| **P0** | Manter **4755 bloqueado**. Fail-closed em Release se `pudod` ausente; remover `execvp("sudo")`. Allow-list sem shells genéricos. | R-C1, R-C2, R-C3, R-I1 |
| **P1** | Sincronizar `pudo-audit.md` / `shells-seguranca.md` com SEC-03..10. `NOTICE` BSD-2 do linenoise. | R-I4, R-I16 |
| **P2** | Porta Front para `clear` + history (tirar linenoise do Mid). Mover `rc_trust` para Foundation/Mid. Header `complete.h` sem vendor. | R-I8, R-I9, R-I13 |
| **P3** | Estender target clang-tidy; testes `env`/`unalias`/`dirs` + negativos de `pudo`; Rule of 3 em expand redirs; `static_assert` de limites; fatiar `dispatcher.c`. | R-I17..R-I28 |

C-level: Cláudio (compliance/licença D5), Narciso (CISO, 4755 e Boundary B), Caetano (CTO, P2/P3). Decisão de 4755 é **sempre** do líder.

---

## 6. Parecer para auditor externo

O dossiê W15+W16 está **coeso e rastreável** (capítulo, evidência, ID, remediação). Adequado a revisão externa do **caminho unpriv**.

**Não** apresentar este livro como autorização para instalar `pudod` setuid. O artefato Release local está `755`. CMake **não** aplica 4755. Smoke/CI cobrem deny sem privilégio.

Próximo item de tabela: NEW-23 (wiki + beginner-guide pós-tag), pré-req = AUD-REPORT em `🔍` (impl do livro; `✅` só após julgamento do orquestrador / TST-AUD de fechamento).

---

## 7. Anti-padrões evitados

1. Auditor interno não refez SAST/SCA: orquestrou via capítulos e consolidou.
2. Índice mestre existe (este arquivo) + livro (`aud-report.md`).
3. Todo achado do livro cita evidência de capítulo.
4. Os 3 🔴 têm plano (P0).
5. Falhas (fallback sudo, CCN, cobertura por módulo, `_exit`/gcov) estão no texto, não omitidas.

*Índice AUD-REPORT. Sem alteração de produto. Sem push. Sem em-dash.*
