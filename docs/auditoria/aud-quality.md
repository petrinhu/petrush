# AUD-QUALITY - Qualidade de código (petrush)

| Campo | Valor |
|-------|-------|
| **ID** | AUD-QUALITY |
| **Data** | 2026-08-22 |
| **SHA HEAD (pré-relatório)** | `e4cdc7b` |
| **Auditor** | tech-lead |
| **Escopo** | `src/` (Front/Mid/Foundation/`pudod`), complexidade, dead code, duplicação / Rule of 3; cruzamento TST-T2 |
| **Manuais** | `Projects/petrush/AUDITORIAS.md` § AUD-QUALITY; vault I9 (limites CCN); CONTRACT DRY; `.clang-tidy` (cognitive ≤ 50) |
| **Porte** | early (Pipeline-Sprint) |
| **Código de produto** | não alterado nesta fatia (só relatório + status TODO) |
| **Push** | não |

## 1. Método

1. Cruzar pré-reqs: TST-T2 (lint) e TST-T14 (smoke já `🔍`).
2. Re-rodar `cmake --build build --target cppcheck` e `clang-tidy` (exit codes).
3. Instalar `lizard` 1.24.0 userland (`pip3 install --user`); analisar `src/ -l c`.
4. Inventariar god-files (`wc -l`), CCN top, cognitive complexity pontual fora da lista do target.
5. Dead code: símbolos `petrush_*` exportados vs usos em `src/` + `tests/`; nota sobre `--suppress=unusedFunction`.
6. Duplicação / Rule of 3: redirs Mid×Foundation (AUD-ARCH F6), expand de paths de redir, vocabulário de operadores parser×highlight.
7. Classificar achados; recomendar patches **sem** aplicar código de produto.

**Ferramentas:** lizard 1.24.0, cppcheck 2.21.1, clang-tidy LLVM 22, `nm`, `rg`, `wc`. **tokei** ausente no PATH; LOC via `wc -l` (5144 linhas em `src/**/*.c`).

Logs brutos: `/var/tmp/petrush-aud-quality/` (`lizard.csv`, `cognitive.txt`).

## 2. Cruzamento TST-T2 (lint após extração)

| Gate | Resultado nesta AUD-QUALITY | Nota |
|------|-----------------------------|------|
| **cppcheck** (`build` target) | **exit 0** | Confirma célula TODO TST-T2 (`🔍`, gate re-rodado 2026-08-22) |
| **clang-tidy** (`build` target) | **exit 0** | Mesmo |
| **TST-T14** | pré-req `🔍` (smoke 53/0) | Não re-executado aqui; aceito como evidência da tabela |

### Lacuna de cobertura do target `clang-tidy`

`CMakeLists.txt` lista **apenas**: `main.c`, `parser.c`, `dispatcher.c`, `pudo.c`, `process.c`, `job.c`, `env.c`, `pudod.c`.

**Fora do gate** (e com cognitive > 50 se analisados isolados):

| Função | Arquivo | Cognitive (limiar 50) |
|--------|---------|------------------------|
| `completion_cb` | `src/front/complete.c` | **67** |
| `expand_brace` | `src/mid/expand.c` | **60** |
| `expand_word` | `src/mid/expand.c` | **56** |
| `glob_word` | `src/mid/expand.c` | **52** |
| `petrush_hl_scan` | `src/front/highlight.c` | **52** |

Consequência: **TST-T2 verde ≠ complexidade sob controle em todo o tree**. O gate cobre o núcleo histórico (parser/dispatcher/process/pudo), não expand/UX Front. Achado de **higiene de tooling**, severidade IMPORTANTE.

Arquivos cobertos (`parser`/`process`/`pudo`/`dispatcher`/`prompt`) **não** estouram cognitive 50 nesta passada (dívida antiga de `tokenize` / `execute_pipeline_with_hook` / `find_pudod_binary` já abaixo do limiar ou silenciada só onde aplicável: `NOLINTNEXTLINE` em `petrush_parse_pipeline`).

## 3. Métricas lizard (ciclomática)

Comando: `lizard src/ -l c -C 10` (e corte analítico CCN > 15 / > 20).

