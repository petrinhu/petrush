# Plano W43: OSH-16..18 (`set` mínimo: `-x` / `-u` / `-e`)

**Status:** plano CTO (Caetano). Sem implementação neste documento. Sem push. Sem editar `TODO.md` (W42 acaba de ir a GHA no SHA `639e624`; código W43 só depois do GHA verde).
**Tipo:** plano de uma onda (anti-OE).
**Audience:** líder + implementer (`backend-engineer`) + orquestrador.
**Last-reviewed:** 2026-08-31.
**Canônico de produto:** [`plano-shell-avancado.md`](plano-shell-avancado.md). Predecessor: [`plano-w42.md`](plano-w42.md).
**Barra desta onda:** POSIX.1-2017 XCU `set` + 2.5.2 (`$?` `$-`) + 2.8.1. OSH primeiro. Recorte dash-like, não bash-4.4+ `inherit_errexit`.

**Decisão autônoma, confirmar retroativamente:** `set` **é a onda inteira** (três fatias). Não empilhar `trap`, arrays nem JOB-1. W42 já tinha marcado OSH-16 como `set` **ou** `trap` (um dos dois); `set -e` sozinho cruza **todo** o dispatch (`dispatch_list`, `if`/`while`/`for`/`case`/`fn`, `&&`/`||`, runner de script). `trap` é o candidato natural de W44.

---

## O que já está no chão (não reabrir)

| ID | Onda | Superfície |
|----|------|------------|
| OSH-0..15 | W20..W42 | shebang, posicionais, `shift`, `if`/`while`/`for`/`case`, funções, `return`/`local`, `$(cmd)`, `$(( ))`, `[[ ]]`, here-doc `<<`/`<<-` |
| FEAT-NOCLOBBER / SEC-09 | W11 / W3 | noclobber **sempre ligado** (`O_EXCL`); sem `set -C` |
| UX-23 | W9 | `&` + tabela `job.c`; **sem** `fg`/`bg`/Ctrl-Z/`%n` |
| FEAT-TRUE | W10 | builtins `true` / `false` / `:` |

Fatos no tree (não inferência):

- Builtins em `src/mid/dispatcher.c` L43-78: **não há** `set` nem `trap`.
- Posicionais: `petrush_positional_set` / `shift` em `expand.c` (OSH-1/2). `set -- a b` ainda não existe; `$0` já é preservável via `petrush_positional_get(0)`.
- Especiais POSIX: `$0` `$1` `$#` `$@` `$*` existem. **Não há** `$?` `$-` `$$` `$!` (`expand.c` ~L669).
- `dispatch_list` (`dispatcher.c` L1120): `&&`/`||` short-circuit; status local; **não aborta** o resto da lista nem o runner. `run_file_lines` (`source.c` L98-106) continua após status ≠0.
- Abort já existente: `g_returning` / `g_return_status` (OSH-7 `return`). Erro de expansão arith: `petrush_take_arith_error()` (OSH-11). Padrão a copiar, **não** reusar o flag de `return`.
- `job.h` L5: “Sem fg/bg/Ctrl-Z/%n/wait builtin”. `main.c` L155-157: `SIGTSTP` = `SIG_IGN`.
- `builtin_info` (`dispatcher.c` L2004): linha Anti-OE ainda diz `sem set -C`.
- HEAD `639e624` = OSH-15. GATE-ONDA-W42 local verde; GHA é o espelho (não poll).

---

## Meta da onda

Scripts OSH passam a ter o trio POSIX que o bash cotidiano escreve no topo (`set -eu` / `set -eux`; **sem** `pipefail`):

1. builtin `set` + `set --` posicionais + `set -x` (xtrace) + `$?` + `$-`
2. `set -u` (nounset): expansão de parâmetro unset é erro
3. `set -e` (errexit): comando/pipeline que falha aborta o script, com as isenções POSIX

DoD de onda = as três fatias em 🔍 (nunca ✅ direto) + smokes verdes no Docker **fedora:44 clang** + regressão OSH-0..15 + `ctest` dos alvos tocados + lint dos TUs tocados. Push só no fim da onda. GHA (matriz já existente) é o espelho remoto; não poll.

