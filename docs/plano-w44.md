# Plano W44: OSH-19..21 (`trap` mínimo: tabela + EXIT + sinais)

**Status:** plano CTO (Caetano). Sem implementação neste documento. Sem push. Sem editar `TODO.md` (W43 GATE + GHA 14/14 no SHA `5848362`; fecho documental `adfc144`).
**Tipo:** plano de uma onda (anti-OE).
**Audience:** líder + implementer (`backend-engineer`) + orquestrador.
**Last-reviewed:** 2026-09-05.
**Canônico de produto:** [`plano-shell-avancado.md`](plano-shell-avancado.md). Predecessor: [`plano-w43.md`](plano-w43.md).
**Barra desta onda:** POSIX.1-2017 XCU `trap` (special builtin) + 2.8.1. OSH primeiro. Recorte dash-like (handler só marca pendência; ação corre em ponto seguro), não bash `ERR`/`DEBUG`/`RETURN`/`trap -p`/`trap -l`.

**Decisão autônoma, confirmar retroativamente:** `trap` **é a onda inteira** (três fatias). Não empilhar arrays nem JOB-1. W43 já tinha marcado OSH-19 como candidato natural de W44; `set -e` sozinho cruzou o dispatch, e `trap` cruza sinais + `exit` + runner de script. JOB-1 (`fg`/`bg`/Ctrl-Z/PTY) e arrays indexados são subsistemas distintos, cada um onda própria.

---

## O que já está no chão (não reabrir)

| ID | Onda | Superfície |
|----|------|------------|
| OSH-0..18 | W20..W43 | shebang, posicionais, `shift`, `if`/`while`/`for`/`case`, funções, `return`/`local`, `$(cmd)`, `$(( ))`, `[[ ]]`, here-doc, `set -x/-u/-e`, `$?`, `$-` |
| FEAT-NOCLOBBER / SEC-09 | W11 / W3 | noclobber **sempre ligado** (`O_EXCL`); sem `set +C` |
| UX-23 | W9 | `&` + tabela `job.c`; **sem** `fg`/`bg`/Ctrl-Z/`%n` |
| FEAT-TRUE | W10 | builtins `true` / `false` / `:` |

Fatos no tree (não inferência):

- Builtins em `src/mid/dispatcher.c` L45-80: há `set`; **não há** `trap`.
- `builtin_info` L2276: `Anti-OE: noclobber always-on; sem trap/arrays/pipefail/fg`.
- `builtin_exit` L1410-1425: `exit(code)` direto; **não** há gancho de EXIT.
- `petrush_run_script` (`source.c` L171-214) devolve o status de `run_file_lines` e o `main` retorna; **não** dispara ação na saída.
- `run_file_lines` L68-122: honra `g_shell_abort` (W43); não há poll de sinal.
- `main.c` L116-122: modo script **não** instala handlers de SIGINT/TERM (SIG_DFL). L146-157: REPL instala SIGINT vazio (linenoise/EINTR), TERM/HUP = `cleanup_and_exit`, SIGTSTP = `SIG_IGN`.
- `job.h` L5: "Sem fg/bg/Ctrl-Z/%n/wait builtin". `job.c` reapeia só PIDs da tabela via `waitpid(pid, WNOHANG)`.
- `process.c` ignora SIGINT/SIGQUIT no pai enquanto espera o filho (L312-314, L518-519); filho externo volta a `SIG_DFL` (L348, L431). `waitpid` **não** retenta EINTR.
- `petrush_run_cmdsubst` e `dispatch_pipeline_background`: filho termina em `_exit` (não corre `atexit`).
- `dispatcher.c` ~2415 linhas. `g_shell_abort` / `g_no_errexit` já existem (OSH-16/18).
- HEAD `adfc144` = fecho documental W43. Código da onda = `5848362`. GHA 14/14 verde. W44 pode implementar.

---

## Meta da onda

Scripts OSH passam a ter o `trap` POSIX que o bash cotidiano escreve no topo (`trap 'cleanup' EXIT`; `trap '...' INT`):

1. builtin `trap` + tabela + dump + ignore/reset (ainda **sem** disparar)
2. `trap ACTION EXIT` (e `0`): corre na saída do processo shell
3. `trap ACTION INT|TERM|…`: handler adiado; ação no ponto seguro

