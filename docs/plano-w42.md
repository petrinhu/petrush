# Plano W42: OSH-13..15 (here-doc `<<` / `<<-`)

**Status:** plano CTO (Caetano). Sem implementação neste documento. Sem push. Sem editar `TODO.md` (W41 ainda 🔍 até GHA de `e9ad142`).
**Tipo:** plano de uma onda (anti-OE).
**Audience:** líder + implementer (`backend-engineer`) + orquestrador.
**Last-reviewed:** 2026-08-30.
**Canônico de produto:** [`plano-shell-avancado.md`](plano-shell-avancado.md). Predecessor: [`plano-w41.md`](plano-w41.md).
**Barra desta onda:** POSIX.1-2017 XCU 2.7.4 + bash 4/5 cotidiano. OSH primeiro.

**Decisão autônoma, confirmar retroativamente:** here-doc **é a onda inteira** (três fatias). Não empilhar `set`/`trap` nem arrays: o construto cruza parser + runner de script + Foundation `apply_redirs` + clone/dispatch. W41 já tinha marcado isso como candidato W42 e tirou da W41 porque `apply_redirs` só abre **path**.

---

## O que já está no chão (não reabrir)

| ID | Onda | Superfície |
|----|------|------------|
| OSH-0..12 | W20..W41 | shebang, posicionais, `shift`, `if`/`while`/`for`/`case`, funções, `return`/`local`, `$(cmd)`, `$(( ))`, `[[ ]]` |
| NEW-20 / UX-16 | (pré-onda) | redirs `<` `>` `>>` `2>` `2>>` `2>&1` `&>`; noclobber SEC-09 |
| W41 | HEAD `e9ad142` | OSH-10/11/12 no origin; GATE local ctest+smokes+check 32/32; GHA ainda é o espelho |

Fatos no tree (não inferência):

- Lexer: `try_consume_simple_op` em `src/mid/parser.c` consome **um** `<` como `TOK_LT`. `cat <<EOF` vira `WORD` + `TOK_LT` + `TOK_LT` + `WORD`; `build_stage` exige WORD depois do primeiro `TOK_LT` e **falha**.
- AST: `petrush_cmd_t.redir_in` é `char *` path. `clone_cmd` (`dispatcher.c`) só duplica path.
- Foundation: `apply_redirs` (`process.c`) faz `open(cmd->redir_in, O_RDONLY)`. Mid `run_builtin_with_redirs` **repete** o mesmo bloco (n=2; AUD Q1 / R-I20). Here-doc é a **terceira** variante de stdin: Rule of 3 dispara; extrair `petrush_apply_redirs` **nesta** onda, não numa onda extra.
- Runner: `run_file_lines` (`source.c`) lê **linha a linha**, corta o `\n`, chama `petrush_parse_list` por linha. Newline = separador de comando no runner, não no parser (`isspace` no tokenize). Compounds OSH-3..12 nos smokes cabem **numa** linha física. Here-doc **não cabe**.
- Expand: `expand_word` faz `~` + `$` + `$( )` + `$(( ))`. Glob **não** está em `expand_word` (só em `expand_cmd_argv` nos argv unquoted). Redirs de path já expandem `~`/`$` sem glob.

---

## Meta da onda

Scripts OSH passam a alimentar stdin de um comando com o corpo que vem **depois** da linha do `<<`, até o delimitador, sem arquivo nomeado em `/tmp`:

1. `<<'DELIM'` (quoted, corpo literal) ponta a ponta em script/`source`
2. `<<DELIM` unquoted (expansão `$` / `$( )` / `$(( ))` no corpo)
3. `<<-` (tira tab à esquerda do corpo e da linha do delimitador)

DoD de onda = as três fatias em 🔍 (nunca ✅ direto) + smokes verdes no Docker **fedora:44 clang** + regressão OSH-0..12 e redirs `<`/`>` + `ctest` dos alvos tocados + lint dos TUs tocados. Push só no fim da onda. GHA (matriz já existente) é o espelho remoto; não poll.

Pré-condição de **implementação**: W41 GATE + GHA do SHA `e9ad142` verde. Este plano pode existir no disco antes disso; código W42 não.

---

## Fora de W42 (explícito)