Pré-condição de **implementação:** W42 GATE + GHA do SHA `639e624` verde. Este plano pode existir no disco antes disso; código W43 não.

---

## Fora de W43 (explícito)

| Fora | Por quê |
|------|---------|
| YSH, tabelas, pipes estruturados | Ordem travada: OSH primeiro |
| Fish UX (highlight/autosuggest no AST) | Depois da linguagem OSH |
| `trap` / EXIT / ERR / DEBUG | Cruza sinais + dispatch; W42: um dos dois; candidato **W44** |
| arrays indexados / associativos | Bash extra; onda própria depois de word-eval |
| JOB-1 `fg`/`bg`/Ctrl-Z/`%n`/PTY | Subsystem distinto (`job.c` + SIGTSTP hoje ignorado) |
| `set -o pipefail` | Bash, não POSIX; `false \| true` continua status 0 |
| `set -a`/`-f`/`-v`/`-n`/`-m`/`-b`/`-h` | Cada um é política nova; anti-OE |
| `set +C` desligar noclobber | Produto: noclobber always-on (SEC-09) |
| `! pipeline` (reserved word) | petrush não tem `!` como reserved de pipeline; não “já deixar” |
| `until` | OSH-4 é só `while` |
| `$$` `$!` `PS4` variável | `$$` trivial mas não desbloqueia `set`; `$!` é job; PS4 = `+ ` fixo |
| `${VAR:?}` / `${VAR?}` | Irmão de nounset; fatia extra. `-u` cobre o caso cotidiano |
| `set +o` dump reinput; `set -o` tabela bash | Formato POSIX “unspecified”; não inflar |
| here-doc REPL/PS2; `n<<`; `<<E"OF"` any-part-quoted | Residual W42; não misturar |
| backticks, coproc, process subst, `<<<`, glob `**` | Fora do OSH-v1 |
| instalar `/bin/sh`, bit 4755, testes fora de Docker | Vetos do líder |
| C++ no parser/eval | ADR-001 |

---

## Arquitetura (recorte fechado)

Fonte: POSIX.1-2017 XCU `set` (special builtin) + 2.5.2 + 2.8.1. Abordagem usual (L-22): **dash-like** (referência `/bin/sh` POSIX), não o pântano bash 4.4+ (`set -E`, ERR trap, `inherit_errexit`).

### Estado (`expand.c`, sem ficheiro novo)

Irmão dos posicionais e do `g_arith_error`. Sem `shellopt.c` / `set.c` (primeira ocorrência).

| API | Papel |
|-----|--------|
| `petrush_shellopt_set(char flag, int on)` | liga/desliga `e` `u` `x` (e só esses nesta onda) |
| `petrush_shellopt_get(char flag)` | 0/1 |
| `petrush_shellopt_flags(void)` | string tipo `$-` (`C` sempre + `eux` se ligados; ordem estável, ex. `Ceux`) |
| `petrush_last_status_set(int)` / `petrush_last_status(void)` | `$?` (decimal do pipeline mais recente) |
| `petrush_take_nounset_error(void)` | irmão arith: 1 se a última expand falhou por `-u` |
| `petrush_shellopt_reset_for_tests(void)` | zera e/u/x e `$?`; **não** desliga noclobber |

`C` em `$-` **sempre** (noclobber de produto). Não é bit que `set +C` apague.

### Builtin `set` (Mid, `dispatcher.c`)

Special builtin. Parse:

- `set` sem args: dump `environ` no formato POSIX `"%s=%s\n"` (subset: só o que está no environ; sem funções). Quoting fino de reinput **fora** (anti-OE).
- `set --` / `set -- a b`: `petrush_positional_set($0, nargs, args)`; **preserva `$0`**. Sem args depois de `--` → `$#=0`.
- `set a b` (args que não começam com `-`/`+`): idem, vira posicionais.
- Letras agrupadas: `set -x`, `set +x`, `set -xu` (quando as duas existirem). `+` desliga.
- `-o errexit` / `nounset` / `xtrace` = aliases de `-e`/`-u`/`-x` (POSIX UP). Outro nome (`pipefail`, `vi`, …) → stderr + status ≠0.
- `set -C` / `set -o noclobber`: sucesso, no-op (já ligado).
- `set +C` / `set +o noclobber`: **erro** (não se desliga segurança).
- Flag ainda não honrada na fatia corrente: **erro unknown** (honesto). Em OSH-16, `set -e` e `set -u` falham; `set -eux` só fica verde no fim de OSH-18.
- Unknown (`-z`, `-o pipefail`): stderr `petrush: set: …: invalid option` + status ≠0. Script/`source`: aborta o runner (2.8.1 special builtin error, non-interactive shall exit). REPL: não mata o processo.