DoD de onda = as três fatias em 🔍 (nunca ✅ direto) + smokes verdes no Docker **fedora:44 clang** + regressão OSH-0..18 + `ctest` dos alvos tocados + lint dos TUs tocados. Push só no fim da onda. GHA (matriz já existente) é o espelho remoto; não poll.

Pré-condição de **implementação:** W43 GATE + GHA do SHA `5848362` verde (já está). Este plano não autoriza código até o orquestrador despachar o `backend-engineer`.

---

## Fora de W44 (explícito)

| Fora | Por quê |
|------|---------|
| YSH, tabelas, pipes estruturados | Ordem travada: OSH primeiro |
| Fish UX (highlight/autosuggest no AST) | Depois da linguagem OSH |
| JOB-1 `fg`/`bg`/Ctrl-Z/`%n`/PTY | Subsystem distinto (`job.c` + SIGTSTP hoje IGN); onda inteira |
| arrays indexados / associativos | Bash extra; precisa word-eval; onda própria |
| `trap ERR` / `DEBUG` / `RETURN` | Bash, não POSIX; unknown condition = erro |
| `trap -p` / `trap -l` | Bash/ksh; POSIX dump é `trap` sem args |
| `set -o pipefail` / `! pipeline` / `until` | Já fora em W43 |
| `$$` `$!` `PS4` variável | `$$` ajudaria smoke de INT, mas o harness mata o PID de fora; `$!` é job |
| `${VAR:?}` | Residual W43 |
| INT/TERM trap **no REPL** (roubar o handler do linenoise) | DoD desta onda = **modo script**; tabela pode ser gravada no REPL, dump funciona, `exit` dispara EXIT |
| `trap CHLD` / `SIGCHLD` | Roubaria o `waitpid` de `job.c`; erro honesto (como `set +C`) |
| `trap KILL` / `STOP` | POSIX: não se apanha; erro |
| `atexit(3)` para EXIT | Filho de `fork` herdaria; cmdsubst/`&` usam `_exit` de propósito |
| instalar `/bin/sh`, bit 4755, testes fora de Docker | Vetos do líder |
| C++ no parser/eval | ADR-001 |

---

## Arquitetura (recorte fechado)

Fonte: POSIX.1-2017 XCU `trap` (special builtin) + 2.8.1. Abordagem usual (L-22): **dash-like deferred trap**.

O handler de sinal **só** escreve `volatile sig_atomic_t` (pendência). A ação (`parse_list` + `dispatch_list` da string guardada) corre num ponto seguro: depois do item em `dispatch_list`, depois do `waitpid` do foreground, antes de o processo shell sair. Nunca `malloc`/stdio/dispatch dentro do handler.

Referência de comportamento: dash (`/bin/sh` POSIX), não bash 4.4+ (`set -E`, ERR trap, `inherit_errexit`, `trap -p`).

### Estado (Mid, `dispatcher.c`, sem ficheiro novo)

Irmão de `g_shell_abort` / tabela de funções. Sem `trap.c` nesta onda (primeira ocorrência; regra de 3). Helpers `static` no mesmo TU; API mínima no header.

| API | Papel |
|-----|--------|
| `int builtin_trap(petrush_cmd_t *cmd)` | special builtin |
| `void petrush_trap_reset_for_tests(void)` | zera tabela + pendências + guarda de EXIT |
| `void petrush_traps_poll(void)` | corre ações pendentes (sinais), uma vez cada |
| `int petrush_run_exit_trap(int status)` | EXIT uma vez; devolve status (o trap pode chamar `exit`) |
| `int petrush_trap_is_command(int sig)` | 1 se a ação é comando (não default, não ignore); para `process.c` não IGN durante wait |

EXIT vive no índice 0 da tabela (não é sinal). Sinais catchable: tabela indexada pelo número (teto `NSIG` / 65). Ação = ponteiro owned (`strdup` do argv já expandido). Três estados por slot: default (ausente no dump), ignore (string vazia), comando (string).

`C` noclobber e `set -e/-u/-x` **não** mudam.

### Builtin `trap` (Mid, `dispatcher.c`)

Special builtin. Parse (após expansão normal dos argv):

