# Inventário Feature × shells × petrush

| Campo | Valor |
|-------|-------|
| **ID** | RES-SH-01 |
| **Tipo Diátaxis** | Reference (lookup factual) |
| **Audience** | intermediário interno (CTO / product / implementers) |
| **Last-reviewed** | 2026-08-22 |
| **Owner** | technical-writer (handoff → Caetano/CTO para FEAT no TODO) |
| **Versão produto** | petrush ≤ v0.3.2.x + Unreleased (UX-16/17/18 no CHANGELOG) |
| **SHA baseline** | cruzado com `dispatcher.c`, `parser.h`, `expand.h`, `hist_expand.h`, `complete.c`, `main.c`, `CHANGELOG.md` |

---

## 1. Propósito

Inventário **Feature × 10 shells** (bash, zsh, fish, dash, ksh, mksh, tcsh, busybox-ash, yash, osh) **mais coluna petrush**, para alimentar WSJF e um teto de **8 FEAT** novos.

**petrush não é bash-compat.** É um **REPL interativo em C23** (Opção A, 2026-05-27): prompt persistente, externos do PATH + builtins próprios, linenoise, `~/.petrushrc`. Não é interpretador POSIX 100%, não é plugin host, não é clone de Fish/Zsh.

Legenda de célula:

| Símbolo | Significado |
|---------|-------------|
| **Y** | Presente de forma utilizável no uso típico |
| **P** | Parcial, subset, opcional, ou depende de build/flag |
| **N** | Ausente / fora do modelo do shell |
| **—** | Não aplicável (classe não faz sentido naquele modelo) |

Anti-OE: classes (~38), **não** lista de 200 builtins bash. Coproc / arrays / `[[ ]]` / brace / process-subst / OMZ entram na matriz **sem** ID de TODO.

---

## 2. Baseline petrush MEDIDO no código

Fontes: `src/mid/dispatcher.c` (tabela `builtins[]`), `include/petrush/parser.h`, `include/petrush/expand.h`, `include/petrush/hist_expand.h`, `src/front/complete.c`, `src/main.c`, `CHANGELOG.md` (Unreleased + 0.3.x).

### Já tem (Y ou P honesto)

| Capacidade | Evidência |
|------------|-----------|
| Builtins | `cd pwd echo exit help clear env export unset history pudo info alias unalias which pushd popd dirs` (`dispatcher.c:24-42`) |
| Line edit setas + history | linenoise + `linenoiseHistoryLoad/Save` (`main.c:180-183`) |
| Hints (ghost text) | `linenoiseSetHintsCallback` (`complete.c:156-158`) |
| Tab complete | builtins + PATH + arquivos (`complete.c` + `petrush_setup_linenoise_ux`) |
| `PETRUSH_PS1` escapes | `\w \u \h \n \$ \\` (`prompt.h` / UX-15; CHANGELOG 0.3.2.0) |
| `~` `$VAR` `${VAR}` | `expand.c` / `expand.h` (UX-12/13) |
| Glob `*` `?` unquoted | `glob_word` / `expand.h` (UX-18 Unreleased); sem `[]`/`**`; teto 256 |
| Listas `&&` `\|\|` `;` | `petrush_parse_list` (`parser.h:57-75`; UX-17 Unreleased) |
| Pipes `\|` | só estágios **externos**; builtin no pipe → erro (`dispatcher.c:233-247`) |
| Redir `> >> < 2> 2>> 2>&1 &>` | `parser.h` + UX-16 Unreleased |
| Bang `!!` `!n` | `hist_expand.h` (só linha inteira `!!`/`!N`; sem `!$`/`!str`) |
| `cd -` / OLDPWD | `builtin_cd` (`dispatcher.c:258+`) |
| `~/.petrushrc` no boot | `load_rc_file` → `petrush_source_file(..., missing_ok=1)` (UX-22) |
| SIGINT graceful | `sigint_handler` + EINTR (`main.c:132-167`) |
| SIGTSTP ignorado | job control ainda mínimo (`main.c:174-176`) |

### Fila TODO (não inventar FEAT por cima)

| ID | Feature | Status tabela |
|----|---------|---------------|
| UX-19 | Builtins no pipe (subshell-like) | ⏳ |
| UX-20 | Ctrl-R history search | ⏳ |
| UX-21 | Syntax highlight mínimo | 🔍 |
| UX-22 | `source` / `.` | 🔍 |
| UX-23 | Background `&` + jobs mínimos | ⏳ |