### `$?` e `$-` (OSH-16, expand)

Em `expand_word` / brace, ao lado de `$#`: `?` → decimal de `petrush_last_status()`; `-` → `petrush_shellopt_flags()`. Dispatcher grava `$?` **depois de cada item** de `dispatch_list` (pipeline, compound, `[[`, fn def). Default 0 no boot.

### `set -x` (OSH-16)

POSIX: “write to stderr a trace for each command after it expands and before it executes”. PS4 **fixo** `+ ` (espaço). Sem variável `PS4`.

- Traçar comando simples (builtin e externo) com argv já expandido, campos separados por espaço.
- Pipeline: um `+ ` por estágio.
- **Não** traçar as keywords `if`/`then`/`fi`; traçar os comandos **dentro**.
- `set -x` **não** se traça a si (flag liga depois). `set +x`: POSIX unspecified; **decisão autônoma:** traça (dash/bash comum).
- Não traçar o corpo interno de `$( )` no pai (o filho, se `-x` herdasse, inflaria; cmdsubst hoje é fork+_exit **sem** copiar flags; manter: `-x` do pai **não** liga no filho do cmdsubst nesta onda).

### `set -u` (OSH-17)

POSIX: expandir parâmetro **unset** (exceto `@` e `*`) → mensagem stderr + expansion error (2.8.1: non-interactive **shall exit**; interactive shall not).

Dispara em: `$UNSET`, `${UNSET}`, `$1` quando `$#=0`, `${#UNSET}`.

**Não** dispara em:

- `${UNSET:-word}` / `${UNSET-word}` / `${UNSET:+word}` / `${UNSET+word}` (o operador existe para isto)
- `$@` `$*` sem posicionais (exceção POSIX explícita)
- variável set mas vazia (`UNSET=`; `-u` não é `-z`)
- here-doc **quoted** (corpo literal; OSH-13)

Here-doc **unquoted** (OSH-14): o helper `expand_heredoc_body` **também** honra `-u` (mesmo take).

Sinal: `g_nounset_error` + `petrush_take_nounset_error()`, irmão arith. Dispatcher após `expand_cmd_argv` / expand de word de `case`/`[[`: se take=1, não executa o comando; status ≠0; script aborta (mesmo caminho de 2.8.1 que OSH-18 usa para `-e`, porque expansion error shall exit **mesmo sem** `-e`).

REPL: stderr + a lista corrente para; processo petrush **vive**.

### `set -e` (OSH-18)

POSIX `set -e`: comando que falha → shell exit, **exceto**:

1. Falha de um estágio **individual** em pipeline de ≥2 comandos: só a falha do **pipeline** (status = último estágio, sem pipefail).
2. `-e` ignorado ao executar a compound-list depois de `while` / `if` / `elif` (condição). petrush não tem `until`. petrush não tem `!` reserved de pipeline → isenção `!` **não se aplica** (não inventar o reserved).
3. Qualquer comando de uma AND-OR list **que não seja o último**.
4. Se o status de um compound (não subshell) veio de falha **enquanto `-e` era ignorado**, `-e` não se aplica a esse compound (`if false; then :; fi` status 0, script segue).

Mecanismo: `g_no_errexit` (contador) à volta de `dispatch_list` da **condição** de `if`/`elif`/`while`. Em `dispatch_list`, depois de cada item com `status != 0` e `-e` on e `g_no_errexit==0`:

- se o item faz parte de cadeia AND-OR e **não** é o último (`items[i+1].cond` é `AND` ou `OR`, ou o próprio `cond` é AND/OR e o próximo **não** é ALWAYS/fim) → não aborta;
- senão → `g_shell_abort = 1` (flag **novo**, não reusar `g_returning`) e quebra o `for`.

`run_file_lines`: após `dispatch_list`, se abort → `break` e devolve o status. Script OSH-0: petrush sai com esse status. `source`: devolve o status ao caller e **não** lê o resto do ficheiro.

Funções: dash-like: `-e` vale **dentro** do body; falha aborta o script, não só a função. Sem `set -E`.

cmdsubst: **não** dispara `-e` no pai (contrato OSH-9: status inner não vira status do pai). Documentar: dash-like, **não** bash (`x=$(false)` no bash com `-e` mata o pai).

`&`: `dispatch_pipeline_background` já devolve 0; não aborta. O filho do background **não** herda abort do pai nesta onda.

### Camadas

Mid: `dispatcher.c` (builtin + xtrace + errexit + abort), `expand.c` (flags, `$?`, `$-`, nounset), `source.c` (parar o runner). Foundation intocado. Front intocado. Sem módulo C novo.

---

## Fatias (INVEST, serial)

Mesma Onda W43 = **não** paralelizável: as três tocam `dispatcher.c` + `expand.c`. Ordem = plumbing observável (`$?`/`-x`) → nounset (expand) → errexit (dispatch).

Pré-req comum: W42 🔍/✅ no remoto, Fedora 44 no Docker, TDD red→green, C23, 4 camadas. Implementer = `backend-engineer`.

Frase-guarda L-21 (verbatim no briefing): uso/contagem de uso no repositório do consumidor NUNCA corta escopo, desenho ou qualidade de um produto feito para distribuição; dor do consumidor é evidência de lacuna, jamais prova de que o que ele não usa pode sair; “ninguém usa X” é afirmação sobre UM repositório, nunca sobre o mundo; regra de paridade: se o motor/biblioteca substituído aceita, o produto novo aceita também (aqui: POSIX `set -e`/`-u`/`-x` e `set --`), não o mínimo medido no único consumidor conhecido.

### OSH-16: builtin `set` + `set --` + `-x` + `$?` + `$-`

**História:** Como autor de script POSIX, quero `set -- args`, ver `$?` do último comando, ligar `set -x` e ver o rastreio em stderr, e inspecionar `$-`.

**Escopo:**

- Entrada `set` na tabela de builtins + `builtin_set`.
- `set --` / `set -- a b` / `set a b` reescreve posicionais, `$0` intacto.
- `set -x` / `set +x` / `set -o xtrace` / `set +o xtrace`.
- `$?` e `$-` (`C` sempre; `x` quando `-x`).
- `set` sem args: dump environ.
- `set -C` no-op 0; `set +C` erro.
- Unknown option / `-o pipefail` → erro. `set -e` e `set -u` **ainda unknown** (fatias seguintes).
- `help`/`info`: mencionar `set`; Anti-OE deixa de dizer “sem set -C” e passa a “noclobber always-on; sem trap/arrays/pipefail/fg”.
- Reset de testes.

**Fora da fatia:** honrar `-e`/`-u`; `trap`; PS4 variável; dump com quoting de reinput.

**Camadas:** Mid dispatcher + expand. `source.c` só se o erro de special builtin no script precisar abortar o runner: **sim**, já nesta fatia para `set -z` em script (2.8.1). Introduzir `g_shell_abort` **aqui** (serve OSH-17/18). Não “já deixar” as isenções de `-e`.

**TDD red primeiro (obrigatório):**

1. `test_info`: `builtin_table_has("set")` (vermelho).
2. `test_expand` (ou info): `set -- a b` → `$1=a` `$#=2` `$0` igual; `set --` → `$#=0`.
3. `false; echo $?` via `dispatch_list` → `1`. `true; echo $?` → `0`.
4. `set -x; echo hi` → stderr contém `+ echo hi` (ou `+ echo hi\n`); stdout `hi`.
5. `set -x; echo $-` contém `x` e `C`.
6. Green. Smoke.

**DoD da fatia:**