- `trap` sem args: dump das entradas **não-default**, uma por linha, formato dash-like `trap -- 'action' NAME` (aspas simples; `'` interno vira `'\''`). Ordem estável: EXIT primeiro, depois nomes em ordem da tabela estática. Usável como input do shell (POSIX: formato unspecified excepto reinput).
- `trap ACTION cond [cond...]`: guarda a mesma ACTION em cada cond.
- `trap - cond...`: reset para default (e `sigaction` SIG_DFL em modo script, se o sinal era nosso).
- `trap '' cond...` / `trap "" cond...`: ignore (`SIG_IGN` imediato, para filhos herdarem ignore; clássico `nohup` light).
- Primeiro operando numérico antigo (`trap 2 INT`) **não** se implementa. Número só como **condition** (`trap '-' 2` = reset INT).
- Vários cond no mesmo comando: POSIX; honrar.

Conditions aceites (nomes sem prefixo `SIG`, e com `SIG` stripado; case conforme POSIX: maiúsculas usuais):

- `EXIT` e `0`
- Nomes POSIX/Linux catchable: `HUP INT QUIT ILL TRAP ABRT BUS FPE USR1 SEGV USR2 PIPE ALRM TERM STKFLT CONT TSTP TTIN TTOU URG XCPU XFSZ VTALRM PROF WINCH POLL PWR SYS`
- Números correspondentes (`2` = INT, `15` = TERM, …) via a **mesma** tabela estática (não `sys_siglist`)

Conditions recusadas (stderr + status ≠0; script aborta via `g_shell_abort`, 2.8.1):

- `KILL` / `STOP` (e números 9 / 19 nesta máquina: "cannot be trapped")
- `CHLD` / `CLD` / `SIGCHLD` (conflito com `job.c`; mensagem honesta `cannot trap CHLD`)
- `ERR` / `DEBUG` / `RETURN` e qualquer nome fora da tabela (`trap: …: invalid trap name`)

`trap` sem ACTION e com conditions: POSIX unspecified / ksh "reset"? **Decisão autônoma:** erro de uso (`trap: usage: trap [-] [action] condition...`), não reset silencioso.

Unknown option (`trap -p`, `trap -l`, `trap --help` como flag): `invalid option` + abort em script. `--` após `trap` é permitido para ACTION que comece com `-` (`trap -- - INT` não; ACTION `-` **é** reset. ACTION que pareça flag: `trap -- '-x' INT` guarda `-x`).

### Disparo EXIT (OSH-20)

POSIX: "The action specified for EXIT shall be executed when the shell exits."

Ganchos **explícitos** (nunca `atexit`):

1. `petrush_run_script`: depois de `run_file_lines` (incluindo abort `-e`/`-u`/special builtin), **antes** do `return` para o `main`.
2. `builtin_exit`: antes de `exit(code)`.
3. Não em `petrush_source_file` / `builtin_source` (source não é saída do shell). Smoke prova: `source` com `trap 'echo x' EXIT` **não** imprime `x` ao fim do source; imprime quando o script **pai** acaba.

Guarda de recursão (`g_in_exit`): se a ação de EXIT chama `exit`, a segunda entrada não dispara o trap de novo. Status final = o do `exit` interno se houver; senão o status que o shell ia usar.

`$?` **dentro** da ação EXIT = o status que o shell ia devolver (POSIX/dash). Gravar `petrush_last_status_set` com esse valor **antes** de `dispatch_list` da ação; restaurar depois se a ação não chamou `exit`.

`set -e` **não** aborta o shell no meio da ação do trap (`g_no_errexit++` à volta). Sem isso, `trap 'false' EXIT` com `-e` reentra no abort.

Filho de `fork` (cmdsubst, `&`, pipeline): **não** corre EXIT do pai. cmdsubst/`&` já `_exit`. Se o filho cair em `builtin_exit`, a guarda `g_cmdsubst_depth > 0` **ou** um flag `g_is_subshell` (setado no filho logo após `fork`) impede `petrush_run_exit_trap`. **Decisão autônoma:** após `fork` no cmdsubst e no background, o filho chama `petrush_trap_reset_for_child()`: ações comando voltam a default; **ignores permanecem** (POSIX: subshell herda signals ignored). Sem copiar a tabela de comandos.

REPL: `exit` builtin dispara EXIT (mesmo gancho). Encerrar por SIGTERM no REPL **não** é DoD desta onda (handler actual `_exit` em `cleanup_and_exit`).

### Disparo de sinais (OSH-21)

DoD = **modo script** (`petrush arquivo`).