### Skip permanente (anti-OE)

| ID | Motivo |
|----|--------|
| UX-24 | Abbreviations (Fish): alias cobre o caso comum |
| UX-25 | Plugins / OMZ / modules |
| UX-26 | POSIX 100% / bash-compat |

---

## 3. Matriz Feature × shells × petrush

Ordem: **interativo** primeiro, depois **linguagem / execução**. Célula insegura → **P** + nota na §7.

Abreviações de coluna: **bb-ash** = busybox ash; **osh** = Oils OSH (compat path; YSH é outra língua).

### 3.1 Interativo / UX

| # | Classe | bash | zsh | fish | dash | ksh | mksh | tcsh | bb-ash | yash | osh | petrush | Nota petrush |
|--:|--------|:----:|:---:|:----:|:----:|:---:|:----:|:----:|:-----:|:----:|:---:|:-------:|--------------|
| 1 | Line editing (setas / emacs básico) | Y | Y | Y | P | Y | Y | Y | P | Y | Y | **Y** | linenoise |
| 2 | History persistente em arquivo | Y | Y | Y | N | Y | Y | Y | P | Y | Y | **Y** | `~/.petrush_history` |
| 3 | History search incremental (Ctrl-R) | Y | Y | Y | N | Y | Y | Y | N | Y | Y | **N** | UX-20 |
| 4 | Autosuggest / ghost text | P | P | Y | N | N | N | N | N | N | N | **Y** | hints linenoise (Fish-like leve) |
| 5 | Tab completion (PATH/files) | Y | Y | Y | N | Y | Y | Y | P | Y | Y | **Y** | builtins+PATH+files |
| 6 | Completion programável rica | Y | Y | Y | N | Y | P | Y | N | P | P | **N** | só complete estático |
| 7 | Syntax highlighting na linha | P | P | Y | N | N | N | N | N | N | N | **P** | UX-21 mínimo (aspas/CMD/OP); bash/zsh via plugin típico |
| 8 | Prompt customizável | Y | Y | Y | P | Y | Y | Y | P | Y | Y | **Y** | `PETRUSH_PS1` + escapes subset |
| 9 | Prompt com cmdsubst / subst avançada | Y | Y | Y | N | P | P | P | N | P | P | **N** | só `\w\u\h\n\$\\` |
| 10 | Aliases | Y | Y | Y | P | Y | Y | Y | P | Y | Y | **Y** | `alias`/`unalias` 1ª palavra |
| 11 | Abbreviations (expand-on-space) | N | N | Y | N | N | N | N | N | N | N | **N** | skip UX-24 |
| 12 | Plugins / module framework | P | Y | P | N | N | N | N | N | N | N | **N** | skip UX-25 (OMZ = teto OE) |
| 13 | Multiline editor / continuation | Y | Y | Y | P | Y | Y | Y | P | Y | Y | **N** | sem `\` newline; sem open-block |
| 14 | Vi editing mode | Y | Y | Y | N | Y | Y | Y | N | P | P | **N** | linenoise emacs-like |
| 15 | Directory stack (pushd/popd/dirs) | Y | Y | Y | N | Y | Y | P | N | Y | Y | **Y** | NEW-25 |
| 16 | `which` / `type` / localizar comando | Y | Y | Y | P | Y | Y | Y | P | Y | Y | **P** | `which` só; sem `type`/`command -v` |
| 17 | SIGINT no prompt sem matar shell | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | **Y** | PR-09 |
| 18 | Startup rc interativo | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | **Y** | `~/.petrushrc` no boot |

### 3.2 Linguagem / execução

| # | Classe | bash | zsh | fish | dash | ksh | mksh | tcsh | bb-ash | yash | osh | petrush | Nota petrush |
|--:|--------|:----:|:---:|:----:|:----:|:---:|:----:|:----:|:-----:|:----:|:---:|:-------:|--------------|
| 19 | Executar externos (PATH) | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | **Y** | `fork/exec` |
| 20 | Builtins de navegação/echo/env | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | **Y** | ver §2 |
| 21 | `export` / `unset` / env | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | **Y** | |
| 22 | Listas `&&` `\|\|` | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | **Y** | short-circuit |
| 23 | Separador `;` | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | **Y** | UX-17 |
| 24 | Pipes `\|` | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | **P** | externos OK; builtin no pipe = N até UX-19 |
| 25 | Redir stdout/stdin `> >> <` | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | **Y** | |
| 26 | Redir stderr / merge (`2>` `2>&1` `&>`) | Y | Y | Y | Y | Y | Y | P | Y | Y | Y | **Y** | UX-16 Unreleased |
| 27 | Background `&` | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | **N** | UX-23; parser declara NÃO |
| 28 | Job control (`fg`/`bg`/`jobs`) | Y | Y | Y | P | Y | Y | Y | P | Y | Y | **N** | UX-23; SIGTSTP ignorado |
| 29 | Tilde `~` / `~/` | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | **Y** | UX-12 |
| 30 | Param `$VAR` `${VAR}` | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | **P** | literal; sem `${VAR:-}` / `#` / `%` |
| 31 | Glob pathname `*` `?` | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | **Y** | unquoted; UX-18 |
| 32 | Glob avançado `[]` / `**` | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | **N** | `parser.h`/`expand.h` excluem |
| 33 | History bang `!!` / `!n` | Y | Y | N | N | Y | Y | Y | N | Y | Y | **P** | só `!!` e `!n` linha inteira |
| 34 | `cd -` / OLDPWD | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | **Y** | UX-14 |
| 35 | `source` / `.` | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | **Y** | UX-22 🔍; rc via mesmo runner |
| 36 | Command substitution `$()` / `` ` ` `` | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | **N** | |
| 37 | Arithmetic `$(( ))` | Y | Y | Y | Y | Y | Y | P | Y | Y | Y | **N** | |
| 38 | Funções shell | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | **N** | |
| 39 | Condicionais / loops (`if`/`for`/`while`/`case`) | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | **N** | |
| 40 | Arrays | Y | Y | Y | N | Y | Y | P | N | Y | Y | **N** | documentar; sem TODO |
| 41 | `[[ ... ]]` | Y | Y | N | N | Y | Y | N | N | P | Y | **N** | documentar; sem TODO |
| 42 | Brace expansion `{a,b}` | Y | Y | N | N | Y | Y | Y | N | Y | Y | **N** | documentar; sem TODO |
| 43 | Process substitution `<( )` | Y | Y | N | N | Y | P | N | N | P | Y | **N** | documentar; sem TODO |
| 44 | Coprocesses (`coproc`) | Y | P | N | N | Y | P | N | N | N | P | **N** | documentar; sem TODO |
| 45 | Here-doc / here-string | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | **N** | candidato FEAT residual só md |
| 46 | IFS word-split pós-expansão | Y | P | N | Y | Y | Y | N | Y | Y | P | **N** | expand substitui valor inteiro |
| 47 | Quoting `' '` `" "` básico | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | **Y** | `argv_quoted` anti re-glob |
| 48 | Privileged helper embutido (`pudo`) | N | N | N | N | N | N | N | N | N | N | **Y** | único; ver `docs/memory/sudo-pudo-riscos.md` |