| Métrica | Valor | Meta vault I9 | Leitura |
|---------|-------|---------------|---------|
| NLOC total (lizard) | 4057 | - | Código efetivo nas funções |
| Funções | 160 | - | - |
| **Avg CCN** | **7.6** | ≤ 5 | Acima da média-alvo |
| **Max CCN** | **40** (`match_op`) | ≤ 15 | Estoura crítico |
| CCN > 10 | 42 | identificar | 26% das funções |
| CCN > 15 | 22 | refatorar | - |
| CCN > 20 | 12 | urgente | - |
| Warning cnt (C>10∨len>100∨params>5) | 43 | - | Fun Rt 0.27 |

### Top 12 CCN (ciclomática)

| CCN | NLOC | Função | Arquivo |
|-----|------|--------|---------|
| 40 | 26 | `match_op` | `front/highlight.c` |
| 32 | 94 | `expand_brace` | `mid/expand.c` |
| 31 | 83 | `completion_cb` | `front/complete.c` |
| 28 | 100 | `glob_word` | `mid/expand.c` |
| 28 | 57 | `petrush_hl_scan` | `front/highlight.c` |
| 25 | 81 | `expand_cmd_argv` | `mid/expand.c` |
| 25 | 76 | `petrush_parse_list` | `mid/parser.c` |
| 24 | 73 | `expand_word` | `mid/expand.c` |
| 24 | 56 | `load_allow_list` | `pudod/pudod.c` |
| 23 | 61 | `prompt_render` | `mid/prompt.c` |
| 23 | 50 | `load_pudo_config` | `mid/pudo.c` |
| 21 | 56 | `petrush_parse_pipeline` | `mid/parser.c` |

**Nota sobre `match_op` (CCN 40):** cadeia densa de `if` de longest-match de operadores (`2>&1`, `>>`, `&&`, …). NLOC baixo (26). CCN alto é artefato de tabela de casos, não god-procedure. Preferível tabela de literais / loop a “explodir” em 10 funções de 2 linhas. Classificar como **IMPORTANTE (métrica)**, não como urgência de redesign.

**Hotspot real:** `expand.c` (várias funções CCN 24-32 **e** cognitive > 50) + `completion_cb` + monólito `dispatcher.c`.

## 4. God files / god functions

Limiar vault I9 (adaptado C): arquivo > 300 linhas = cheiro; função NLOC > ~30 / CCN > 15 = atenção.

| Arquivo | Linhas (`wc`) | Papel | Severidade |
|---------|---------------|-------|------------|
| `src/mid/dispatcher.c` | **952** | builtins + redirs + bg + `test`/`[` | 🔴 CRÍTICO (god-file) |
| `src/mid/pudo.c` | 734 | cliente pudo | 🔴 |
| `src/mid/parser.c` | 651 | tokenize + AST | 🔴 |
| `src/mid/expand.c` | 531 | brace/glob/$VAR | 🔴 |
| `src/foundation/process.c` | 513 | fork/exec/pipeline/redirs | 🔴 |
| `src/pudod/pudod.c` | 314 | helper | 🟠 (limiar) |

`dispatcher.c` concentra a tabela `builtins[]` e dezenas de `builtin_*` + `run_builtin_with_redirs` + FEAT-TEST. AUD-ARCH já encaminhou; aqui confirma-se como **dívida de manutenibilidade #1** (não é bug funcional).

Funções longas (length lizard > 100): `expand_brace` (110), `glob_word` (114), `execute_external` (106), `execute_pipeline_with_hook` (108), `main` pudod (121), `main` petrush (113). Nenhuma é “morta”; todas no caminho quente ou no helper.

## 5. Dead code

| Verificação | Resultado |
|-------------|-----------|
| APIs `petrush_*` exportadas sem uso em `src/` | Várias com U=0 **no binário petrush**, mas **todas** referenciadas em `tests/` e/ou no próprio módulo (ex.: `petrush_parse`, `petrush_hl_scan`, `petrush_job_probe`, `petrush_job_reset_for_tests`) |
| API só-teste | `petrush_job_reset_for_tests`, `petrush_complete_count`, `petrush_history_hint` (probe) - **intencional**, não morto |
| cppcheck `unusedFunction` | **Suprimido** no target (`--suppress=unusedFunction`) - dead-code automático **cego** no gate |
| Blocos `#if 0` / stubs óbvios em `src/` | Não encontrados nesta varredura |

