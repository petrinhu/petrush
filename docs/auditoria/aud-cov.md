# AUD-COV - Cobertura de testes (petrush)

| Campo | Valor |
|-------|-------|
| **ID** | AUD-COV |
| **Data** | 2026-08-22 |
| **SHA HEAD (pré-relatório)** | `7b15ecc` |
| **Auditor** | qa-engineer |
| **Escopo** | módulos críticos: `parser`, `expand`, `process`, `dispatcher`, `pudo`, `prompt` (+ helpers `pudod/*` ligados em `test_pudo`) |
| **Manuais** | `Projects/petrush/AUDITORIAS.md` § AUD-COV; vault `TESTES.md` T1 (meta ≥70% linhas nos críticos); A6 indexado no vault sem checklist operacional completo nesta cópia |
| **Porte** | early (Pipeline-Sprint) |
| **Código de produto** | não alterado (só relatório + status TODO) |
| **Push** | não |

## 1. Método

1. Confirmar tooling: `gcov` (GCC 16.1.1) e `lcov` 2.0-7.fc44 presentes; `gcovr` **ausente** (não instalado; gap de tooling só de DX, não bloqueia).
2. Build isolado em `/var/tmp/petrush-aud-cov` com `-DENABLE_COVERAGE=ON` (`TMPDIR=/var/tmp`, `-j2`).
3. Exercitar: `cmake --build … --target check` (ctest 18/18) + smoke `tests/smoke/pudo-smoke.sh` (53/0) no binário instrumentado.
4. `lcov --capture` → extract dos paths críticos → `genhtml` + parse do `.info` (o `lcov --list` do 2.0 exibiu Rate corrompida; números canônicos = genhtml / parser Python do `.info`).
5. Classificar contra meta T1 (≥70% **linhas** nos módulos críticos). Branches reportadas como evidência; sem meta dura no `AUDITORIAS.md` do projeto.

**Artefatos brutos (fora do git):** `/var/tmp/petrush-aud-cov/`, `/var/tmp/petrush-aud-cov-report/` (`coverage_critical.info`, `html-critical/`, logs).

## 2. Ambiente e suíte

| Item | Valor |
|------|-------|
| Configure | `cmake -S . -B /var/tmp/petrush-aud-cov -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON` |
| Compilador | GNU 16.1.1 (`--coverage -O0 -g`) |
| Unit (`check`) | **18/18 PASS** (0.42 s) |
| Smoke | **53/0 PASS** |
| Capture | `lcov --capture --directory /var/tmp/petrush-aud-cov --rc branch_coverage=1` |

## 3. Números: módulos críticos

Escopo extract: `src/mid/{parser,expand,dispatcher,pudo,prompt}.c`, `src/foundation/process.c`, `src/pudod/{allow_resolve,child_argv,target_check,target_open}.c`.

| Arquivo | Linhas | Funções | Branches | vs meta 70% linhas |
|---------|--------|---------|----------|--------------------|
| `mid/parser.c` | **90.9%** (378/416) | 100% (17/17) | 80.5% (240/298) | OK |
| `mid/expand.c` | **85.1%** (286/336) | 100% (14/14) | 69.5% (182/262) | OK |
| `mid/prompt.c` | **93.9%** (46/49) | 100% (1/1) | 69.2% (27/39) | OK |
| `mid/dispatcher.c` | **67.7%** (359/530) | 91.9% (34/37) | 57.8% (215/372) | **abaixo** |
| `mid/pudo.c` | **68.0%** (183/269) | 100% (14/14) | 57.8% (126/218) | **abaixo** |
| `foundation/process.c` | **65.4%** (183/280) | 86.7% (13/15) | 59.4% (95/160) | **abaixo** |
| `pudod/allow_resolve.c` | 76.9% (10/13) | 100% (1/1) | 58.3% (7/12) | OK |
| `pudod/child_argv.c` | 77.8% (14/18) | 100% (1/1) | 61.1% (11/18) | OK |
| `pudod/target_check.c` | 100% (10/10) | 100% (1/1) | 100% (8/8) | OK |
| `pudod/target_open.c` | 100% (7/7) | 100% (2/2) | 100% (2/2) | OK |
| **TOTAL críticos** | **76.6%** (1476/1928) | **95.1%** (98/103) | **65.7%** (913/1389) | **agregado OK** |

### Contexto `src/` completo (não-meta)

Após remove de `tests/` + `vendor/`: **77.5%** linhas (2000/2580), 91.7% funções, 64.6% branches. Abaixo de 70% linhas também: `mid/dirstack.c` 64.7% (fora do escopo AUD-COV crítico).

## 4. Lacunas observáveis

### 4.1 Funções com 0 hits (críticos)

| Módulo | Função | Nota |
|--------|--------|------|
| `process.c` | `signal_name` | helper de mensagem de sinal |
| `process.c` | `pipeline_abort` | caminho de erro de pipeline |
| `dispatcher.c` | `builtin_env` | builtin sem case dedicado na suíte atual |
| `dispatcher.c` | `builtin_unalias` | idem |
| `dispatcher.c` | `builtin_dirs` | idem |