**Contagem de classes na matriz:** **48** linhas (acima do piso ~32 do plano Caetano; ainda longe de “200 builtins”). Classes 40–44 = documentação anti-OE **sem** ID TODO.

---

## 4. Notas por família (1 parágrafo)

**dash / busybox-ash / yash (eixo POSIX magro).** Dash (Debian Almquist) e ash do BusyBox priorizam footprint e conformidade sh: pipes, redirs, `$VAR`, glob POSIX, `.`/`source`, jobs básicos quando o build liga; **quase zero** UX interativa rica (sem Ctrl-R/highlight/autosuggest). BusyBox ash varia com `CONFIG_ASH_*` (daí várias células **P**). Yash é POSIX com foco em previsibilidade e Unicode; cobre o núcleo Bourne + extras controlados, sem o “teto de features” do zsh. Para petrush: este eixo é o **piso de linguagem** que já cobrimos em parte (pipe/redir/list/expand), não o teto de UX.

**bash / osh (Bourne estendido + compat).** Bash define o dialeto dominante: builtins Bourne + job control + bang history + arrays/`[[`/brace/process-subst/coproc. OSH (Oils 0.37 docs) persegue compat Bash/POSIX com caminho de endurecimento; YSH é dialeto novo (fora desta coluna). petrush **não** mira bash-compat (UX-26 skip): a coluna petrush fica Y só onde o REPL C23 já implementou o subconjunto.