- `test_info` + `test_expand` verdes (casos set/`$?`/`$-`).
- Smoke `tests/smoke/osh16-set-x.sh` (≥6: `set --` posicionais; `$?` após `false`; `-x` traça builtin; `+x` para o rastreio; `set` sem args imprime `PATH=` ou similar do environ; unknown `-z` ≠0 em script; `set +C` ≠0; regressão `osh2` shift / `osh0` shebang).
- Target CMake `osh16_set_x` + entrada no target `smoke`.
- Docker fedora:44 clang: `ctest -R 'test_info|test_expand'` + smoke osh16 **e** osh0 + osh13 (here-doc intacto).
- clang-tidy dispatcher/expand/source. Sem desligar `WarningsAsErrors`.
- Sem 4755. Sem push.

EXECUTAR TESTE [`ctest -R 'test_info|test_expand'` (host depois Docker fedora:44 clang)] [NA FATIA OSH-16]
EXECUTAR TESTE [`bash tests/smoke/osh16-set-x.sh $BUILD/petrush` + regressão `osh0-script.sh` + `osh13-heredoc.sh`] [NA FATIA OSH-16]
EXECUTAR TESTE [`clang-tidy -p=build --quiet src/mid/dispatcher.c src/mid/expand.c src/mid/source.c`] [NA FATIA OSH-16]

---

### OSH-17: `set -u` (nounset)

**História:** Como autor de script POSIX, quero `set -u` para o shell recusar `$VAR` unset em vez de expandir vazio.

**Escopo (POSIX `set -u` + 2.8.1 expansion error):**

- `set -u` / `set +u` / `-o nounset`. `$-` ganha `u`.
- `$UNSET` / `${UNSET}` / `$1` com `$#=0` → stderr + take + abort no script.
- Exceções: `$@` `$*`; `${UNSET:-x}`; var set-vazia.
- Here-doc unquoted honra `-u`; quoted não.
- REPL: não mata o processo.
- Sem `-u`, comportamento OSH-1 intacto (unset → vazio).

**Fora da fatia:** `${VAR:?}`; `-e`; `trap`.

**Camadas:** Mid expand (detecção) + dispatcher (take após expand, abort) + source (runner já aborta via OSH-16).

**TDD red primeiro:**

1. `test_expand`: com `-u`, `expand_word("$NOPE")` liga take; sem `-u`, `""` e take=0. `${NOPE:-x}` com `-u` → `x` sem take. Provar vermelho.
2. Green no expand + call site dispatcher.
3. Smoke.

**DoD da fatia:**

- `test_expand` + `test_info` verdes.
- Smoke `tests/smoke/osh17-set-u.sh` (≥6: `$UNSET` aborta script ≠0; `${UNSET:-ok}` segue; `UNSET=` vazia não aborta; `$@` vazio não aborta; quoted here-doc `$UNSET` literal; unquoted here-doc aborta; `set +u` restaura vazio).
- Target CMake `osh17_set_u` + `smoke`.
- Docker fedora:44 clang: expand/info + osh17 + osh16 + osh14 (here-doc expand).
- Sem 4755. Sem push.

EXECUTAR TESTE [`ctest -R 'test_expand|test_info'` (host depois Docker fedora:44 clang)] [NA FATIA OSH-17]
EXECUTAR TESTE [`bash tests/smoke/osh17-set-u.sh $BUILD/petrush` + `osh16-set-x.sh` + `osh14-heredoc-expand.sh`] [NA FATIA OSH-17]

---

### OSH-18: `set -e` (errexit)

**História:** Como autor de script POSIX, quero `set -e` para o script parar no primeiro comando que falha, sem parar nas condições de `if`/`while` nem no lado esquerdo de `&&`/`||`.

**Escopo (POSIX `set -e`, recorte dash-like):**