- `trap 'cmd' SIG`: `sigaction` no processo shell, flags **sem** `SA_RESTART` (para `waitpid` devolver EINTR). Handler: `g_trap_pending[sig] = 1;` só.
- `trap '' SIG`: `SIG_IGN` imediato; limpa pendência.
- `trap - SIG`: `SIG_DFL`; limpa pendência.
- `petrush_traps_poll`: para cada sig pendente, limpa o flag, `parse_list` + `dispatch_list` da string; `$?` durante a ação = `128+sig` (POSIX: valor > 128); depois restaura o `$?` anterior, salvo se a ação chamou `exit`.
- Pontos de poll: fim de cada item em `dispatch_list`; depois de `waitpid` de foreground em `process.c`; antes de EXIT.
- `process.c`: se `petrush_trap_is_command(SIGINT)` (idem QUIT), **não** pôr `SIG_IGN` no pai durante o wait; retentar `waitpid` em EINTR; após o wait, `petrush_traps_poll()`. Sem trap, o IGN actual permanece (não regressar Ctrl-C de externos no REPL).
- Vários sinais pendentes: correr em ordem crescente de número, uma vez cada (dash-like). Sem reentrância: se a ação de INT dispara outro INT, o flag espera o próximo poll.
- Sinal **não** apanhado: comportamento actual (script morre com o default).

Smoke de INT/TERM: o harness bash arranca o petrush em background, espera o script entrar no loop, `kill -s INT $pid`, `wait`. **Não** depende de `$$`. Timeout no harness (`timeout 5` ou equivalente) para não pendurar o Docker.

### Camadas

Mid: `dispatcher.c` (tabela + builtin + poll + EXIT + reset de filho). `source.c` (gancho EXIT só em `petrush_run_script`). Foundation: `process.c` só o wait/EINTR/IGN condicional (OSH-21). Front: `main.c` **intocado** no REPL (linenoise SIGINT fica). Expand intocado. Sem módulo C novo. Sem ilha ASM nova.

---

## Fatias (INVEST, serial)

Mesma Onda W44 = **não** paralelizável: as três tocam `dispatcher.c`. Ordem = plumbing observável (dump) → EXIT (saída de processo) → sinais (handler + poll).

Pré-req comum: W43 ✅ no remoto, Fedora 44 no Docker, TDD red→green, C23, 4 camadas. Implementer = `backend-engineer`.

Frase-guarda L-21 (verbatim no briefing): uso/contagem de uso no repositório do consumidor NUNCA corta escopo, desenho ou qualidade de um produto feito para distribuição; dor do consumidor é evidência de lacuna, jamais prova de que o que ele não usa pode sair; "ninguém usa X" é afirmação sobre UM repositório, nunca sobre o mundo; regra de paridade: se o motor/biblioteca substituído aceita, o produto novo aceita também (aqui: POSIX `trap` ACTION/reset/ignore/dump/EXIT/sinais catchable), não o mínimo medido no único consumidor conhecido.

### OSH-19: builtin `trap` + tabela + dump + ignore/reset

**História:** Como autor de script POSIX, quero declarar `trap 'cleanup' EXIT`, ver o dump, resetar com `trap - EXIT` e ignorar com `trap '' INT`, sem ainda depender do disparo.

**Escopo:**

- Entrada `trap` na tabela de builtins + `builtin_trap`.
- Tabela de ações (EXIT + sinais da lista fechada).
- Dump `trap -- '…' NAME`.
- Set / reset / ignore. `sigaction` de ignore/default em modo script **já** nesta fatia para `''` e `-` (efeito observável: filho de `trap '' INT; sleep` herda ignore; se o teste for pesado, fica no smoke OSH-21; unitário cobre dump).
- Condições inválidas / KILL / STOP / CHLD / ERR → erro + `g_shell_abort` em script.
- `help`/`info`: mencionar `trap`; Anti-OE passa a `noclobber always-on; sem arrays/pipefail/fg` (tira `sem trap`).
- `petrush_trap_reset_for_tests`.

**Fora da fatia:** disparar EXIT; disparar INT/TERM; `process.c`; REPL SIGINT.

**Camadas:** Mid dispatcher. Header: declaração `builtin_trap` + reset. `source.c` intocado.

**TDD red primeiro (obrigatório):**