Demais funções dos críticos têm ≥1 hit. Em `pudo.c` todas as funções foram tocadas; o déficit é de **linhas/branches** (ramos de erro, allow-list, path de elevação).

### 4.2 Hotspots de linhas sem hit (amostra)

- **`process.c`:** ~97 linhas miss; ranges iniciais 21-33 (`signal_name`), ramos de `apply_redirs` / abort.
- **`dispatcher.c`:** ~171 linhas miss; builtins `env`/`unalias`/`dirs` + ramos de redir/erro em builtins cobertos só no happy path.
- **`pudo.c`:** ~86 linhas miss; ramos de config/allow/sanitize/`run_via_pudod` (elevação real não exercitada sem helper privilegiado).

### 4.3 Gap `pudod.c` (binário helper)

- `src/pudod/pudod.c` **compila com `--coverage`** (`.gcno` + símbolos `__gcov0.*` no binário).
- **Nenhum `.gcda`** após executar `./pudod /usr/bin/true` (deny por allow-list ausente, exit 1).
- Causa: `pudod.c` termina com **`_exit(rc)`** de propósito (comentário: evitar atexit hooks em setuid). `_exit` **não** dispara o flush do gcov → cobertura do `main` do helper **não é mensurável** com o runtime atual.
- Helpers (`allow_resolve`, `child_argv`, `target_*`) entram na tabela porque `test_pudo` os linka e sai via `exit` normal.
- **Não** é recomendação casual trocar `_exit`→`exit` (impacto de segurança setuid). Alternativas futuras: `__gcov_dump()` antes do `_exit`, ou harness que exercita a lógica sem o `main` setuid.

### 4.4 Tooling

| Ferramenta | Status |
|------------|--------|
| gcov / lcov | OK (usados) |
| llvm-cov | presente no PATH; não necessário (GCC build) |
| gcovr | **ausente**; não instalado nesta passada |
| `lcov --list` (2.0) | display Rate/Num inconsistente; contornar com genhtml / parse `.info` |

## 5. Veredicto

| Critério | Resultado |
|----------|-----------|
| Suíte verde sob coverage | PASS (18 unit + 53 smoke) |
| Agregado críticos ≥70% linhas (T1) | **PASS** (76.6%) |
| Cada crítico ≥70% linhas | **FAIL parcial** (`process` 65.4%, `dispatcher` 67.7%, `pudo` 68.0%) |
| Branches críticos | 65.7% (informativo; sem meta de projeto) |
| `pudod.c` mensurável | **GAP** (`_exit` engole gcov) |

**Veredicto:** **APROVAR COM RESSALVAS**.

Cobertura agregada dos críticos passa o piso de 70% linhas; três núcleos ficam logo abaixo; o helper `pudod.c` continua cego ao gcov por desenho de `_exit`. Prioridade de remediação (sem aplicar nesta fatia):

1. **IMPORTANTE:** testes que exercitem `builtin_env`, `builtin_unalias`, `builtin_dirs` + `pipeline_abort` / `signal_name`.
2. **IMPORTANTE:** casos negativos em `pudo.c` (config ausente/malformada, sanitize, candidato a pudod rejeitado) sem exigir setuid.
3. **COSMÉTICO / tooling:** `__gcov_dump()` antes de `_exit` em `pudod.c` (ou unit do corpo sem `main`) para fechar o buraco de medição; opcional instalar `gcovr` userland.

## 6. O que esta auditoria não cobre

- Mutation score (qualidade dos asserts).
- Coverage diferencial só de código novo (relatório é absoluto no HEAD medido).
- Elevação real setuid / `4755` (fora de escopo; alinhado a AUD-SEC).
- CI job de coverage (não existe ainda no GHA).

## 7. Reprodução

```bash
export TMPDIR=/var/tmp
COV=/var/tmp/petrush-aud-cov
OUT=/var/tmp/petrush-aud-cov-report
cmake -S . -B "$COV" -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
cmake --build "$COV" -j2 --target check petrush pudod
bash tests/smoke/pudo-smoke.sh "$COV/petrush"
mkdir -p "$OUT"
lcov --capture --directory "$COV" --output-file "$OUT/coverage_raw.info" --rc branch_coverage=1
lcov --extract "$OUT/coverage_raw.info" \
  '*/src/mid/parser.c' '*/src/mid/expand.c' '*/src/foundation/process.c' \
  '*/src/mid/dispatcher.c' '*/src/mid/pudo.c' '*/src/mid/prompt.c' \
  '*/src/pudod/*.c' \
  --output-file "$OUT/coverage_critical.info" --rc branch_coverage=1
genhtml "$OUT/coverage_critical.info" -o "$OUT/html-critical" --branch-coverage
```
