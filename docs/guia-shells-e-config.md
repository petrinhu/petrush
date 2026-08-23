# Guia: shells maduros × petrush (bugs, features e config)

| Campo | Valor |
|-------|-------|
| **Tipo Diátaxis** | Explanation (comparar e situar) + Reference curta de caminhos |
| **Audience** | intermediário interno (dev / ops / CISO) e leitor externo que já usa bash/zsh |
| **Last-reviewed** | 2026-08-23 |
| **Owner** | technical-writer |
| **Versão produto** | tree pós UX-19..23 e FEAT-BANG/TRUE/NOCLOBBER/UMASK/READ/PARAM/TEST |
| **Fontes cruzadas** | `docs/memory/shells-seguranca.md`, `docs/memory/shells-funcoes.md`, `docs/auditoria/aud-report.md`, `docs/beginner-guide.md`, `src/main.c`, `src/foundation/rc_trust.c`, `src/mid/source.c`, `src/mid/dispatcher.c` |

**Premissa:** petrush é um **REPL unpriv** em C23. Não é bash-compat (skip **UX-26**). O binário interativo **não** é setuid; elevação, se existir, fica no helper `pudod` (fora do escopo detalhado deste guia).

**Como ler o veredicto por shell**

- **✅ resolvido** = a classe de bug daquele shell **não se aplica** ao petrush, ou já foi **mitigada** no tree (cite SEC/UX/arquivo).
- **⛔ proibido vermelho** = a classe ainda existe de forma análoga, residual, ou de propósito fora de escopo (explique o porquê).

Não inventamos CVE. Só usamos âncoras já mapeadas em `shells-seguranca.md` e o estado medido no código.

---

## 1. Bugs encontrados

Subsessão por cada um dos 10 shells pesquisados. No fim de cada uma: check verde ou proibido vermelho.

### 1.1 bash (GNU Bash)

**Classes âncora (pesquisa):** Shellshock / function export (`CVE-2014-6271` e irmãos); privileged mode (`CVE-2019-18276`); `PROMPT_COMMAND` / cmdsubst em `PS1`; `BASH_ENV`/`ENV`; IFS word-split; overwrite clássico em `>`; history bang `!!`.

**No petrush (código):**

| Classe bash | Estado petrush | Evidência |
|-------------|----------------|-----------|
| Function export / Shellshock | ausente | sem funções exportáveis; `expand.c` só `$VAR` / `${…}` |
| bash `-p` / saved-UID | ausente | shell unpriv; `geteuid` só no escape `\$` do prompt |
| `PROMPT_COMMAND` / PS1 cmdsubst | ausente | `prompt_render` só `\w\u\h\n\$\\` |
| `BASH_ENV` | ausente | boot interativo só `~/.petrushrc` (`main.c` → `load_rc_file`) |
| IFS split pós-expansão | ausente | expansão substitui o valor inteiro |
| overwrite via `>` / `2>` | mitigado | **SEC-09**: `O_CREAT\|O_EXCL` em `process.c` / `dispatcher.c` (**FEAT-NOCLOBBER** always-on) |
| history file symlink / perms | mitigado | **SEC-08**: `O_NOFOLLOW` + `fchmod(0600)` no fd (CVE-2025-9810 no linenoise) |
| rc uid/mode | mitigado | **SEC-10**: `petrush_rc_stat_ok` (`rc_trust.c`) no rc e no `source` |
| bang `!!` / `!n` / `!$` / `!^` | presente (unpriv) | `hist_expand.c` (**FEAT-BANG**); risco de input colado, não priv-esc |

**✅ resolvido** nas classes Shellshock, privileged-mode, prompt-RCE, `BASH_ENV`, IFS e overwrite `>` (**SEC-08/09/10**, **FEAT-NOCLOBBER**). Residual unpriv de bang/alias/rc: ver §1 fim (R-I5 do `aud-report.md`).

### 1.2 zsh

**Classes âncora:** `PROMPT_SUBST` recursivo (`CVE-2021-45444`); drop de privilégio incompleto (`CVE-2019-20044`); shebang mishandle / trunc (`CVE-2018-0502`, `CVE-2018-13259`); `ZDOTDIR` / `.zshenv` em todo invoke; glob agressivo.

**No petrush:**