- `set -e` / `set +e` / `-o errexit`. `$-` ganha `e`. `set -eux` passa a ser válido (as três letras).
- Abort após pipeline/compound com status ≠0, salvo isenções da secção Arquitetura.
- `if false; then echo x; fi; echo y` → imprime `y` (condição isenta; compound não dispara).
- `if true; then false; fi` → aborta (body).
- `false && echo x` → não aborta; `true && false` → aborta (último da AND-OR).
- `false; echo x` → não imprime `x`.
- `true | false` → aborta; `false | true` → não (status do pipeline = último).
- Função: `f() { false; echo inner; }; set -e; f; echo outer` → nem inner nem outer (script).
- cmdsubst: `set -e; x=$(false); echo still` → **imprime still** (OSH-9). Caso de regressão documentado no smoke.
- `set +e` restaura.

**Fora da fatia:** pipefail; `! pipeline`; `trap ERR`; bash inherit_errexit; `until`.

**Camadas:** Mid dispatcher (`g_no_errexit` + AND-OR last) + source (já aborta). Expand intocado se OSH-17 já entrega take.

**TDD red primeiro:**

1. Teste de dispatch (info ou source): script `set -e\nfalse\necho x\n` → status ≠0 e stdout sem `x`. O mesmo sem `-e` imprime `x`. Provar vermelho contra OSH-16.
2. Casos isenção: `if` cond, `true && false` vs `false && echo`.
3. Green. Smoke.

**DoD da fatia:**

- Teste de dispatch/source verde.
- Smoke `tests/smoke/osh18-set-e.sh` (≥8: `false` aborta; `if false` não; body `false` aborta; `false &&` não; `true && false` sim; `false | true` não; `true | false` sim; função aborta o script; cmdsubst `$(false)` **não** aborta o pai; `set -eux` liga os três bits em `$-`).
- Target CMake `osh18_set_e` + `smoke`.
- Docker fedora:44 clang: info/expand/source + osh18 + osh16 + osh17 + osh3 (`if`) + osh4 (`while`).
- clang-tidy dispatcher/expand/source. Sem desligar `WarningsAsErrors`.
- Sem 4755. Sem push.

EXECUTAR TESTE [`ctest -R 'test_info|test_expand|test_source'` (host depois Docker fedora:44 clang)] [NA FATIA OSH-18]
EXECUTAR TESTE [`bash tests/smoke/osh18-set-e.sh $BUILD/petrush` + `osh16-set-x.sh` + `osh17-set-u.sh` + `osh3-if.sh` + `osh4-while.sh`] [NA FATIA OSH-18]

---

## Ritual de fim de onda (para colar no `TODO.md` depois)

Não editar a tabela nesta revisão. Quando o agente da tabela abrir W43, copiar as linhas da secção “Texto pronto para `TODO.md`” no relatório do CTO, mais:

| ID sugerido | Texto |
|-------------|--------|
| GATE-ONDA-W43 | NA ONDA ACIMA, todos os testes foram feitos? funcionalidade de cada fatia e da onda garantida? todos os status check verde? version/tag/push autorizados apenas dessa forma; do contrário retorne a quem não tem status concluído e veja o que falta. |

EXECUTAR TESTE [`cmake --build $BUILD --target smoke` (Docker fedora:44 clang; inclui osh0..osh18 + xdg)] [NO FIM DA ONDA W43]
EXECUTAR TESTE [`ctest -R 'test_parser|test_expand|test_process|test_info|test_source'` no mesmo container] [NO FIM DA ONDA W43]
EXECUTAR TESTE [`clang-tidy -p=build --quiet src/mid/dispatcher.c src/mid/expand.c src/mid/source.c`] [NO FIM DA ONDA W43]
EXECUTAR TESTE [push só com smokes+ctest+lint verdes; GHA matriz existente; sem poll; sem 4755] [PRE-PUSH W43]

Commit **por fatia** (Conventional: `feat(osh): ...` + ID no corpo). Status no `TODO.md` → 🔍 no **mesmo** commit. Push **só** no fecho da onda.

---

## Ordem de execução e mapa de quem faz

```
OSH-16 set+--+-x+$?+$-  →  OSH-17 -u  →  OSH-18 -e  →  GATE-ONDA-W43  →  push
```

| Papel | Quem | Modelo (Grok) |
|-------|------|----------------|
| Plano (este doc) | Caetano / CTO | mais atual |
| Implementação (TDD, C23, Docker) | `backend-engineer` | um abaixo |
| Julgamento (re-roda smoke/ctest) | orquestrador / main | n/a |
| `TODO.md` linhas W43 | agente da tabela (depois do GHA W42) | n/a |