1. `test_info`: `builtin_table_has("trap")` (vermelho).
2. `trap 'echo x' EXIT` + `trap` dump contém `EXIT` e `echo x`.
3. `trap - EXIT` + dump **não** contém EXIT.
4. `trap '' INT` + dump contém `trap -- '' INT` (ou equivalente aspas).
5. `trap 'x' ERR` e `trap 'x' CHLD` e `trap 'x' KILL` → status ≠0.
6. Script `trap 'x' NOTASIGNAL` aborta o runner (2.8.1), não imprime o eco seguinte.

EXECUTAR TESTE [`ctest -R 'test_info'` (host depois Docker fedora:44 clang)] [NA FATIA OSH-19]
EXECUTAR TESTE [`bash tests/smoke/osh19-trap.sh $BUILD/petrush` + `osh16-set-x.sh` + `osh0-script.sh` + clang-tidy dispatcher] [NA FATIA OSH-19]

Smoke OSH-19 (espelho OSH-16): dump, reset, ignore, nomes `INT`/`SIGINT`/`2` equivalentes no dump (`INT`), inválidos, special-builtin abort, `help`/`info` sem "sem trap". **Não** exige que EXIT imprima na saída do processo.

### OSH-20: `trap ACTION EXIT`

**História:** Como autor de script, quero `trap 'rm -f $tmp' EXIT` e ver a acção correr no fim normal, no `exit n`, e no abort de `set -e`.

**Escopo:**

- `petrush_run_exit_trap` no fim de `petrush_run_script`.
- `builtin_exit` dispara EXIT (guarda de recursão).
- `$?` dentro do trap = status de saída previsto.
- `-e` isento na ação.
- `source` **não** dispara.
- Filho cmdsubst/`&`: reset de child; `exit` no filho não corre EXIT do pai.
- Abort W43 (`false` com `-e`, `-u`, `set -z`) **ainda** dispara EXIT (o processo vai sair).

**Fora da fatia:** INT/TERM; `process.c`; `atexit`.

**Camadas:** Mid dispatcher + `source.c` (só `petrush_run_script`). `builtin_exit` no mesmo TU.

**TDD red primeiro (obrigatório):**

1. Script `trap 'echo bye' EXIT; echo hi` → stdout `hi` depois `bye`, rc 0.
2. `trap 'echo $?' EXIT; false` (sem `-e`) → trap imprime `1`.
3. `set -e; trap 'echo e' EXIT; false; echo x` → `e` presente, `x` ausente, rc ≠0.
4. `trap 'echo s' EXIT; source other` (other também poderia setar trap) → source return **sem** imprimir `s`; `s` no fim do pai.
5. `trap 'exit 7' EXIT; exit 3` → rc 7, sem loop.
6. `echo $(trap 'echo inner' EXIT; echo n)` → `n` sem `inner` no pai.

EXECUTAR TESTE [`ctest -R 'test_info|test_source'` (host depois Docker fedora:44 clang)] [NA FATIA OSH-20]
EXECUTAR TESTE [`bash tests/smoke/osh20-trap-exit.sh $BUILD/petrush` + `osh19-trap.sh` + `osh18-set-e.sh` + `osh7-return.sh`] [NA FATIA OSH-20]

### OSH-21: sinais adiados (`INT` / `TERM` / catchable genérico)

**História:** Como autor de script, quero `trap 'echo caught; exit 0' INT` e, ao receber SIGINT, correr a acção em vez de morrer no default.

**Escopo:**

- Handler `sig_atomic_t` + `petrush_traps_poll`.
- `sigaction` ao setar comando; `SIG_IGN` no ignore; `SIG_DFL` no reset.
- Máquina **genérica** (qualquer sinal da tabela, não um `if (SIGINT)`). DoD de smoke: INT e TERM (HUP no mesmo smoke se couber sem inflar).
- `process.c`: se o sinal está em modo comando, não IGN no wait; EINTR retenta; poll depois.
- Timeout no harness. Loop de builtin (`while true; do :; done`) como caso 1; `sleep` curto como caso 2 (prova o gancho do wait).
- REPL: **não** substituir o SIGINT do linenoise.

**Fora da fatia:** JOB-1, Ctrl-Z, `fg`, CHLD, `$$`, ERR.

**Camadas:** Mid dispatcher (poll + sigaction) + Foundation `process.c` (wait). `main.c` intocado.

**TDD red primeiro (obrigatório):**