- Prompt RCE: **não** (sem `PROMPT_SUBST` / cmdsubst).
- Privileged drop: **não** aplicável (unpriv).
- Loader `#!` / shebang trunc: **não** há loader de script (ver §4); `source` lê linhas, não faz `execve` de shebang.
- Glob: só `*`/`?` unquoted, teto `PETRUSH_GLOB_MAX` fail-closed; sem `**` / qualifiers zsh.
- Bang: subconjunto presente (`!!`/`!n`/`!$`/`!^`).

**✅ resolvido** nas classes prompt-RCE, privileged drop e shebang-loader do zsh. Residual: bang unpriv (mesmo que bash).

### 1.3 fish

**Classes âncora:** prompt default que chama `git` (`CVE-2022-20001`); universal variables cross-session; markers Unicode internos (`CVE-2023-49284`); sem bang / sem IFS clássico.

**No petrush:**

- Prompt não executa externos (`prompt.c`).
- Sem store universal; env = `getenv`/`setenv` (`env.c`).
- Parser ASCII-centrado; sem markers fish.
- Alias + complete + history file existem (superfície interativa comum, unpriv).

**✅ resolvido** nas classes RCE-via-prompt-git e universal-vars. Residual unpriv: alias/rc/complete (aceitável no uid do user; R-I5).

### 1.4 dash (Debian Almquist / `/bin/sh`)

**Classes âncora:** poucas CVE “de marca”; risco dominante = **classe** POSIX: IFS + unquoted `$var` + glob; variável mágica `ENV` em boundary legado.

**No petrush:**

- IFS split: **não**.
- `ENV` mágica: **não**; rc fixo `~/.petrushrc`.
- Como alvo na allow-list do `pudod`: **SEC-12** recusa basename `sh`/`dash` (e bash/ash/busybox) após `realpath`.

**✅ resolvido** nas classes IFS/`ENV` do eixo dash-as-sh, e negação de shell genérico no load privilegiado (**SEC-12**). Residual REPL: redir/pipe/append unpriv.

### 1.5 ksh (ksh93 / ATT AST)

**Classes âncora:** eval de variáveis de ambiente no boot (`CVE-2019-14868`); nameref / aritmética rica; `ENV`/profiles.

**No petrush:**

- Env eval de código: **não**.
- nameref / `$(( ))` / `${!x}`: **não**.
- Expansão `$VAR` inocente: **sim**, limitada (`expand.c` + **FEAT-PARAM** `${VAR:-}` / `${VAR:+}` / `${#VAR}` sem nameref).

**✅ resolvido** na classe env-eval/nameref do ksh93. Residual: expansão de parâmetros no uid do user (esperado).

### 1.6 mksh (MirBSD Korn Shell)

**Classes âncora:** TTY flush histórico (`CVE-2008-1845`); demais = classes POSIX (IFS, ENV, glob, history, jobs).

**No petrush:**

- IFS/`ENV`/overwrite `>`: mesma mitigação da tabela-mãe (**não** / **SEC-09**).
- Job control: **mínimo** (**UX-23**): `&` + builtin `jobs`; **sem** `fg`/`bg`/Ctrl-Z ricos. Auditoria marca residual TTY/orphan (**R-I7** em `aud-report.md`).

**⛔ proibido vermelho** na fatia **jobs/TTY**: **UX-23** entregou tabela mínima de propósito; `fg`/`bg`/`wait`/`%n` **não** estão no escopo. Residual aceito unpriv + monitorar orphan/SIGTSTP (SIGINT no prompt já é tratado; SIGTSTP ignorado no shell).

Classes IFS/ENV/overwrite do mksh: **✅ resolvido** (ver §1.1 / tabela-mãe).

### 1.7 tcsh (C shell)

**Classes âncora:** history bang agressivo; here-doc symlink clássico; metachar em paths em CVEs antigos; sem IFS Bourne.

**No petrush:**

- Here-doc `<<`: **não** existe.
- Bang: **sim** (subconjunto).
- Prompt `\w`: imprime `getcwd` cru; não executa metachar (risco de display/confusão, não RCE de prompt).

**✅ resolvido** na classe here-doc/symlink-loader. Residual: bang unpriv (igual bash/zsh).

### 1.8 busybox ash

**Classes âncora:** bugs de parser ash (`CVE-2021-42375`, `CVE-2022-48174`); applet confusion (wget SSL, netstat VT); superfície multi-call.