| Fora | Por quê |
|------|---------|
| YSH, tabelas, pipes estruturados | Ordem travada: OSH primeiro |
| Fish UX (highlight/autosuggest no AST) | Depois da linguagem OSH |
| here-string `<<<` | Fora do OSH-v1 (`plano-shell-avancado.md`) |
| backticks `` ` `` | OSH-9 recusou; continua literal |
| coproc, process subst, `n<<` (fd ≠ 0) | petrush não tem `n<` genérico; anti-OE |
| glob `**` e classes `[]` | Política atual de glob |
| `set` / `trap` / `set -e` | Cruza **todo** o dispatch; onda própria (W43+) |
| arrays indexados / associativos | Bash extra; depois de word-eval |
| `--posix` word-eval / IFS split extra | Modo, não construto |
| JOB-1 fg/bg/Ctrl-Z/%n/PTY | Subsystem distinto |
| PS2 / here-doc **interativo** no REPL | `linenoise` é uma linha; onda UX depois. Script + `source` só. |
| compounds multi-linha (`if` quebrado em várias linhas) | Não é here-doc; não “já deixar genérico” o collector |
| instalar `/bin/sh`, bit 4755, testes fora de Docker | Vetos do líder |
| C++ no parser/eval | ADR-001 |
| tempfile nomeado em `/tmp` | CVE-2000-1134 (symlink); ver segurança abaixo |

---

## Arquitetura (recorte fechado)

POSIX XCU 2.3 + 2.7.4: depois de reconhecer `io_here`, o corpo começa na linha **seguinte** ao newline da linha do operador e termina na primeira linha que contém **só** o delimitador + newline (sem blank). Vários `<<` na mesma linha: corpos na ordem dos operadores. O fd **pode** ser pipe (não precisa ser seekable).

### AST (`petrush_cmd_t`)

Campos novos (owned), **sem** reusar `redir_in` como se o delimitador fosse path (senão `apply_redirs` tenta `open("EOF")`):

| Campo | Papel |
|-------|--------|
| `char *here_delim` | delimitador já com quote-removal; NULL = não é here-doc |
| `char *here_body` | corpo **sem** a linha delimitadora; NULL = pendente (parser viu `<<`, runner ainda não encheu) |
| `int here_quoted` | 1 se **alguma** parte do word era quoted (`<<'EOF'` / `<<"EOF"`) |
| `int here_strip` | 1 se operador foi `<<-` |

Last-wins com `< file`: `<<` limpa `redir_in`; `< file` limpa os quatro campos here-*. Vários `<<` no mesmo estágio: **consome** todos os corpos na ordem (senão o runner dessincroniza); stdin efetivo = **último** (igual last-wins de `redir_in`). Não criar lista de here-docs (anti-OE).

`clone_cmd` / `cmd_clear` / `petrush_cmd_free` copiam/liberam os quatro. Sem isso, `for`/`fn` que clonam o body perdem o here-doc.

### Parser

- `TOK_DLESS` (`<<`) e `TOK_DLESSDASH` (`<<-`) em `try_consume_simple_op` **antes** do `<` simples. Sem isso, `<<-EOF` vira `<<` + palavra `-EOF` (footgun).
- Delimitador = próximo `TOK_WORD` (quote-removal já é `scan_word`). Word que **começou** quoted → `here_quoted=1`. Recorte: `<<E"OF"` (quote no meio, token.quoted=0) **não** conta como quoted nesta onda (POSIX “any part of word is quoted” completo = fatia extra; anti-OE).
- Corpo **não** é tokenizado. Parser de uma linha só grava `here_delim` + flags e deixa `here_body == NULL` (pendente).
- API de fill (Mid, usada pelo runner e pelos testes): percorrer a lista parseada, para cada `here_delim` com body NULL, receber linhas até casar o delim (com strip de tabs na linha do delim se `here_strip`). Retorno: 1 = ainda faminto, 0 = todos cheios, -1 = EOF sem delim (erro).

Não mudar `petrush_parse_list` para “arquivo inteiro”: newline continuaria whitespace e `echo a\necho b` viraria um comando só.

### Runner (`run_file_lines`)

Depois de `parse_list` com sucesso, se houver here-doc pendente: ler linhas seguintes (sem tratar `#` como comentário **dentro** do corpo; POSIX: o corpo não é tokenizado). Concatenar com `\n`. Cap **1 MiB** (irmão OSH-9 cmdsubst). Estouro ou EOF sem delim → status ≠0 + stderr, não hang.