1. Unitário: set INT comando → `petrush_trap_is_command(SIGINT)==1`; reset → 0.
2. Smoke harness: script `trap 'echo caught; exit 0' INT` + loop builtin; `kill -s INT`; stdout contém `caught`; rc 0.
3. Idem TERM (`kill -s TERM`; ação `echo term; exit 0`).
4. Sem trap: `kill -s INT` no mesmo loop → petrush morre ≠0, sem `caught`.
5. `trap '' INT` + kill INT → processo **não** morre (ignore); harness mata com TERM no cleanup.
6. Regressão: `osh18-set-e.sh` e `osh19-trap.sh` e `osh20-trap-exit.sh` verdes.

EXECUTAR TESTE [`ctest -R 'test_info|test_process'` (host depois Docker fedora:44 clang)] [NA FATIA OSH-21]
EXECUTAR TESTE [`bash tests/smoke/osh21-trap-sig.sh $BUILD/petrush` + `osh19-trap.sh` + `osh20-trap-exit.sh` + `osh18-set-e.sh`] [NA FATIA OSH-21]

---

## GATE-ONDA-W44

NA ONDA ACIMA, todos os testes foram feitos? funcionalidade de cada fatia e da onda garantida? todos os status check verde? version/tag/push autorizados apenas dessa forma; do contrário retorne a quem não tem status concluído e veja o que falta.

EXECUTAR TESTE [`cmake --build $BUILD --target smoke` + `ctest -R 'test_parser|test_expand|test_process|test_info|test_source'` + clang-tidy dispatcher/source/process; push só com verdes; GHA matriz; sem 4755] [FIM W44]

CMake: acrescentar `osh19-trap.sh` / `osh20-trap-exit.sh` / `osh21-trap-sig.sh` ao target `smoke` (ALL) **e** targets isolados `osh19_trap` / `osh20_trap_exit` / `osh21_trap_sig`, no mesmo padrão de `osh16_set_x`. Comentário do `smoke` ALL: `pudo + OSH-0..21 + XDG-1`.

---

## Ordem da onda

```
OSH-19 trap+dump+ignore  →  OSH-20 EXIT  →  OSH-21 INT/TERM  →  GATE-ONDA-W44  →  push
```

Serial. Um trabalho pesado (Docker fedora:44 clang / suíte) por vez. `TMPDIR=/var/tmp`. Sem 4755. Sem install nesta máquina.

Cada fatia: red evidenciado → green mínimo → refactor com suíte verde → commit local citando o ID (`feat(shell): … (OSH-19)` etc.) + Status `TODO.md` ⬜→🔍 **no mesmo commit**, quando o orquestrador autorizar a edição da tabela. Este plano **não** edita `TODO.md`.

---

## Pré-requisitos de máquina / CI

- Imagem `fedora:44`, clang, CMake do projeto, acutest já no tree.
- Não instalar pacotes de sistema sem ordem do líder.
- Não relaxar `-Werror` / `WarningsAsErrors`.
- Não tocar `pudod` / setuid / plugins / `configsh` / ASM.
- Dívida clang-tidy antiga em `dispatcher.c` / `expand.c`: não "aproveitar" para limpar o ficheiro inteiro. Só o caminho `trap` / poll / EXIT.
- Smoke de sinal: **Docker**, não na sessão viva do líder (L-50). Sem `xdotool`. Sem display `:0`. `kill` por PID do petrush, não do grupo do compositor.

---

## Segurança (Narciso, by design)

- Ação de trap é comando do **próprio script** (já em execução). Não é superfície nova de injeção além do que `eval` seria; **não** implementar `eval` nesta onda.
- Dump de trap escreve a action em stdout: smokes **não** metem segredo na action (T8).
- `trap '' INT` no script é ignore consciente; não é default.
- Special builtin `trap` com condition inválida aborta script (2.8.1), não deixa tabela pela metade (ou: falha a condition má e **não** aplica as seguintes do mesmo argv. **Decisão autônoma:** aborta na primeira condition inválida; as anteriores **desse mesmo comando** desfazem-se / não se aplicam. Implementação simples: validar **todas** as conditions antes de mutar a tabela).
- Filho de cmdsubst não herda ações comando (não disparar cleanup do pai duas vezes, nem no pipe).
- Sem 4755.
- Handler async-signal-safe: só `sig_atomic_t`. Qualquer outra coisa no handler = veto de review.