**No petrush:**

- Parser próprio; **NEW-01** garante `argv[argc]=NULL` após realloc.
- Não é binário multi-call BusyBox → sem applet confusion no REPL.
- Se a allow-list do `pudod` apontar para applets BusyBox: risco de **composição** do helper, não do REPL; **SEC-12** já barra basename `busybox`/`ash` no load.

**✅ resolvido** na classe parser-OOB/applet do ash **no REPL**. Residual de política: operador não deve alargar `/etc/petrush/pudo.allow` além do example mínimo (**SEC-03**).

### 1.9 yash (Yet another shell)

**Classes âncora:** poucas CVE nomeadas no NVD; superfície = classes POSIX (IFS, ENV, glob, noclobber, rc, `source`).

**No petrush:** mesmo padrão dash: IFS **não**; noclobber **SEC-09**; `source` **UX-22** com `rc_stat_ok` e depth 8.

**✅ resolvido** nas classes POSIX que o petrush estruturalmente evita ou mitiga. Ausência de CVE famosa no yash **não** significa “zero risco de classe”.

### 1.10 osh (Oils / oils-for-unix)

**Classes âncora:** OSH persegue compat Bash/POSIX endurecido; YSH `simple_word_eval` remove split/glob/elisão implícitos. Sem Shellshock-level listado no escopo da pesquisa.

**No petrush:**

- Já se aproxima do espírito YSH em **um** ponto: **sem IFS split** após `$VAR`.
- Ainda faz glob dinâmico unquoted `*`/`?` (YSH enfatiza glob estático/explícito).
- Sem arith/cmdsubst/funções.

**✅ resolvido** relativamente a Shellshock/IFS clássico (fora por desenho). Residual: glob dinâmico limitado (teto 256 fail-closed) e bang unpriv.

### Síntese da §1 (REPL)

| Estado | O que cobre |
|--------|-------------|
| **✅ resolvido** | Shellshock/function-export; privileged-mode; prompt-RCE; `ENV`/`BASH_ENV`; IFS; overwrite `>`/`2>` (**SEC-09**); history symlink (**SEC-08**); rc uid/mode (**SEC-10**); argv NULL (**NEW-01**); shells genéricos no load pudo (**SEC-12**) |
| **⛔ residual / fora** | Bang history (unpriv, aceito); alias/rc/`source` (confiança no dono do home + **SEC-10**); jobs sem `fg`/`bg` (**UX-23** mínimo, R-I7); append `>>`; glob `*`/`?`; **UX-24/25/26** skip permanente (abbr / plugins / POSIX 100%) |

---

## 2. Features implementadas

O que o petrush **já tem** que aquele shell também tem (ou equivalente honesto). Cite IDs. Não listamos 200 builtins.

Legenda rápida: **Y** = utilizável; **P** = parcial / subset.

### 2.1 bash

| Capacidade bash típica | petrush | ID / evidência |
|------------------------|---------|----------------|
| Line edit + history file | Y | linenoise; `~/.petrush_history` |
| Tab complete PATH/files | Y | `complete.c` |
| Autosuggest / ghost text | Y (leve) | hints linenoise |
| Ctrl-R history search | Y | **UX-20** |
| Syntax highlight mínimo | P | **UX-21** (aspas/CMD/OP; `NO_COLOR`) |
| Prompt custom | Y | `PETRUSH_PS1` (**UX-15**); escapes `\w\u\h\n\$\\` |
| Aliases | Y | `alias` / `unalias` |
| `&&` `\|\|` `;` | Y | **UX-17** |
| Pipes `\|` | P→Y útil | externos + builtins no pipe em filho (**UX-19**); sem lastpipe/pipefail |
| Redir `> >> < 2> 2>> 2>&1 &>` | Y | **UX-16**; `>` com noclobber |
| Background `&` + `jobs` | P | **UX-23** (sem `fg`/`bg`) |
| `source` / `.` | P | **UX-22** (path explícito; sem PATH/`$1`) |
| `~` `$VAR` `${VAR}` | Y / P | **UX-12/13** + **FEAT-PARAM** (`:-` `:+` `#`) |
| Glob `*` `?` | Y | **UX-18**; sem `[]`/`**` |
| Bang `!!` `!n` `!$` `!^` | P | **FEAT-BANG**; sem `!str` / modifiers |
| `true`/`false`/`:` | Y | **FEAT-TRUE** |
| `umask` / `read` / `test` `[` | Y | **FEAT-UMASK** / **READ** / **TEST** |
| Directory stack | Y | `pushd`/`popd`/`dirs` |
| `cd -` | Y | **UX-14** |
| Funções / `[[` / arrays / cmdsubst / `$(( ))` | N | skip **UX-26** / anti-OE |