Comentários / linhas vazias **fora** do corpo continuam skip como hoje. Linha vazia **dentro** do corpo é conteúdo.

`source` e modo script compartilham o runner: os dois ganham here-doc. REPL (`main.c` linenoise) **não** entra: `cat <<EOF` numa linha só = pendente sem fill → erro de parse/unterminated (não PS2).

### Apply (Foundation)

Extrair `int petrush_apply_redirs(const petrush_cmd_t *cmd)` para `process.h` / `process.c`. `execute_external` / pipeline e `run_builtin_with_redirs` **chamam** a mesma função (Mid só faz save/restore de FDs no caminho builtin).

Here-doc: **não** `open` de path. **Decisão autônoma, confirmar retroativamente:** fd anônimo Linux `memfd_create` (`MFD_CLOEXEC`), `write` do body, `lseek(0)`, `dup2` → stdin. Fallback se `memfd_create` falhar: `open("/dev/shm", O_TMPFILE|O_RDWR)` ou `mkstemp` em `/dev/shm` + `unlink` imediato. **Nunca** nome previsível em `/tmp` (CVE-2000-1134). Pipe + writer no filho deadlocks acima da capacidade do pipe ou deixa zumbi no `exec`; memfd evita os dois.

Ordem de apply inalterada: stdin (path **ou** here, here ganha se `here_body`) → stdout → stderr/merge.

### Expansão (só OSH-14)

POSIX unquoted: parameter + cmdsubst + arith; **sem** tilde, **sem** field split, **sem** pathname, `"` não é especial no corpo (exceto dentro de `$( )` / `${ }`). `expand_word` faz tilde se a linha começa com `~`: **não** chamar cru. Helper local em `expand.c` (primeira ocorrência; sem `heredoc.c`): percorre a linha com a mesma lógica `$` / `$(` / `$((` já existente, **pula** o bloco `~`. Quoted: corpo intacto (nem `$`).

Expandir no dispatch (quando o comando roda), não no parse: `$( )` no corpo vê o ambiente do momento da execução (funções / loop). Corpo clonado permanece cru; expand produz o bytes do memfd.

---

## Fatias (INVEST, serial)

Mesma Onda W42 = **não** paralelizável: as três tocam `parser.c` + `source.c` + `process.c`. Ordem = quoted e2e primeiro (prova o tubo); expand depois; `<<-` por último (W41 já disse “`<<-` depois”).

Pré-req comum: W41 🔍/✅ no remoto, Fedora 44 no Docker, TDD red→green, C23, 4 camadas. Implementer = `backend-engineer`.

Frase-guarda L-21 (verbatim no briefing): uso/contagem de uso no repositório do consumidor NUNCA corta escopo, desenho ou qualidade de um produto feito para distribuição; dor do consumidor é evidência de lacuna, jamais prova de que o que ele não usa pode sair; “ninguém usa X” é afirmação sobre UM repositório, nunca sobre o mundo; regra de paridade: se o motor/biblioteca substituído aceita, o produto novo aceita também (aqui: POSIX `<<` / `<<-` quoted e unquoted), não o mínimo medido no único consumidor conhecido.

### OSH-13: `<<'DELIM'` quoted, ponta a ponta (script)

**História:** Como autor de script POSIX, quero `cat <<'EOF'` seguido de linhas e `EOF` para mandar um corpo literal no stdin, sem o shell expandir `$` e sem criar ficheiro em `/tmp`.

**Escopo:**

- Tokens `TOK_DLESS` / `TOK_DLESSDASH` (dash **tokenizado** já; strip = OSH-15). Nesta fatia `<<-'EOF'` comporta-se como `<<'EOF'` **sem** strip (documentar no smoke: não usar `<<-` como caso verde de strip).
- AST + `cmd_clear` / `clone_cmd`.
- Fill API + `run_file_lines` consome corpos pendentes.
- Extrair `petrush_apply_redirs`; here-doc via memfd; path `<` continua `open`.
- Quoted: corpo byte-a-byte, incluindo `$HOME` literal.
- Unterminated / cap 1 MiB → erro fechado.
- Vários `<<` na mesma linha física: consumir N corpos; stdin = último.

**Fora da fatia:** expansão unquoted, strip de tabs, REPL/PS2, `<<<`, `n<<`.