Main **não** implementa. Sem `isolation: worktree`. Build pesado: `TMPDIR=/var/tmp`. Testes **só** Docker (mínimo `fedora:44`; GHA já tem a matriz). Sem display `:0`.

---

## Pré-requisitos de máquina / CI

- Imagem `fedora:44`, clang, CMake do projeto, acutest já no tree.
- Não instalar pacotes de sistema sem ordem do líder.
- Não relaxar `-Werror` / `WarningsAsErrors`.
- Não tocar `pudod` / setuid / plugins / `configsh` / ASM.
- Dívida clang-tidy antiga em `expand.c` (fora da lista nova): não “aproveitar” para limpar o ficheiro inteiro. Só o caminho `$?`/`$-`/`-u`.

---

## Segurança (Narciso, by design)

- `set +C` **não** desliga noclobber (SEC-09 / FEAT-NOCLOBBER). Fail-closed.
- `-x` escreve argv expandido em stderr: smokes **não** dumpam `env` full (T8). Não traçar valores só para “debug do implementer”.
- `-u` é fail-closed de expansão (não executar comando com campo vazio acidental).
- Special builtin `set` com opção inválida aborta script (2.8.1), não segue com flags pela metade.
- Sem 4755.

---

## Candidatos W44+ (não desta onda)

1. **OSH-19** `trap` mínimo (`trap 'cmd' EXIT` + `INT`; sem ERR/DEBUG).
2. **JOB-1** process group + fg/bg/Ctrl-Z (PTY no container).
3. Arrays indexados; `--posix` word-eval.
4. `set -o pipefail`; reserved `!` de pipeline; `until`.
5. `$$` / `$!` / `PS4`; `${VAR:?}`.
6. PS2 / here-doc no REPL; `n<<`; `<<E"OF"` any-part-quoted.

---

## Decisões autónomas desta revisão (confirmar retroativamente)

1. W43 = **3** fatias, **todas** `set`. `trap`/arrays/JOB-1 **fora** (ondas próprias).
2. Ordem `-x`+plumbing → `-u` → `-e`. Observável primeiro; abort de dispatch por último.
3. Recorte **dash-like**, não bash-4.4+ errexit em funções/cmdsubst.
4. `set -e`/`-u` unknown até a fatia que os honra (`set -eux` só no fim de OSH-18).
5. `pipefail` fora. `set -o pipefail` = invalid option (não no-op mentiroso).
6. Noclobber always-on: `$-` contém `C`; `set -C` no-op; `set +C` erro.
7. `$?` e `$-` entram em OSH-16 (canal de prova do próprio `set`). `$$`/`$!` fora.
8. PS4 fixo `+ `. Sem variável.
9. cmdsubst **não** dispara `-e` no pai (OSH-9 intacto).
10. `! pipeline` não se inventa para cumprir a isenção POSIX.
11. `g_shell_abort` novo; **não** reusar `g_returning`.
12. Sem módulo C novo. Flags em `expand.c`.
13. Dump de `set` sem args = environ, sem quoting de reinput.
14. Expansion error (`-u`) aborta script **mesmo sem** `-e` (POSIX 2.8.1).

---

## Anti-OE (checklist do implementer)

- Não implementar `trap` “já que estamos no dispatch”.
- Não implementar arrays, `pipefail`, `!`, `until`, `$$`, `$!`, PS4 variável.
- Não criar `set.c` / `shellopt.c`.
- Não reusar `g_returning` para errexit.
- Não desligar noclobber.
- Não “já deixar” o runner juntar linhas para `if` multi-linha.
- Não abrir YSH, Fish, backticks, coproc, `**`.
- Não editar manuais do vault. Não editar `TODO.md` neste plano.
- Red evidenciado **antes** do green, em toda fatia.
- Não cortar `set -e` nas isenções POSIX com o argumento “o smoke atual não usa” (L-21).
- Não copiar bash `x=$(false)` mata o pai: o smoke OSH-18 **exige** o contrário (dash-like).