### 2.2 zsh

Sobreposição útil com petrush: line edit, history, complete básico, aliases, lists/pipes/redir, bang subset, `source` parcial, prompt **sem** `PROMPT_SUBST`, jobs mínimos.

**Não** copiamos (e não vamos): completion framework infinito, módulos/OMZ (**UX-25** skip), glob qualifiers, modifiers `:h`/`:t`.

### 2.3 fish

Sobreposição de **sabor** interativo: hints (autosuggest leve), tab complete, highlight mínimo (**UX-21**), aliases, pushd, history file, Ctrl-R (**UX-20**).

**Não** temos: abbreviations (**UX-24** skip), universal vars, sintaxe fish, prompt que roda `git`.

### 2.4 dash

Sobreposição no **piso POSIX magro**: externos PATH, `export`/`unset`, lists, pipes, redirs, `~`/`$VAR`, glob `*`/`?`, `.`/`source` (parcial), `true`/`false`/`:`, `umask`, `read`, `test`/`[`, `cd -`.

UX rica (Ctrl-R, highlight, hints): petrush **tem** mais que o dash típico interativo.

Jobs: petrush **P** (**UX-23**); dash costuma ter job control mais completo quando ligado.

### 2.5 ksh

Sobreposição: núcleo Bourne interativo (edit/history/alias/lists/redir/jobs mínimos) + `test`/`[` + `umask`/`read` + param expand parcial.

**Não** temos: nameref, discipline functions, arrays ksh, `[[` completo como meta.

### 2.6 mksh

Mesmo eixo Korn/POSIX magro que §2.5. Jobs petrush = **P** (**UX-23**). Bang = **P** (**FEAT-BANG**).

### 2.7 tcsh

Comparar só no eixo **interativo**: history, edit, complete, aliases, bang subset. Sintaxe csh **não** é alvo.

### 2.8 busybox ash

Piso ash: PATH, builtins básicos, lists/pipe/redir, expand/glob simples, `source` parcial. UX interativa rica do petrush (hints/Ctrl-R/highlight) **não** é o perfil BusyBox.

### 2.9 yash

POSIX previsível: mesma sobreposição da §2.4 (dash), mais a honestidade de que petrush **não** mira conformidade yash/POSIX 100% (**UX-26**).

### 2.10 osh (Oils)

OSH compat: overlap no Bourne estendido parcial (lists, redir, expand, bang subset). Espírito YSH (anti-IFS): petrush já **não** faz word-split pós-`$VAR`. Glob dinâmico ainda existe (limitado).

**Único “extra” petrush vs todos os 10:** builtin **`pudo`** (helper privilegiado separado). Ver `docs/memory/sudo-pudo-riscos.md` e `docs/security/`.

---

## 3. Pasta de configs

### 3.1 O que JÁ É (medido no código)

| Artefato | Caminho hoje | Quem lê | Notas |
|----------|--------------|---------|-------|
| RC do shell | `$HOME/.petrushrc` | `get_rc_file()` + `load_rc_file()` em `src/main.c` | Se `HOME` vazio: `./.petrushrc` no cwd. Boot via `petrush_source_file(..., missing_ok=1)` (**UX-22**). |
| History | `$HOME/.petrush_history` | `get_history_file()` + linenoise load/save | Fallback `./.petrush_history`. Save com **SEC-08** (`O_NOFOLLOW` + mode 0600 no fd). |
| Prompt | env `PETRUSH_PS1` | `main` → `prompt_render` | Definir no rc: `export PETRUSH_PS1='\u@\h:\w\$ '`. Escapes: `\w \u \h \n \$ \\`. Default `petrush> `. |
| Aliases | tabela em memória | builtins `alias` / `unalias` | Persistência = colocar `alias …` no `~/.petrushrc` (ou `source` de outro arquivo). |
| PATH / env | processo atual | `export` / `unset` / `env` | petrush **não** inventa PATH próprio; herda e altera via builtins. |
| Trust do rc/`source` | N/A (checagem) | `petrush_rc_stat_ok` (`src/foundation/rc_trust.c`) | Arquivo regular, `st_uid == getuid()`, sem write group/other (`mode & 0022 == 0`). Fail closed. |
| Config **client** do pudo | `$HOME/.config/petrush/pudo.conf` | `src/mid/pudo.c` | **Só** filtro client-side do builtin `pudo`. **Não** é o rc do shell. Autoridade privilegiada = allow-list do `pudod` em `/etc` (quando aplicável). |