**ksh / mksh (Korn).** ksh93/OpenBSD ksh e mksh trazem arrays, `[[`, jobs, history e scripting denso com binário relativamente enxuto. mksh (MirBSD) é comum em Android/embarcado; man local é a fonte estável se o site oscilar. petrush herda poucas ideias Korn além do que já é UX diária (`cd -`, lists, redir).

**zsh = teto de over-engineering.** Expansões em cascata (history, process, parameter, command, arith, brace, filename) e ecossistema de módulos/OMZ. Excelente referência do que **não** copiar no petrush: completion framework infinito, `PROMPT_SUBST`, glob qualifiers, modifiers `:h/:t`. Usar zsh como **mapa de classes**, não como backlog.

**fish = outra língua.** Autosuggest, highlight, abbr e completions “just work” nativamente; sintaxe **não-POSIX** (sem bang, sem IFS split clássico, `set` em vez de `export` puro). petrush já tomou o **sabor** de hints/tab (v0.3.0) sem importar a linguagem Fish. Abbr e plugin-like function path ficam em skip.

**tcsh = linhagem csh.** Sintaxe csh (foreach, setenv, history bang agressivo), completions e editing maduros; **não** é Bourne. Comparar com petrush só nas classes interativas (history, edit, complete), nunca como alvo de sintaxe.

---

## 5. Gap analysis (A–E)

### (A) Já tem (não reabrir como FEAT)

Line edit, history file, hints, tab complete básico, PS1 escapes, alias, `&&`/`||`/`;`, pipes externos, redirs incl. stderr, `~`/`$VAR`, glob `*`/`?`, `!!`/`!n`, pushd/popd/dirs, `which`, `cd -`, builtins listados, rc boot, SIGINT, `pudo`.

### (B) Fila UX-19..23 (IDs já no TODO; não duplicar)

Builtins-no-pipe, Ctrl-R, highlight mínimo, `source`, `&`/jobs.

### (C) Skip permanente

UX-24 abbr · UX-25 plugins/OMZ · UX-26 POSIX/bash 100% · coproc/arrays/`[[`/brace/process-subst como meta de produto · funções/loops/scripting de arquivo completo.

### (D) Candidatos FEAT (teto **8**)

Ver ranking §6. Só estes podem virar linha nova no `TODO.md` (orquestrador/CTO), **depois** desta referência.

### (E) Só neste md (sem ID)

Here-doc completo, `eval`, `trap` rico, `hash`, nameref, `PROMPT_COMMAND`, zsh modifiers, Fish universal vars, brace/process-subst/coproc/arrays/`[[` como implementação, Vi mode, completion programável, CDPATH avançado, `**` globstar.

---

## 6. Ranking WSJF qualitativo das classes (D) → 8 FEAT

Fórmula informal (early):  
`WSJF ≈ (amor REPL × frequência × quebra-expectativa) / (esforço C23 + risco OE + risco bug)`.

Escala: **Alto / Médio / Baixo**. Cap **8**. Resto permanece só no md (§5E).

| Rank | ID proposto | Classe | Amor | Freq | Expectativa | Esforço | Risco OE | WSJF | Por quê |
|-----:|-------------|--------|------|------|-------------|---------|----------|------|---------|
| 1 | **FEAT-01** | Bang `!$` / `!^` (word designators mínimos) | Alto | Alta | Alta | Baixo | Baixo | **Alto** | Extensão natural de `!!`/`!n` já no código |
| 2 | **FEAT-02** | Builtin `printf` | Alto | Alta | Alta | Baixo | Baixo | **Alto** | Formatação sem depender de `/usr/bin/printf` em smoke |
| 3 | **FEAT-03** | Builtins `true` / `false` / `:` | Médio | Alta | Alta | Baixo | Baixo | **Alto** | Listas `&&`/`||` testáveis sem externo |
| 4 | **FEAT-04** | Builtin `test` / `[` (primaries curtos) | Alto | Média | Alta | Médio | Médio | **Alto** | Condições no REPL sem `if` completo |
| 5 | **FEAT-05** | Builtin `read` (1 linha → var) | Alto | Média | Alta | Médio | Baixo | **Alto** | Interatividade clássica; ROI de shell “vivo” |
| 6 | **FEAT-06** | `umask` builtin | Médio | Média | Média | Baixo | Baixo | **Médio+** | POSIX magro; simetria com env |
| 7 | **FEAT-07** | Glob `[]` (char class; ainda sem `**`) | Médio | Média | Média | Médio | Médio | **Médio+** | Fecha buraco pathname sem globstar OE |
| 8 | **FEAT-08** | `noclobber` / recusar overwrite em `>` | Médio | Baixa | Média | Baixo | Baixo | **Médio** | Segurança UX; irmão de shells-seguranca |