**Veredito dead code:** sem achado CRÍTICO de função pública órfã. Risco residual: suppress de `unusedFunction` + lista parcial do clang-tidy. Recomendação: em onda futura, rodar cppcheck **sem** suppress numa passada de auditoria (não no CI blocking early), ou `clang-tidy` `misc-unused-*` pontual.

## 6. Duplicação e Rule of 3

Critério da casa: **terceira cópia** obriga extrair; duas cópias = dívida nomeada (ainda reversível barata).

### Q1 - IMPORTANTE - Redirs `open`/`dup2` Mid × Foundation (2 cópias)

| | |
|--|--|
| **Onde** | `apply_redirs` em `src/foundation/process.c` (~141-199); bloco equivalente em `run_builtin_with_redirs` em `src/mid/dispatcher.c` (~104-162) |
| **Padrão** | Mesmos flags SEC-09 (`O_EXCL` vs `O_APPEND`), mesmas mensagens de erro, mesma ordem stdin→stdout→stderr/merge |
| **Rule of 3** | **Ainda não dispara** (n=2). AUD-ARCH F6 já documentou. |
| **Porém** | Mid reimplementa primitiva de Foundation; qualquer terceira variante (ex.: bg-job redirs extras) **obriga** extrair `petrush_apply_redirs()` compartilhada. |
| **Mitigação** | Extrair helper Foundation usado pelo filho **e** pelo caminho builtin (com save/restore de FDs no Mid). Sem mudança nesta fatia. |

### Q2 - IMPORTANTE (micro) - Expand de três paths de redir (n=3)

| | |
|--|--|
| **Onde** | `expand_cmd_argv` em `src/mid/expand.c` (~520-529): três blocos idênticos `redir_in` / `redir_out` / `redir_err` |
| **Rule of 3** | **Dispara** (terceira cópia literal). |
| **Esforço** | S (loop sobre `char **` ou helper `expand_inplace(char **slot)`). |
| **Reversibilidade** | two-way. |

### Q3 - LOW/MEDIUM - Vocabulário de operadores em dois lugares (n=2)

| | |
|--|--|
| **Onde** | Parser: `try_consume_simple_op` / `try_consume_err_redir` (`parser.c`); Front: `match_op` (`highlight.c`) |
| **Contexto** | Highlight **propositalmente** não chama o parser (comentário UX-21). Duplicar a tabela de ops é trade-off anti-acoplamento. |
| **Rule of 3** | Não dispara. Risco: novo operador (ex.: `>&`) atualizado num lado e esquecido no outro. |
| **Mitigação** | Se surgir 3º consumidor, extrair tabela neutra Foundation/`op_table.h` sem puxar AST. Até lá: teste de paridade opcional ou comentário cruzado. |

### Q4 - OK - alias / Rule of 3 consciente

`src/mid/alias.c` declara no cabeçalho “Rule of 3, anti-OE”: tabela simples sem over-engineering. Alinhado.

## 7. Achados consolidados

Severidade: vault **CRÍTICO / IMPORTANTE / COSMÉTICO**.

### 🔴 CRÍTICO (qualidade / manutenibilidade)

| ID | Achado | Evidência |
|----|--------|-----------|
| **Q-GOD-01** | God-file `dispatcher.c` (952 linhas) | `wc`; tabela builtins + redirs + FEAT-TEST + bg |
| **Q-CCN-01** | 12 funções com CCN > 20; avg CCN 7.6 > meta 5 | lizard |
| **Q-COG-01** | Cognitive > 50 em `expand_*` / `completion_cb` / `hl_scan` **fora** do gate clang-tidy | clang-tidy pontual |

### 🟠 IMPORTANTE

| ID | Achado | Evidência |
|----|--------|-----------|
| **Q-T2-GAP** | Target `clang-tidy` omite `expand.c`, `complete.c`, `highlight.c`, … | `CMakeLists.txt` L514-521 |
| **Q-DRY-01** | Redirs duplicadas Mid×Foundation (n=2; pré-Rule-of-3) | F6 / Q1 |
| **Q-DRY-02** | Três blocos idênticos de expand em redirs | Q2 |
| **Q-CCN-02** | `match_op` CCN 40 (tabela densa; NLOC 26) | lizard; tratar como métrica, não redesign |

### 🟢 COSMÉTICO