---

## Candidatos W45+ (não desta onda)

1. **JOB-1** process group + `fg`/`bg`/Ctrl-Z/`%n` (PTY no container). SIGTSTP deixa de ser IGN.
2. Arrays indexados; `--posix` word-eval.
3. `$$` / `$!` / `PS4`; `${VAR:?}`.
4. `set -o pipefail`; reserved `!` de pipeline; `until`.
5. INT/TERM trap no REPL (coexistir com linenoise).
6. PS2 / here-doc no REPL; `n<<`; `<<E"OF"` any-part-quoted (residual W42).

---

## Decisões autónomas desta revisão (confirmar retroativamente)

1. W44 = **3** fatias, **todas** `trap`. Arrays/JOB-1 **fora** (ondas próprias).
2. Ordem dump → EXIT → sinais. Observável primeiro; handler por último.
3. Recorte **dash-like deferred**, não bash ERR/DEBUG nem ação no handler.
4. Sem `trap.c`. Estado no `dispatcher.c` (primeira ocorrência).
5. Sem `atexit`. Ganchos explícitos.
6. EXIT não dispara no `source`; dispara na saída do processo e no `exit` builtin.
7. DoD de sinais = modo script. REPL SIGINT intocado.
8. Máquina de sinais genérica; smoke prova INT e TERM.
9. `CHLD`/`KILL`/`STOP` = erro honesto.
10. `ERR`/`DEBUG`/`RETURN`/`-p`/`-l` = invalid.
11. Validar todas as conditions do argv **antes** de mutar a tabela.
12. `$?` no EXIT = status previsto; `$?` no trap de sinal = 128+sig durante a ação.
13. `set -e` isento na ação do trap.
14. Subshell/cmdsubst/`&`: reset de ações comando; ignores ficam.
15. `process.c` só muda o IGN/EINTR quando há trap comando; senão regressão zero.
16. Sem `$$` nesta onda; harness mata o PID.
17. `trap` sem args com conditions = erro de uso, não reset implícito.
18. Dump formato `trap -- 'action' NAME` (dash-like).
19. Nomes com ou sem prefixo `SIG`; dump sempre sem prefixo (`INT`, não `SIGINT`).
20. Não extrair Foundation `trap` nem ilha ASM.

---

## Anti-OE (checklist do implementer)

- Não implementar `fg`/`bg`/Ctrl-Z "já que estamos nos sinais".
- Não implementar arrays, `pipefail`, `!`, `until`, `$$`, `$!`, PS4 variável, `eval`.
- Não criar `trap.c` / `signal.c`.
- Não usar `atexit`.
- Não roubar SIGINT do linenoise.
- Não apanhar `CHLD`.
- Não "já deixar" `trap ERR` como alias de `set -e`.
- Não abrir YSH, Fish, backticks, coproc, `**`.
- Não editar manuais do vault. Não editar `TODO.md` neste plano.
- Red evidenciado **antes** do green, em toda fatia.
- Não cortar conditions POSIX catchable com o argumento "o smoke só usa INT" (L-21): a **tabela** aceita a lista fechada; o smoke prova INT/TERM.
- Não copiar bash `trap DEBUG` / `RETURN`.
- Não `kill` no process group do compositor; só o PID do petrush no Docker.

---

## Mapa de despacho (orquestrador)

Fase 6 (desenvolvimento) + Fase 7 (QA da fatia). Porte do produto = o do `.bigtech-porte`; esta onda é **uma** construto OSH, serial.

| Passo | Quem | Modelo |
|-------|------|--------|
| Plano (este ficheiro) | Caetano / CTO | feito |
| Implementar OSH-19 → OSH-20 → OSH-21 (TDD, Docker) | `backend-engineer` | um abaixo |
| Review adversarial (executa os smokes, não só lê) | reviewer ≠ implementer | mais atual |
| Re-verificar host+Docker + GATE; commit por fatia; push no fim | orquestrador (main) | sessão |
| AppSec pontual (handler unsafe, atexit, 4755) | `security-engineer` só se o review achar handler/não-async | mais atual |

Nenhum C-level de GTM/CLO/CFO. Narciso entra só se o handler sair do `sig_atomic_t`.

WIP: um implementer desta onda por vez (os três TUs sobrepõem-se). Sem `isolation: worktree` (repo sob IDrive).