**Camadas:** Mid parser/source/dispatcher (clone + builtin redir chama a Foundation extraída). Foundation `process.c` / `process.h`. Sem módulo C novo.

**TDD red primeiro (obrigatório):**

1. `tests/test_parser.c`: `cat <<'EOF'` (uma linha) → `here_delim=="EOF"`, `here_quoted==1`, `here_body==NULL`; `"<<" ` não é `TOK_LT` duplo; `<<-` é token distinto de `<<` + `-EOF`. Provar vermelho.
2. Teste de fill: alimentar `hello` + `EOF` → `here_body` com `hello\n` (ou `hello` + newline POSIX: corpo inclui newlines internas; a linha delim **não** entra; newline final do último linha-do-corpo: POSIX inclui os newlines das linhas do corpo, cada uma terminada por `\n`).
3. Green parser + fill + apply.
4. Smoke ponta a ponta.

**DoD da fatia:**

- `test_parser` + `test_process` (here-doc no externo, noclobber/`<` regressão) verdes.
- Smoke `tests/smoke/osh13-heredoc.sh` (≥6: quoted literal `$HOME`; duas linhas; delim errado = ≠0; last-wins dois `<<`; `cat <<'E' | ...` ou `<<` + `>` no mesmo comando; regressão `echo hi < file` inalterado).
- Target CMake `osh13_heredoc` + entrada no target `smoke`.
- Docker fedora:44 clang: `ctest -R 'test_parser|test_process'` + smoke osh13 **e** osh0 (script runner) + um smoke de redir existente se houver.
- clang-tidy parser/dispatcher/process: `misc-no-recursion` só NOLINT pontual na cadeia nova. Sem desligar `WarningsAsErrors`.
- Sem 4755. Sem push.

EXECUTAR TESTE [`ctest -R 'test_parser|test_process'` (host depois Docker fedora:44 clang)] [NA FATIA OSH-13]
EXECUTAR TESTE [`bash tests/smoke/osh13-heredoc.sh $BUILD/petrush` + regressão `osh0-script.sh`] [NA FATIA OSH-13]
EXECUTAR TESTE [`clang-tidy -p=build --quiet src/mid/parser.c src/mid/source.c src/mid/dispatcher.c src/foundation/process.c`] [NA FATIA OSH-13]

---

### OSH-14: `<<DELIM` unquoted (expansão no corpo)

**História:** Como autor de script POSIX, quero `cat <<EOF` com `$VAR`, `$(cmd)` e `$((1+1))` expandidos no corpo, sem glob e sem tilde.

**Escopo (POSIX 2.7.4 unquoted):**

- `here_quoted==0` → no dispatch, expandir o corpo (linha a linha ou stream) com helper em `expand.c` (sem tilde, sem glob, sem split). `$` / `$( )` / `$(( ))` já existem.
- Quoted continua literal (regressão OSH-13).
- `\` no corpo: recorte bash/POSIX “como dentro de aspas duplas” só para `$`, `` ` ``, `\`, newline. Backticks continuam **não** cmdsubst (política OSH-9). `\$` → `$` literal. Outros `\` podem ficar literais (não inflar).
- Expand no **dispatch**, não no parse.

**Fora da fatia:** `<<-` strip, PS2, `` ` `` no corpo.

**Camadas:** Mid expand + dispatcher (ponto de expand antes do apply). Foundation intocado se OSH-13 já entrega o memfd a partir de `here_body` já final.

**TDD red primeiro:**

1. `test_expand` (ou teste de fill+dispatch): corpo unquoted `$FOO` com `FOO=bar` no env → bytes `bar\n`. Quoted `'EOF'` com `$FOO` → literal `$FOO`. Provar vermelho contra OSH-13 (unquoted ainda literal).
2. Green no helper + call site.
3. Smoke.

**DoD da fatia:**

- `test_expand` + `test_parser` verdes.
- Smoke `tests/smoke/osh14-heredoc-expand.sh` (≥6: `$VAR`; `$(echo x)` / builtin; `$((1+1))`; quoted **não** expande; `*` no corpo **não** globa; `~/x` no corpo **não** vira `$HOME/x`).
- Target CMake `osh14_heredoc_expand` + `smoke`.
- Docker fedora:44 clang: expand/parser + osh14 + osh13 + osh9/osh11 (regressão cmdsubst/arith).
- Sem 4755. Sem push.