**Não há** hoje, no código do REPL, um diretório XDG tipo `~/.config/petrush/` para o **shell** (rc, history, prompt). O único path sob `.config/petrush/` no tree é o **`pudo.conf`** client.

### 3.2 Exemplo mínimo de `~/.petrushrc`

```bash
# ~/.petrushrc - executado no boot interativo (mesmo motor do source)
export PETRUSH_PS1='\u@\h:\w\$ '
alias ll='ls -la'
```

Requisitos de permissão (senão o boot/`source` recusa): dono = você; sem write para group/other (ex.: `chmod 600 ~/.petrushrc`).

### 3.3 PROPOSTA (não implementado, não produção)

**PROPOSTA - não está no código:** migrar rc/history para layout XDG, por exemplo:

- `~$XDG_CONFIG_HOME/petrush/petrushrc` (fallback `~/.config/petrush/petrushrc`)
- `~$XDG_STATE_HOME/petrush/history` (ou `XDG_DATA_HOME`)

Isso seria feature nova (parser de path + migração + testes). **Não** instalar, **não** criar diretórios de sistema, **não** alterar `src/` neste documento. Até lá, o canônico continua **`~/.petrushrc`** e **`~/.petrush_history`**.

---

## 4. Shebang / declarar petrush num arquivo `.sh`

### Fato do código

Em `src/main.c`:

```c
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    /* … entra direto no REPL interativo … */
}
```

Consequências:

1. **`petrush script.sh` não funciona** como interpretador de arquivo: `argv[1]` é ignorado.
2. **`#!/bin/petrush`**, **`#!/usr/bin/env petrush`**, e variantes com espaço (`#! /bin/petrush`) **não funcionam ainda**. O kernel passaria o path do script em `argv[1]` (e o resto da linha shebang conforme a regra do SO); o petrush descarta `argc`/`argv` e abre o REPL.
3. Não afirmamos suporte a shebang. Não instalamos em `/bin` nem `/usr/bin` neste guia.

### Caminho honesto hoje

| Objetivo | Como fazer agora |
|----------|------------------|
| Rodar um arquivo de comandos | Dentro do REPL: `source ./arquivo` ou `. ./arquivo` (**UX-22**) |
| Config no boot | Linhas em `~/.petrushrc` |
| Comentários | Linhas `# …` no arquivo sourcado / rc (ignoradas pelo runner) |

Limites do `source` (honestidade): caminho **explícito** (relativo ou absoluto); **não** procura no `PATH`; um argumento só; sem `$1`…`$n`; nesting máximo **8**; arquivo precisa passar em **SEC-10**.

Quando (e se) script mode / shebang existir, será item de produto próprio com testes. Até lá: **só** `source` / rc.

---

## 5. Regra: nada em produção neste entregável

Este arquivo é **doc local** em `docs/`.

- Sem install prefix, sem `chmod 4755`, sem setuid, sem copiar binário para `/bin`.
- Sem wiki pública obrigatória nesta fatia (wiki/GitHub fica com **NEW-23** / orquestrador após tag, se pedido).
- Sem inventar IDs novos no `TODO.md`.
- Sem push (commit local a cargo de quem fecha a fatia).

Leitura complementar:

- Iniciante: [`docs/beginner-guide.md`](beginner-guide.md)
- Matriz de risco: [`docs/memory/shells-seguranca.md`](memory/shells-seguranca.md)
- Matriz de features: [`docs/memory/shells-funcoes.md`](memory/shells-funcoes.md)
- Relatório de auditoria: [`docs/auditoria/aud-report.md`](auditoria/aud-report.md)