**Fora do cap 8 (só md):** here-string `<<<`, `type`/`command -v`, CDPATH, line-continuation `\`, Vi mode, `hash`, `eval`, `trap`, brace, process-subst.

**Ordem sugerida pós UX-19..23:** FEAT-01 → 03 → 02 → 06 → 05 → 04 → 07 → 08 (esforço crescente com WSJF ainda alto). Decisão final de encaixe no `TODO.md` = líder + CTO (este arquivo **não** edita a tabela).

---

## 7. Fontes (URL + data de consulta)

Crawl desta sessão ≤5 páginas; demais = conhecimento cruzado com docs irmãos / manuais já no repo. Células **P** por incerteza de build/flag estão marcadas na matriz.

| # | Fonte | URL | Consultado | Uso |
|---|-------|-----|------------|-----|
| 1 | Wikipedia: Comparison of command shells | https://en.wikipedia.org/wiki/Comparison_of_command_shells | 2026-08-22 | Características gerais; **aviso**: página com tags de original research / citations needed (tratar como mapa, não prova fina) |
| 2 | GNU Bash: Bourne Shell Builtins | https://www.gnu.org/software/bash/manual/html_node/Bourne-Shell-Builtins.html | 2026-08-22 | `.` `cd` `export` `pwd` `test` `unset` etc. |
| 3 | fish: Interactive use | https://fishshell.com/docs/current/interactive.html | 2026-08-22 | autosuggest, tab, highlight, abbr, Ctrl-R pager, pushd |
| 4 | Oils: All Docs (0.37.0) | https://oils.pub/release/latest/doc/index.html | 2026-08-22 | OSH compat vs YSH; completion † |
| 5 | zsh: Expansion | https://zsh.sourceforge.io/Doc/Release/Expansion.html | 2026-08-22 | teto de expansões (bang, process, brace, glob) = anti-OE |
| — | Repo local | `CHANGELOG.md`, `parser.h`, `expand.h`, `dispatcher.c`, `docs/plan-implementacao-roi.md` | 2026-08-22 | baseline petrush |
| — | Irmão | `docs/memory/shells-seguranca.md` | 2026-08-22 | superfície de risco correlata |

### Fontes pedidas que **não** foram crawled nesta fatia (limite 5)

| Fonte | Status | Mitigação |
|-------|--------|-----------|
| GNU Bash Job Control | não fetched | classe 27–28 = Y nos Bourne-likes por conhecimento padrão; petrush N medido |
| dash man (Debian) | não fetched | células dash alinhadas a POSIX magro; incerteza → P |
| OpenBSD ksh man | não fetched | ksh = Y no núcleo Korn típico; detalhes finos → P |
| BusyBox ash docs | não fetched | **P** deliberado (CONFIG_*) |
| yash manual | não fetched | POSIX Y; extras → P |
| mksh man (site pode cair) | não fetched | tratar como ksh-like; **P** onde divergência é comum |

Nenhuma fonte “falhou” por HTTP nesta rodada; o teto de crawl é que deixou manuais magros em **P**.

---

## 8. Checklist lifecycle

- [x] Tipo Diátaxis: Reference
- [x] Audience: intermediário interno
- [x] Last-reviewed: 2026-08-22
- [x] Owner: technical-writer
- [x] Coluna petrush cruzada com código (não README velho)
- [x] Cap 8 FEAT + skips UX-24/25/26 respeitados
- [x] Sem em-dash em prosa; `—` só como valor de célula
- [x] ID RES-SH-01 no cabeçalho
- [ ] Snippet executável: N/A (reference tabular)
- [ ] Encaixe FEAT no `TODO.md`: **fora de escopo deste agent**
)