EXECUTAR TESTE [`ctest -R 'test_expand|test_parser'` (host depois Docker fedora:44 clang)] [NA FATIA OSH-14]
EXECUTAR TESTE [`bash tests/smoke/osh14-heredoc-expand.sh $BUILD/petrush` + `osh13-heredoc.sh` + `osh9-cmdsubst.sh`] [NA FATIA OSH-14]

---

### OSH-15: `<<-` strip de tabs

**História:** Como autor de script indentado, quero `<<-EOF` para o tab à esquerda do corpo (e da linha `EOF`) não ir para o stdin.

**Escopo (POSIX 2.7.4 `<<-`):**

- `here_strip==1` no token `TOK_DLESSDASH`.
- Ao **encher** o corpo: cada linha perde o prefixo de **tabs** (não espaços). A linha do delimitador também: `\t\tEOF` casa com delim `EOF`.
- Espaço à esquerda **não** é tab: não strip, e `\tEOF` vs `EOF` já coberto; ` EOF` (espaço) **não** fecha.
- `<<` (sem dash) não strip (regressão).

**Fora da fatia:** misturar espaços no lugar de tabs; REPL.

**Camadas:** Mid fill (source + parser helper). Sem Foundation nova.

**TDD red primeiro:**

1. `test_parser` fill: linhas `\thello` + `\tEOF` com `<<-` → body `hello\n`; o mesmo com `<<` **não** casa o delim `\tEOF` (unterminated ou delim não encontrado). Provar vermelho.
2. Green no strip.
3. Smoke.

**DoD da fatia:**

- `test_parser` (strip) verde.
- Smoke `tests/smoke/osh15-heredoc-dash.sh` (≥6: strip tab no corpo; delim indentado com tab fecha; espaço não é tab; `<<` sem dash preserva tab; quoted `<<-'E'`; unquoted `<<-EOF` + `$VAR` se OSH-14 já estiver no tree).
- Target CMake `osh15_heredoc_dash` + `smoke`.
- Docker fedora:44 clang: parser + osh15 + osh13 + osh14.
- clang-tidy parser/source. Sem desligar `WarningsAsErrors`.
- Sem 4755. Sem push.

EXECUTAR TESTE [`ctest -R test_parser` (host depois Docker fedora:44 clang)] [NA FATIA OSH-15]
EXECUTAR TESTE [`bash tests/smoke/osh15-heredoc-dash.sh $BUILD/petrush` + `osh13-heredoc.sh` + `osh14-heredoc-expand.sh`] [NA FATIA OSH-15]

---

## Ritual de fim de onda (para colar no `TODO.md` depois)

Não editar a tabela nesta revisão. Quando o agente da tabela abrir W42, copiar as linhas da secção “Texto pronto para `TODO.md`” no relatório do CTO, mais:

| ID sugerido | Texto |
|-------------|--------|
| GATE-ONDA-W42 | NA ONDA ACIMA, todos os testes foram feitos? funcionalidade de cada fatia e da onda garantida? todos os status check verde? version/tag/push autorizados apenas dessa forma; do contrário retorne a quem não tem status concluído e veja o que falta. |

EXECUTAR TESTE [`cmake --build $BUILD --target smoke` (Docker fedora:44 clang; inclui osh0..osh15 + xdg)] [NO FIM DA ONDA W42]
EXECUTAR TESTE [`ctest -R 'test_parser|test_expand|test_process|test_info'` no mesmo container] [NO FIM DA ONDA W42]
EXECUTAR TESTE [`clang-tidy -p=build --quiet src/mid/parser.c src/mid/source.c src/mid/dispatcher.c src/mid/expand.c src/foundation/process.c`] [NO FIM DA ONDA W42]
EXECUTAR TESTE [push só com smokes+ctest+lint verdes; GHA matriz existente; sem poll; sem 4755] [PRE-PUSH W42]

Commit **por fatia** (Conventional: `feat(osh): ...` + ID no corpo). Status no `TODO.md` → 🔍 no **mesmo** commit. Push **só** no fecho da onda.

---

## Ordem de execução e mapa de quem faz

```
OSH-13 quoted+apply+runner  →  OSH-14 unquoted expand  →  OSH-15 <<-  →  GATE-ONDA-W42  →  push
```