| ID | Achado |
|----|--------|
| **Q-TOOL-01** | tokei ausente; LOC via `wc` |
| **Q-TOOL-02** | cppcheck com `--suppress=unusedFunction` (dead-code cego no CI) |
| **Q-DOC-01** | `docs/memory/tst-t2-estatica.md` ainda descreve gate falho antigo; TODO já `🔍` verde (drift de relatório T2) |

## 8. Score e veredito

| Dimensão | Nota (0-20) | Critério |
|----------|-------------|----------|
| Complexidade (CCN/cognitive) | 10 | Avg>5, 12× CCN>20, cognitive hotspots fora do gate |
| God-files / tamanho | 10 | Cinco `.c` > 500; dispatcher 952 |
| Dead code | 18 | Sem órfão público real; suppress residual |
| DRY / Rule of 3 | 14 | Uma violação micro (Q2); F6 em n=2; ops×2 consciente |
| Alinhamento tooling (TST-T2) | 13 | Lint verde no subconjunto; lacuna de lista |
| **Total** | **65 / 100** | |

**Veredito:** **APROVADO COM RESSALVAS** para porte early.

Não há indício de código morto estrutural nem de “quarta cópia” escondida de redirs. Há dívida clara de complexidade/god-file e um gap de lint que mascara cognitive em expand/Front. Nenhuma mudança de produto aplicada nesta fatia.

**Não marca ✅** (só `🔍`). ✅ fica para consolidação TST/AUD / AUD-REPORT.

## 9. Patches recomendados (ordem; não aplicados)

1. **TOOL** - Estender target `clang-tidy` com `expand.c`, `complete.c`, `highlight.c`, `alias.c`, `source.c`, `prompt.c`, `hist_expand.c`, `dirstack.c`, `front/rc_trust.c` (e unidades `pudod` auxiliares se desejado). Aceitar `NOLINT` pontual **ou** extrair átomos até cognitive ≤ 50. Re-rodar TST-T2.
2. **S / Rule of 3** - Helper `expand_inplace(char **)` para os três redirs em `expand_cmd_argv` (Q2).
3. **M** - Extrair `petrush_apply_redirs` (+ variante save/restore) antes de uma terceira cópia (Q1/F6).
4. **L** - Fatiar `dispatcher.c`: `builtins_test.c`, `builtins_alias.c`, tabela + dispatch fino (Q-GOD-01). Preferir fatias com testes já existentes (`test_info`, alias, …).
5. **M** - Decompor `expand_brace` / `glob_word` / `completion_cb` (CCN + cognitive).
6. **DOC** - Atualizar `docs/memory/tst-t2-estatica.md` com o gate verde atual (Q-DOC-01); opcional: instalar `tokei` userland para próximas AUDs.

**Reversibilidade:** todos two-way exceto se alguém remover API pública no fatia do dispatcher (evitar).

## 10. Evidências (comandos)

```text
cmake --build build --target cppcheck    # EXIT 0
cmake --build build --target clang-tidy  # EXIT 0
lizard src/ -l c -C 10
clang-tidy -p=build --checks='-*,readability-function-cognitive-complexity' \
  src/mid/expand.c src/front/complete.c src/front/highlight.c
find src -name '*.c' -exec wc -l {} +
```

## 11. Relação com outros itens

| ID | Relação |
|----|---------|
| **TST-T2** | Pré-req; revalidado verde; gap de lista documentado (Q-T2-GAP) |
| **TST-T14** | Pré-req `🔍` |
| **AUD-ARCH** | F6 = Q1; god-file dispatcher confirmado |
| **AUD-DEPS** | Acoplamento linenoise no Mid (fora do escopo quality profundo) |
| **AUD-LANG** | Idioms C23; cognitive/NOLINT |
| **AUD-REPORT** | Consolida score 65 e patches 1-6 |

## 12. Checklist de saída AUD-QUALITY

- [x] Complexidade lizard coletada (avg/max/top)
- [x] Cognitive amostrada além do gate
- [x] God-files inventariados
- [x] Dead code varrido (API + suppress)
- [x] Rule of 3 / duplicação (Q1-Q4)
- [x] Cruzamento TST-T2 (lint exit 0 + lacuna)
- [x] Relatório em `docs/auditoria/aud-quality.md`
- [ ] Status TODO `🔍` (este commit)
- [ ] ✅ só após julgamento do orquestrador / fechamento AUD

*Relatório de qualidade. Sem refactor de produto. Sem push. Sem em-dash.*
