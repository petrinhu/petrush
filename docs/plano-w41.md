# Plano W41: OSH-10..12 (case, $(( )), [[ ]])

**Status:** plano CTO (Caetano). Sem implementação neste documento. Sem push. Sem editar `TODO.md` (outro agente fecha W30).
**Tipo:** plano de uma onda (anti-OE).
**Audience:** líder + implementer (`backend-engineer`) + orquestrador.
**Last-reviewed:** 2026-08-30.
**Canônico de produto:** [`plano-shell-avancado.md`](plano-shell-avancado.md).
**Barra desta onda:** POSIX.1-2017 + bash 4/5 cotidiano. OSH primeiro.

**Decisão autônoma, confirmar retroativamente:** três fatias (não quatro). POSIX que desbloqueia script (`case`; `$( )` já existe; irmão `$(( ))`) mais o `[[ ]]` que o bash cotidiano escreve dentro do `if` já entregue. Here-doc, `set`/`trap`, arrays, `--posix` e JOB-1 ficam para ondas seguintes.

---

## O que já está no chão (não reabrir)

| ID | Onda | Superfície |
|----|------|------------|
| OSH-0..9 | W20..W40 | shebang, posicionais, `shift`, `if`/`while`/`for`, funções, `return`/`local`, `$(cmd)` |
| XDG-1 | W32 | rc/history XDG |
| FEAT-TEST | W13 | builtins `test` / `[` (primaries curtos; sem `[[`, sem `-a`/`-o`/`!`) |
| W30 CI | GHA | 14/14 verde no SHA `1437719`; matriz fedora:44 + ubuntu + debian + arch + CachyOS |

Fato no tree (OSH-9 deixou o gancho): `petrush_cmdsubst_span` devolve 0 para `$((`; smoke `osh9-cmdsubst.sh` caso 7 e `test_expand` `osh9_arith_not_cmdsubst` **exigem** que `echo $((1+1))` saia literal. OSH-11 **tem** de atualizar esses gêmeos no mesmo commit da expansão (L-17).

Parser: `PETRUSH_ITEM_{PIPELINE,IF,WHILE,FOR,FN}`. Dispatcher `clone_list` / `petrush_list_free` / `dispatch_list` precisam de um `kind` novo por compound. Expand: hook DIP só para `$( )`.

Foundation `process.c` `apply_redirs` só abre **path** em `redir_in`. Por isso here-doc **não** entra em W41 (tocaria Foundation + AST de redir).

---

## Meta da onda

Scripts OSH passam a ter os três compostos que o bash cotidiano usa depois de `if`/`for`/`$( )`:

1. `case`/`esac` (POSIX)
2. expansão aritmética `$(( expr ))` (POSIX)
3. `[[ ... ]]` mínimo (bash cotidiano, não POSIX)

DoD de onda = as três fatias em 🔍 (nunca ✅ direto) + smokes verdes no Docker **fedora:44 clang** + regressão OSH-0..9 + `ctest` dos alvos tocados + lint dos TUs tocados. Push só no fim da onda. GHA (matriz já existente) é o espelho remoto; não poll.

---

## Fora de W41 (explícito)