| Papel | Quem | Modelo (Grok) |
|-------|------|----------------|
| Plano (este doc) | Caetano / CTO | mais atual |
| Implementação (TDD, C23, Docker) | `backend-engineer` | um abaixo |
| Julgamento (re-roda smoke/ctest) | orquestrador / main | n/a |
| `TODO.md` linhas W42 | agente da tabela (depois do GHA W41) | n/a |

Main **não** implementa. Sem `isolation: worktree`. Build pesado: `TMPDIR=/var/tmp`. Testes **só** Docker (mínimo `fedora:44`; GHA já tem a matriz). Sem display `:0`.

---

## Pré-requisitos de máquina / CI

- Imagem `fedora:44`, clang, CMake do projeto, acutest já no tree.
- Não instalar pacotes de sistema sem ordem do líder (`memfd_create` / `O_TMPFILE` já no kernel/glibc da imagem).
- Não relaxar `-Werror` / `WarningsAsErrors`.
- Não tocar `pudod` / setuid / plugins / `configsh` / ASM.
- Dívida clang-tidy antiga em `expand.c` (fora da lista CI W41) **não** é desta onda: não “aproveitar” para limpar o ficheiro inteiro. Só o helper novo, se o tidy do TU passar a falhar por causa dele.

---

## Segurança (Narciso, by design)

- CVE-2000-1134: here-doc com nome em `/tmp` + symlink. Mitigação = fd anônimo (`memfd_create` / `O_TMPFILE`). Teste de DoD: o smoke **não** cria ficheiro visível em `/tmp` com o corpo (assert: glob `/tmp/petrush*` vazio no caso here-doc, além do tmp do próprio smoke).
- Corpo cap 1 MiB: fail-closed, sem OOM como arma.
- `here_body` é dado do script, não path: nunca passar pelo `open` de `redir_in`.
- Sem 4755.

---

## Candidatos W43+ (não desta onda)

1. **OSH-16** `set` mínimo (`-e` `-u` `-x`) **ou** `trap` (um dos dois, não os dois).
2. **JOB-1** process group + fg/bg/Ctrl-Z (PTY no container).
3. Arrays indexados; `--posix` word-eval.
4. PS2 / here-doc no REPL; compounds multi-linha no runner.
5. `n<<` fd genérico; `<<E"OF"` “any part quoted” completo.

---

## Decisões autónomas desta revisão (confirmar retroativamente)

1. W42 = **3** fatias, **todas** here-doc. `set`/`trap`/arrays **fora** (onda inteira).
2. Ordem quoted e2e → expand unquoted → `<<-`. Plumbing primeiro; POSIX strip por último.
3. Tokenizar `<<-` já em OSH-13 (senão `-EOF` vira delim); strip só em OSH-15.
4. Quoted = `token.quoted` (word começou com aspas). `<<E"OF"` no meio fica para W43+.
5. Runner line-oriented + fill API; **não** parse do ficheiro inteiro.
6. REPL/PS2 fora. Script + `source` só.
7. Extrair `petrush_apply_redirs` **dentro** de OSH-13 (3ª variante de stdin = Rule of 3), não como ID separado.
8. Entrega do corpo: `memfd_create`, nunca `/tmp` nomeado.
9. Cap 1 MiB (irmão OSH-9).
10. Vários `<<`: consumir N, last-wins no fd 0, sem lista.
11. Expand unquoted **sem** tilde/glob/split; helper em `expand.c`, sem ficheiro novo.
12. `n<<` e `<<<` fora.
13. Sem módulo C novo nesta onda.

---

## Anti-OE (checklist do implementer)

- Não “já deixar” o runner juntar linhas para `if`/`while` multi-linha.
- Não implementar `<<<` junto com `<<`.
- Não criar `heredoc.c` / `redir.c` (primeira/segunda ocorrência).
- Não copiar `open`/`dup2` pela terceira vez no dispatcher: extrair.
- Não tempfile em `/tmp`.
- Não abrir YSH, Fish, backticks, coproc, `**`, `set`/`trap`, arrays.
- Não editar manuais do vault. Não editar `TODO.md` neste plano.
- Red evidenciado **antes** do green, em toda fatia.
- Não cortar `<<-` nem unquoted com o argumento “o smoke atual não usa” (L-21).