| Fora | Por quê |
|------|---------|
| YSH, tabelas, pipes estruturados | Ordem travada: OSH primeiro |
| Fish UX (highlight/autosuggest no AST) | Depois da linguagem OSH |
| backticks `` ` `` | OSH-9 recusou; continua literal |
| coproc, process subst, `<<<` | Fora do OSH-v1 (`plano-shell-avancado.md`) |
| glob `**` e classes `[]` | Política atual de glob (`*` `?` só) |
| here-doc `<<` / `<<-` | Foundation + redir; onda própria (candidato W42) |
| `set` / `trap` / `set -e` | Cruza **todo** o dispatch; onda própria |
| arrays indexados / associativos | Bash extra; depois de word-eval |
| `--posix` word-eval / IFS split extra | Modo, não construto; depois |
| JOB-1 fg/bg/Ctrl-Z/%n/PTY | Subsystem distinto (`job.c` já cobre só `&`); onda seguinte se W41 fechar |
| `;&` `;;&` (bash case) | Não POSIX; anti-OE |
| `=~` regex em `[[` | Bash extra; anti-OE |
| `for ((` C-style, `(( ))` comando aritmético | Não é `$(( ))` expansão |
| instalar `/bin/sh`, bit 4755, testes fora de Docker | Vetos do líder |
| C++ no parser/eval | ADR-001 |

---

## Fatias (INVEST, serial)

Mesma Onda W41 = **não** paralelizável: as três tocam `src/mid/parser.c`. Ordem = dependência de regressão (OSH-11 quebra o smoke OSH-9 de propósito) e de `if [[`.

Pré-req comum: OSH-9 🔍/✅ no tree, Fedora 44 no Docker, TDD red→green, C23, 4 camadas (Mid = parser/expand/dispatcher; Foundation intocado).

### OSH-10: `case` / `esac`

**História:** Como autor de script POSIX, quero `case word in pat) list ;; esac` para ramificar sem encadear `if`.

**Escopo (POSIX XCU 2.9.4, recorte):**

- AST novo `PETRUSH_ITEM_CASE` + `petrush_case_t` (word owned; braços: padrões `char **` + body `petrush_list_t`).
- Palavra do `case` expandida **uma** vez (`expand_word`).
- Padrões: `*` `?` via `petrush_glob_match` (já existe). Alternação `pat1|pat2`. Primeiro match ganha.
- Terminator de braço: só `;;`. `esac` fecha. `"esac"` quoted **não** fecha (irmão de `"fi"` / `"done"`).
- `in` obrigatório. Lista vazia de braços → status 0 (nada rodou), igual `for` sem words.
- Status = último comando do braço que rodou; 0 se nenhum padrão casou.

**Fora da fatia:** `;&` `;;&`, classes `[]` no padrão, `case` em posição de palavra (só keyword de comando).

**Camadas:** Mid `parser.c` / `parser.h` / `dispatcher.c` (`clone_list`, free, dispatch). Sem Foundation.

**TDD red primeiro (obrigatório):**

1. `tests/test_parser.c`: `case x in y) echo a ;; esac` → `kind == PETRUSH_ITEM_CASE`; `"esac"` quoted não termina; `a|b)` dois padrões. Rodar e **provar vermelho**.
2. Green mínimo no parser + dispatch.
3. Smoke ponta a ponta.

**DoD da fatia:**

- `test_parser` inclui os casos novos e a suíte antiga verde.
- Smoke `tests/smoke/osh10-case.sh` (≥6 casos: match simples, `|`, `*`, nenhum match, quoted `esac`, status).
- Target CMake `osh10_case` + entrada no target `smoke`.
- Docker fedora:44 clang: `ctest -R test_parser` + smoke osh10 **e** osh3/osh5 (regressão compound).
- clang-tidy no parser/dispatcher: `misc-no-recursion` só com NOLINT pontual na cadeia nova (irmão CI-TIDY-RECURSE / CI-TIDY-DISP). Sem desligar `WarningsAsErrors`.
- Sem 4755. Sem push.

EXECUTAR TESTE [`ctest -R test_parser` (host depois Docker fedora:44 clang)] [NA FATIA OSH-10]
EXECUTAR TESTE [`bash tests/smoke/osh10-case.sh $BUILD/petrush` + regressão `osh3-if.sh` `osh5-for.sh`] [NA FATIA OSH-10]
EXECUTAR TESTE [`clang-tidy -p=build --quiet src/mid/parser.c src/mid/dispatcher.c`] [NA FATIA OSH-10]

---

### OSH-11: `$(( expr ))` (expansão aritmética)

**História:** Como autor de script POSIX, quero `$((1+1))` virar `2` na palavra, sem forking.

**Escopo (POSIX XCU 2.6.4, recorte):**

- Lexer: `petrush_arith_span` (irmão de `petrush_cmdsubst_span`). `$((` **não** é cmdsubst (já é 0). Span conta parênteses; aspas internas protegem.
- Expand: se span>0, avalia o interior e concatena o decimal (como `$( )` concatena stdout).
- Expressão: inteiros signed 64 via `petrush_parse_i64` (ilha ASM já existe; fallback C se `PETRUSH_ASM=OFF`). Operadores `+ - * / %`, unário `+ -`, parênteses. Identificador / `$n` / `$VAR` = valor inteiro (vazio ou não-numérico → 0, estilo bash cotidiano).
- Divisão ou módulo por zero: falha fechada (expansão vazia **e** o comando que contém a palavra sai ≠0, ou a palavra vira `0` **com** mensagem em stderr). Escolher **uma** e cobrir no smoke; decisão autônoma recomendada: stderr + status ≠0 do comando, palavra `0` (não crash, não UB).
- Sem assignment dentro de `$(( ))`, sem `++` `--`, sem bitwise, sem ternário, sem vírgula, sem `$(( $(( )) ))` aninhado nesta fatia (um nível; `$VAR` sim).

**Gêmeos (obrigatório no mesmo commit):**

- `tests/smoke/osh9-cmdsubst.sh` caso `arith-not-cmdsubst` deixa de exigir literal; vira regressão “não é cmdsubst” (span cmdsubst continua 0) **ou** sai do smoke OSH-9 e o OSH-11 assume o valor `2`.
- `tests/test_expand.c` `osh9_arith_not_cmdsubst` idem.
- `test_parse_osh9_arith_not_cmdsubst_span` **permanece** (prova que cmdsubst não engole `$((`).

**Camadas:** Mid `parser.c` (span) + `expand.c` (eval). Sem AST de item novo. Sem Foundation. **Não** criar `arith.c` nesta onda (primeira ocorrência; DRY regra de 3).

**TDD red primeiro:**

1. `test_expand`: `expand_word("$((1+1))")` → `"2"` com hook de cmdsubst **NULL** (prova que não passou pelo `$( )`). Provar vermelho contra o comportamento OSH-9.
2. Green no span + eval.
3. Smoke.

**DoD da fatia:**

- `test_expand` + `test_parser` (span) verdes.
- Smoke `tests/smoke/osh11-arith.sh` (≥6: `1+1`, `$VAR`, unário, parênteses, concat `pre$((1))post`, div0 sem crash; backticks continuam literais).
- Target CMake `osh11_arith` + `smoke`.
- Docker fedora:44 clang: ctest expand/parser + osh11 + osh9 regressão.
- Sem 4755. Sem push.

EXECUTAR TESTE [`ctest -R 'test_expand|test_parser'` (host depois Docker fedora:44 clang)] [NA FATIA OSH-11]
EXECUTAR TESTE [`bash tests/smoke/osh11-arith.sh $BUILD/petrush` + `osh9-cmdsubst.sh` (gêmeos `$((` atualizados)] [NA FATIA OSH-11]

---

### OSH-12: `[[ ... ]]` mínimo (bash cotidiano)

**História:** Como autor de script bash 4/5, quero `if [[ -f f && $x == pat* ]]; then` sem o builtin `[` partir a lista em `&&`.

**Escopo (bash cotidiano, recorte):**

- Keyword de comando `[[` ... `]]` -> AST `PETRUSH_ITEM_DBRACKET` (não é o builtin `[`). `&&` `||` `!` **dentro** não são conectores de lista.
- Primaries = os de FEAT-TEST já existentes: `-f -d -e -z -n = != -eq -ne -lt -gt`. Reusar a lógica (hoje `static` em `dispatcher.c`); **não** extrair ficheiro novo (segunda ocorrência; regra de 3).
- `==` aceito como sinónimo de `=` (bash cotidiano). Lado direito **unquoted** de `=` / `==` / `!=` casa com `petrush_glob_match`; quoted → strcmp.
- `]]` fecha. `"]]"` quoted não fecha.
- Status 0/1/2 como `test`. Erro de operador → 2, não crash.

**Fora da fatia:** `=~`, `-a`/`-o` estilo `test`, `<` `>` locale, `-le`/`-ge` (FEAT-TEST também não tem), `]]` como argv de comando externo.

**Camadas:** Mid parser + dispatcher. `if [[ ... ]]; then` já funciona se `[[` for um item da lista da condição (OSH-3). Sem Foundation.

**TDD red primeiro:**

1. `test_parser`: `[[ -f x ]]` → `PETRUSH_ITEM_DBRACKET`; `[[ a == b && c == d ]]` = **um** item (não duas pipelines). Provar vermelho.
2. Green parse + eval.
3. Smoke com `if [[ ... ]]`.

**DoD da fatia:**

- `test_parser` + caminho de eval (via `test_info` chamando dispatch, ou smoke; não inflar `dispatcher.c` com harness se smoke cobre).
- Smoke `tests/smoke/osh12-dbracket.sh` (>=6: `-f` true/false, `&&` curto-circuito, `==` glob, quoted `]]`, `if [[ ... ]]; then`, não-operador -> !=0).
- Target CMake `osh12_dbracket` + `smoke`.
- Docker fedora:44 clang: parser + osh12 + osh3 + osh10.
- clang-tidy dispatcher/parser, NOLINT pontual. Sem desligar `WarningsAsErrors`.
- Sem 4755. Sem push.

EXECUTAR TESTE [`ctest -R test_parser` (host depois Docker fedora:44 clang)] [NA FATIA OSH-12]
EXECUTAR TESTE [`bash tests/smoke/osh12-dbracket.sh $BUILD/petrush` + `osh3-if.sh` + `osh10-case.sh`] [NA FATIA OSH-12]

---

## Ritual de fim de onda (para colar no `TODO.md` depois)

Não editar a tabela nesta revisão. Quando o agente da tabela abrir W41, copiar:

| ID sugerido | Texto |
|-------------|--------|
| GATE-ONDA-W41 | NA ONDA ACIMA, todos os testes foram feitos? funcionalidade de cada fatia e da onda garantida? todos os status check verde? version/tag/push autorizados apenas dessa forma; do contrário retorne a quem não tem status concluído e veja o que falta. |

EXECUTAR TESTE [`cmake --build $BUILD --target smoke` (Docker fedora:44 clang; inclui osh0..osh12 + xdg)] [NO FIM DA ONDA W41]
EXECUTAR TESTE [`ctest -R 'test_parser|test_expand|test_info'` no mesmo container] [NO FIM DA ONDA W41]
EXECUTAR TESTE [`clang-tidy -p=build --quiet src/mid/parser.c src/mid/dispatcher.c src/mid/expand.c`] [NO FIM DA ONDA W41]
EXECUTAR TESTE [push só com smokes+ctest+lint verdes; GHA matriz existente; sem poll; sem 4755] [PRE-PUSH W41]

Commit **por fatia** (Conventional: `feat(osh): ...` + ID no corpo). Status no `TODO.md` -> 🔍 no **mesmo** commit. Push **só** no fecho da onda.

---

## Ordem de execução e mapa de quem faz

```
OSH-10 case  →  OSH-11 $(( ))  →  OSH-12 [[ ]]  →  GATE-ONDA-W41  →  push
```

| Papel | Quem | Modelo (Grok) |
|-------|------|----------------|
| Plano (este doc) | Caetano / CTO | mais atual |
| Implementação (TDD, C23, Docker) | `backend-engineer` | um abaixo |
| Julgamento (re-roda smoke/ctest) | orquestrador / main | n/a |
| `TODO.md` linhas W41 | agente da tabela (depois) | n/a |

Main **não** implementa. Sem `isolation: worktree`. Build pesado: `TMPDIR=/var/tmp`. Testes **só** Docker (mínimo `fedora:44`; GHA já tem a matriz). Sem display `:0`.

---

## Pré-requisitos de máquina / CI

- Imagem `fedora:44`, clang, CMake do projeto, acutest já no tree.
- Não instalar pacotes de sistema sem ordem do líder.
- Não relaxar `-Werror` / `WarningsAsErrors`.
- Não tocar `pudod` / setuid / plugins / `configsh` / ASM (só **usar** `petrush_parse_i64` e `petrush_glob_match`).

---

## Candidatos W42+ (não desta onda)

1. **OSH-13** here-doc `<<EOF` (unquoted + quoted delimiter; `<<-` depois). Toca `redir_in` / `apply_redirs`.
2. **OSH-14** `set` mínimo (`-e` `-u` `-x`) **ou** `trap` (um dos dois, não os dois).
3. **JOB-1** process group + fg/bg/Ctrl-Z (PTY no container).
4. Arrays indexados; `--posix` word-eval; `;&`; `=~`.

---

## Decisões autónomas desta revisão (confirmar retroativamente)

1. W41 = **3** fatias (OSH-10/11/12), não 4: here-doc saiu porque é Foundation.
2. Ordem case → arith → `[[ ]]`: POSIX compound primeiro; arith aproveita o gancho OSH-9; `[[ ]]` por último porque o valor está em `if [[` (já existe `if`).
3. Recorte `[[ ]]` = FEAT-TEST + `&&` `||` `!` + `==` glob. Sem `=~`.
4. Recorte `$(( ))` = `+ - * / % ( )` int64. Sem assignment, sem bitwise, sem aninhamento.
5. Recorte `case` = `;;` + glob `*` `?`. Sem `;&`.
6. Sem módulo C novo nesta onda.
7. Divisão por zero: sem crash; stderr + status ≠0; palavra `0`.
8. JOB-1 **não** é W41 mesmo sendo “próximo no plano-mãe”: tamanho de onda inteira.

---

## Anti-OE (checklist do implementer)

- Não “já deixar genérico” o parser de `[[` para regex.
- Não implementar `(( ))` comando junto com `$(( ))` expansão.
- Não mover `feat_test_*` para header público.
- Não abrir YSH, Fish, backticks, coproc, `**`.
- Não editar manuais do vault. Não editar `TODO.md` neste plano.
- Red evidenciado **antes** do green, em toda fatia.
