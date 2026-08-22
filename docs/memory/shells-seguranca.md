# Segurança de shells vs petrush (REPL unpriv)

**Data:** 2026-08-22  
**SHA HEAD:** `1fdbfbfe8e9000ef490bf531f898340704fa4210`  
**Premissa:** petrush = **REPL unpriv** (C23, parser próprio). O binário interativo **não** é setuid. Mapping = “o petrush corre o mesmo risco de shell?”  
**Features pendentes (hoje ausentes):** UX-22 `source`/`.` e UX-23 background `&` → células **NÃO**; nota “vira PARCIAL se UX-22/23 ligar”.  
**Escopo:** defensivo. Sem PoC. Sem clone de git alheio.

Legenda petrush: **SIM** = superfície presente e análoga; **PARCIAL** = superfície menor/mitigada; **NÃO** = classe inexistente hoje.

---

## 0. Tabela-mãe (classes transversais × shells × petrush)

| Classe | CVE / referência (shells típicos) | bash | zsh | fish | dash | ksh | mksh | tcsh | busybox ash | yash | osh (Oils) | petrush | Evidência petrush |
|--------|-----------------------------------|------|-----|------|------|-----|------|------|-------------|------|------------|---------|-------------------|
| Function export (Shellshock) | CVE-2014-6271 (+7169,6277,6278,7186,7187) | SIM | NÃO (sem export de função via env) | NÃO | NÃO | PARCIAL (env eval CVE-2019-14868) | NÃO típico | NÃO | NÃO | NÃO | NÃO (sem export de função bash) | **NÃO** | Sem funções exportáveis; `expand.c` só `$VAR`/`${VAR}` literal |
| History file perms / symlink | CVE-2025-9810 (linenoise); histórico world-readable clássico | SIM | SIM | SIM (fish_history) | N/A (sem hist interativo rico) | SIM | SIM | SIM | limitado | SIM | SIM | **NÃO** (mitigado) | `linenoiseHistorySave`: `open(O_NOFOLLOW\|O_CREAT\|O_TRUNC\|O_CLOEXEC)` + `fchmod(fd,0600)` (SEC-08) |
| History expansion `!!` / `!n` | abuso de bang em input compartilhado | SIM | SIM | NÃO (sem bang) | NÃO | SIM (ksh) | SIM | SIM | NÃO típico | SIM | SIM (OSH) | **SIM** | `hist_expand.c:19-42` (`!!`, `!n`) |
| IFS word-split | clássico unquoted `$var`; dash/POSIX | SIM | SIM | NÃO (sem IFS split implícito) | SIM (POSIX) | SIM | SIM | NÃO (csh) | SIM | SIM | PARCIAL (OSH legado; YSH `simple_word_eval` remove) | **NÃO** | `expand_word` substitui valor inteiro; sem split pós-expansão |
| Glob / globstar | path injection, DoS de matches | SIM (`**` c/ globstar) | SIM | SIM (globs próprios) | SIM (`* ? []`) | SIM | SIM | SIM | SIM | SIM | SIM (estático; YSH sem glob dinâmico implícito) | **PARCIAL** | `expand.c` `*`/`?` unquoted; sem `[]`/`**`; teto `PETRUSH_GLOB_MAX` 256 fail-closed (`expand.h:10`) |
| noclobber / overwrite em `>` | overwrite acidental / race em `>` (mitigação noclobber ausente) | SIM (`set -C`) | SIM | SIM (`noclobber`) | SIM | SIM | SIM | SIM | SIM | SIM | SIM | **SIM** | Sem noclobber: `process.c:161` e `dispatcher.c:108` usam `O_CREAT\|O_TRUNC` sem `O_EXCL` |
| Privileged mode / drop priv | CVE-2019-18276 (bash `-p`); CVE-2019-20044 (zsh) | SIM | SIM | NÃO (sem setuid shell) | NÃO típico | PARCIAL | PARCIAL | PARCIAL | NÃO típico | NÃO típico | NÃO típico | **NÃO** | Shell unpriv; `geteuid` só no prompt `\$` (`prompt.c:59`); privilégio só em `pudod` separado |
| RCE via prompt | CVE-2021-45444 (zsh PROMPT_SUBST); `PROMPT_COMMAND`/PS1 | SIM (`PROMPT_COMMAND`, cmdsubst em PS1) | SIM | PARCIAL (prompt fish + git auto) | NÃO | PARCIAL | PARCIAL | PARCIAL | NÃO | PARCIAL | PARCIAL | **NÃO** | `prompt_render` só `\w\u\h\n\$\\` (`prompt.c:32-67`); sem cmdsubst |
| ENV / BASH_ENV / rc path | RCE em boundary via `ENV`/`BASH_ENV`/rc | SIM | SIM (ZDOTDIR etc.) | SIM (config + universal) | SIM (`ENV` POSIX) | SIM | SIM | SIM | SIM (`ENV`) | SIM | SIM | **PARCIAL** | Só `~/.petrushrc` no boot interativo (`main.c:59-106`); sem `BASH_ENV`/`ENV` non-interactive |
| Alias / complete | alias malicioso no rc; complete injeta path | SIM | SIM | SIM | alias limitado | SIM | SIM | SIM | limitado | SIM | SIM | **SIM** | `alias.c`; `complete.c` (PATH+fs) |
| Redir / pipe | overwrite, fd confusion | SIM | SIM | SIM | SIM | SIM | SIM | SIM | SIM | SIM | SIM | **SIM** | `parser.c` + `process.c:apply_redirs` |
| Arith / nameref | CVE-ish via `$(( ))` / `nameref` / indireção | SIM | SIM | NÃO (math próprio) | arith POSIX | SIM + nameref | SIM | limitado | arith limitado | SIM | SIM (estático em Oils) | **NÃO** | Sem `$(( ))`, sem nameref, sem `${!x}` |
| Fish universal vars | sync/persistência cross-session | NÃO | NÃO | SIM | NÃO | NÃO | NÃO | NÃO | NÃO | NÃO | NÃO | **NÃO** | Sem store universal; env = libc `getenv`/`setenv` (`env.c`) |
| BusyBox applet confusion | wget SSL, ash parser, netstat VT | NÃO | NÃO | NÃO | NÃO | NÃO | NÃO | NÃO | SIM | NÃO | NÃO | **NÃO** | Não é multi-call BusyBox |
| Parser OOB / realloc argv | Shellshock irmãos; NEW-01 | hist. SIM | hist. | hist. | hist. | hist. | hist. | hist. | CVE-2021-42375 / 2022-48174 | hist. | foco em parse seguro | **NÃO** (mitigado) | NEW-01 ✅ `finalize_argv` garante `argv[argc]=NULL` (`parser.c:227-237`) |
| linenoise history TOCTOU | CVE-2025-9810 | N/A (readline) | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | **NÃO** (mitigado) | Vendor: `O_NOFOLLOW` + `fchmod` no fd (SEC-08) |
| `source` / `.` | execução de arquivo no shell | SIM | SIM | SIM (`.`/`source`) | SIM | SIM | SIM | SIM (`source`) | SIM | SIM | SIM | **NÃO** | UX-22 ⏳; vira **PARCIAL** se UX-22 ligar |
| Background `&` / jobs | race TTY, orphan, signal | SIM | SIM | SIM | SIM | SIM | SIM | SIM | SIM | SIM | SIM | **NÃO** | UX-23 ⏳; `SIGTSTP` ignorado (`main.c:174-176`); vira **PARCIAL** se UX-23 ligar |

**Resposta curta ao mapping:** petrush **não** corre o mesmo risco de Shellshock, privileged-mode, prompt-RCE, IFS-split nem arith/nameref. Corre riscos **análogos menores** em history bang, alias/rc, redir/pipe, glob limitado e history-file TOCTOU. Features pendentes `source`/`&` reabririam superfície.

---

## 1. bash (GNU Bash)

### Perfil
Shell Bourne-compatible dominante. Expansões em cascata (brace → tilde → param → arith → cmdsubst → word-split IFS → pathname). Funções exportáveis via ambiente. Modo privilegiado (`-p`) quando euid≠ruid.

### CVEs âncora
| CVE | Resumo | CVSS (NVD) |
|-----|--------|------------|
| CVE-2014-6271 | Shellshock: trailing code após `function` em env | 9.8 |
| CVE-2014-7169 | Fix incompleto do parser de função em env | crítico (irmão) |
| CVE-2014-6277 / 6278 | Parser de função em env → RCE/DoS | críticos |
| CVE-2014-7186 / 7187 | OOB em redirecionamento / `read_token_word` | DoS/possível RCE |
| CVE-2019-18276 | `disable_priv_mode` não limpa saved-UID; `enable -f` + setuid | 7.8 |

### Superfícies clássicas
- `BASH_ENV` / `ENV` (não-interativo / sh-compat) e `~/.bashrc` / profile.
- `PROMPT_COMMAND` e command substitution em `PS1`.
- `export -f` / herança de função no ambiente (raiz do Shellshock).
- `set -C` noclobber (opt-in); default permite `>` truncar.
- History `~/.bash_history` (perms dependem de umask; symlink risk se mal configurado).
- `!!` / `!$` etc. (histexpand).

### petrush mapeia?
| Risco bash | petrush |
|------------|---------|
| Shellshock function-export | **NÃO** |
| bash `-p` / saved-UID | **NÃO** (unpriv) |
| PROMPT_COMMAND / PS1 cmdsubst | **NÃO** (`prompt.c` escapes fixos) |
| BASH_ENV | **NÃO**; rc só `~/.petrushrc` interativo |
| IFS split | **NÃO** |
| `!!` | **SIM** (`hist_expand.c`) |
| redir overwrite | **SIM** sem noclobber |

---

## 2. zsh

### Perfil
Shell poderoso com `PROMPT_SUBST`, módulos carregáveis, privileged mode, globbing avançado, completion framework.

### CVEs âncora
| CVE | Resumo |
|-----|--------|
| CVE-2018-0502 | Shebang mishandle → `execve` do nome na 2ª linha |
| CVE-2018-1100 | Stack overflow em `checkmailpath` |
| CVE-2018-13259 | Shebang >64 chars truncado → exec substring |
| CVE-2019-20044 | `--no-PRIVILEGED` não limpa saved uid; módulo restaura priv |
| CVE-2021-45444 | `PROMPT_SUBST` recursivo: output controlado no prompt → RCE |

### Superfícies
- Prompt com substituição recursiva (CVE-2021-45444).
- Drop de privilégio incompleto (irmão conceitual do bash CVE-2019-18276).
- `ZDOTDIR`, `ENV`, arquivos `.zshrc` / `.zshenv` (este último em todo invoke).
- Globbing e filename generation agressivos.

### petrush mapeia?
- Prompt RCE: **NÃO** (sem PROMPT_SUBST / cmdsubst).
- Privileged drop: **NÃO**.
- Shebang trunc: **NÃO** (petrush não interpreta scripts `#!` como loader de arquivo; UX-22 ausente).
- History bang: **SIM** (subconjunto `!!`/`!n`).

---

## 3. fish

### Perfil
Shell não-POSIX, sem IFS word-split clássico, sem bang history. Universal variables, prompt rico, autosuggest. Completions e fishd.

### CVEs âncora
| CVE | Resumo |
|-----|--------|
| CVE-2022-20001 | Prompt default roda `git` ao `cd`; repo git malicioso → RCE (3.1.0-3.3.1) |
| CVE-2023-49284 | Unicode non-characters (marcadores internos) vazam em command substitution → comportamento inesperado / risco menor |

### Superfícies
- Universal vars persistidas (cross-session, multi-host sync potencial).
- Prompt que executa comandos externos (git status).
- Sem Shellshock (não exporta funções bash).

### petrush mapeia?
- Universal vars: **NÃO**.
- Prompt que chama git: **NÃO** (`prompt_render` só escapes locais).
- Unicode marker confusion: **NÃO** (parser ASCII-centrado; sem markers fish).
- Alias + complete + history file: **SIM/PARCIAL** análogos.

---

## 4. dash (Debian Almquist Shell)

### Perfil
`/bin/sh` em Debian/Ubuntu. POSIX estrito, pequeno, sem bashisms. `ENV` para scripts interativos quando aplicável. Word-splitting IFS é o modelo mental de segurança #1 em scripts `sh`.

### CVEs
Poucos CVEs “de marca” recentes no NVD sob keyword dash-shell (superfície menor). O risco dominante é **classe**, não CVE famosa: unquoted expansions + IFS + glob.

### Superfícies
- `IFS` + unquoted `$var`.
- `ENV` apontando para arquivo controlado por atacante em boundary setuid/`su` legado.
- Redirecionamentos e pipes POSIX.

### petrush mapeia?
- IFS split: **NÃO** (vantagem estrutural vs dash-as-sh).
- `ENV`: **NÃO** como variável mágica; rc fixo `~/.petrushrc`.
- Como `/bin/sh` do sistema: se a allow-list do `pudod` incluir `/bin/sh` ou `/usr/bin/dash`, **reabre toda a superfície POSIX** no lado privilegiado (ver §12).

---

## 5. ksh (ksh93 / ATT AST)

### Perfil
KornShell com nameref, aritmética rica, discipline functions, avaliação de certas variáveis de ambiente no startup.

### CVEs âncora
| CVE | Resumo | CVSS |
|-----|--------|------|
| CVE-2019-14868 | ksh 20120801: eval de env vars permite bypass de restrições / execução | 7.8 |

### Superfícies
- Env-var evaluation no boot (classe Shellshock-adjacente, mecanismo distinto).
- `nameref` e indireção.
- `ENV` / profiles.

### petrush mapeia?
- Env eval de código: **NÃO**.
- nameref / arith: **NÃO**.
- Expansão `$VAR` inocente: **SIM** limitada (`expand.c`).

---

## 6. mksh (MirBSD Korn Shell)

### Perfil
ksh-compat usado em Android e alguns *BSD. Mais enxuto que ksh93; ainda POSIX+ksh.

### CVEs
| CVE | Resumo |
|-----|--------|
| CVE-2008-1845 | mksh pré-R33d: não flush tty I/O ao novo terminal → local priv-esc |

Poucos CVEs modernos de alto perfil no NVD; risco operacional = mesmas classes POSIX (IFS, ENV, glob, history).

### petrush mapeia?
- TTY flush/job control avançado: petrush tem job control **mínimo** e **sem** `&` (UX-23 ausente) → **NÃO** hoje; **PARCIAL** se UX-23 ligar.
- Demais: igual tabela-mãe (IFS **NÃO**, bang **SIM**, redir **SIM**).

---

## 7. tcsh (C shell)

### Perfil
Sintaxe csh: sem `$IFS` word-split estilo Bourne; backticks e metachar em paths históricos. History bang agressivo. `ls-F` e builtins com histórico de bugs de plataforma.

### CVEs / histórico
| CVE | Resumo |
|-----|--------|
| CVE-1999-1383 | Metachar em nomes de diretório (tcsh/bash antigos) |
| CVE-2000-1134 | Symlink em here-doc (`<<`) em vários shells incl. tcsh |
| CVE-2003-1024 | `ls-F` em Solaris 8 |

### petrush mapeia?
- Here-doc: **NÃO** (sem `<<`).
- Bang history: **SIM** (subconjunto).
- Metachar em cwd no prompt: petrush imprime `getcwd` cru em `\w` (`prompt.c:33-35`) sem executar; risco é display/confusão, não RCE de prompt.

---

## 8. busybox ash

### Perfil
Ash embutido no binário multi-call BusyBox (IoT, containers mínimos). Parser pequeno; applets vizinhos (wget, netstat) ampliam blast radius.

### CVEs âncora
| CVE | Resumo |
|-----|--------|
| CVE-2021-42375 | ash: handling de elemento especial → DoS |
| CVE-2022-48174 | ash.c stack overflow → RCE em contexto IoV |
| CVE-2018-1000500 | `busybox wget` sem validar cert SSL |
| CVE-2022-28391 | `netstat` imprime PTR em terminal VT → RCE via escape |

### petrush mapeia?
- Parser ash OOB: petrush tem parser próprio; NEW-01 mitigou NULL terminator (**NÃO** na classe ash).
- Applet confusion: **NÃO** (binário único shell, não BusyBox).
- Se pudod allow-list apontar para busybox applets setuid-adjacentes: risco de **composição**, não do REPL.

---

## 9. yash (Yet another shell)

### Perfil
Shell POSIX com foco em conformidade e previsibilidade. Pouca “marca” de CVE no NVD (keyword search 0 em 2026-08-22). Superfície = classes POSIX padrão (IFS, ENV, glob, noclobber, rc).

### petrush mapeia?
- Mesmo padrão dash/POSIX: IFS **NÃO** no petrush; redir/pipe **SIM**; `source` **NÃO** até UX-22.
- Ausência de CVE famosa ≠ ausência de risco de classe.

---

## 10. osh (Oils / oils-for-unix)

### Perfil
OSH: compat Bash/POSIX com caminho de endurecimento. YSH: `simple_word_eval` remove split/glob/elisão implícitos (doc oficial oils.pub 0.37.0). Objetivo declarado: corrigir o “silent eval” de word expansion que gera classes de bug há 30+ anos.

### CVEs
Sem CVE “Shellshock-level” listada no escopo desta pesquisa; o projeto posiciona-se como mitigação estrutural das classes bash.

### petrush mapeia?
- petrush já se aproxima do espírito YSH em um ponto: **sem IFS split** após `$VAR`.
- Diferenças: petrush ainda faz glob dinâmico em tokens unquoted com `*`/`?` (YSH enfatiza glob estático / explícito); petrush não tem arith/cmdsubst; Oils é runtime completo de scripting.

Referência: https://oils.pub/release/0.37.0/doc/simple-word-eval.html

---

## 11. Superfície petrush (estado no HEAD)

### O que o REPL é
- Loop linenoise + parse lista/pipeline + expand + dispatch + `fork/exec` (`main.c`, `dispatcher`, `process.c`).
- Unprivileged por design. Setuid, se existir, é só em **`pudod`** (binário separado).

### Presente (SIM / PARCIAL)
| Peça | Arquivo | Nota de risco |
|------|---------|---------------|
| Parser pipes/redir | `src/mid/parser.c` | NULL argv OK (`finalize_argv`); operadores `\| < > >> 2> 2>> 2>&1 &>` |
| Expand `$VAR` / `~` | `src/mid/expand.c` | Sem split; sem arith; sem cmdsubst |
| Glob `*` `?` | `expand.c` + `expand.h` | Sem `[]`/`**`; max 256 fail-closed |
| History bang | `src/mid/hist_expand.c` | Só `!!` e `!n` (não `!$`, `!foo`, `^`) |
| Prompt escapes | `src/mid/prompt.c` | Sem execução |
| Alias | `src/mid/alias.c` | Expande 1ª palavra; tabela 64×512 |
| Complete | `src/front/complete.c` | Varre PATH/fs (DoS local / info leak menor) |
| rc | `main.c:load_rc_file` | Executa linhas do `~/.petrushrc` (confiança no dono do home) |
| History file | `vendor/linenoise/linenoise.c:linenoiseHistorySave` | `umask` + `open(O_NOFOLLOW\|…)` + `fchmod(0600)` no fd (SEC-08) |
| Redir write (noclobber ausente) | `process.c:161` + `dispatcher.c:108` | **SIM** superfície overwrite; `O_CREAT\|O_TRUNC` sem `O_EXCL` nos dois sítios |
| Env | `src/foundation/env.c` | Pass-through libc; sem interpretador de `ENV` |

### Ausente (NÃO) com nota de futuro
| Peça | Status TODO | Se ligar |
|------|-------------|----------|
| `source` / `.` | UX-22 ⏳ | **PARCIAL**: rc-like on demand; risco de path controlado |
| `&` / jobs | UX-23 ⏳ | **PARCIAL**: TTY/SIGTSTP/orphan |
| Funções / export -f | fora | manter **NÃO** |
| `$(( ))` / nameref | fora | manter **NÃO** |
| `PROMPT_COMMAND` | fora | manter **NÃO** |
| setuid no shell | explicitamente fora | manter **NÃO** |

### Mitigações já creditadas
- **NEW-01** ✅: terminator NULL em argv após realloc.
- **CVE-2025-9810** / **SEC-08**: `open(O_NOFOLLOW|O_CREAT|O_TRUNC|O_CLOEXEC)` + `fchmod` no fd do history (não segue symlink; melhor que `chmod(path)`).
- Glob fail-closed no overflow.
- Prompt sem substituição de comando.

### Riscos residuais do REPL (prioridade)
1. **Sem noclobber** (baixo-médio, SEC-09 sugerido): `>` destrói arquivo existente em `process.c:161` e `dispatcher.c:108`.
2. **`~/.petrushrc` confiado** (médio se home compartilhado/NFS; SEC-10 sugerido): qualquer linha vira comando; sem checar uid/mode do arquivo.
3. **`!!` em contexto de input colado** (baixo): ecoa e executa última linha do histórico.
4. **Complete PATH walk** (baixo): custo/DoS local.
5. ~~History symlink TOCTOU~~ (SEC-08 fechado em impl: `O_NOFOLLOW` + teste `test_linenoise_history`).

---

## 12. Interação com pudo (curta)

O shell petrush permanece unpriv. Elevação é **só** via builtin `pudo` → helper `pudod` (setuid/capabilities), com allow-list em `/etc/petrush/pudo.allow`, `realpath`, path absoluto e sanitize de `IFS`/`ENV`/`BASH_ENV`/`LD_*` no lado root (`pudod.c:238-249`).

**Ponto crítico de composição:** se a allow-list incluir `/bin/sh`, `/usr/bin/bash`, `dash`, `busybox`, etc., o atacante que passa no `pudo` **reabre o modelo POSIX completo** (IFS, scripts, redirecionamentos, eventualmente `ENV`) **já como root no filho**. Isso não é bug do parser do petrush; é política da allow-list.

Recomendação defensiva (sem reescrever `sudo-pudo-riscos.md`):
- Nunca allow-listar um shell interativo genérico.
- Preferir applets/binários de propósito único (`/usr/bin/id`, etc.).
- Tratar `pudo.allow.example` amplo (apt/dnf/systemctl/passwd) como risco de documentação (SEC-03 na TODO).

Detalhe fino de pudo/sudo fica no artefato irmão de pesquisa; aqui só o acoplamento shell↔helper.

---

## 13. Fontes + crawl log

### Fontes primárias (código local, leitura)
- `src/mid/parser.c`, `expand.c`, `hist_expand.c`, `prompt.c`, `alias.c`, `dispatcher.c`
- `src/front/complete.c`
- `src/foundation/env.c`, `process.c`
- `src/main.c`
- `src/pudod/pudod.c`, `pudo.allow.example`
- `vendor/linenoise/linenoise.c` (`linenoiseHistorySave`)
- `docs/research-shell-features.md`, `CHANGELOG.md`, `TODO.md` (somente leitura)
- `include/petrush/expand.h` (`PETRUSH_GLOB_MAX`)

### Fontes web (crawl ≤5 hops a partir de NVD / sites oficiais)
| # | URL | Hop | Uso |
|---|-----|-----|-----|
| 1 | https://nvd.nist.gov/vuln/detail/CVE-2014-6271 | 0 | Shellshock |
| 2 | https://nvd.nist.gov/vuln/detail/CVE-2019-18276 | 0 | bash privileged |
| 3 | https://nvd.nist.gov/vuln/detail/CVE-2019-14868 | 0 | ksh93 env |
| 4 | https://nvd.nist.gov/vuln/detail/CVE-2021-45444 | 0 | zsh prompt |
| 5 | https://nvd.nist.gov/vuln/detail/CVE-2025-9810 | 0 | linenoise TOCTOU |
| 6 | https://services.nvd.nist.gov/rest/json/cves/2.0?cveId=… | 1 | irmãos Shellshock + zsh 2018/2019 |
| 7 | https://services.nvd.nist.gov/rest/json/cves/2.0?keywordSearch=… | 1 | fish, mksh, tcsh, busybox ash |
| 8 | https://oils.pub/ | 0 | Oils landing |
| 9 | https://oils.pub/release/0.37.0/doc/published.html | 1 | índice docs |
| 10 | https://oils.pub/release/0.37.0/doc/simple-word-eval.html | 2 | semântica anti-IFS |
| 11 | https://fishshell.com/docs/current/security.html | 0 | 404 (sem doc security dedicada neste path) |
| 12 | https://www.mirbsd.org/mksh.htm | 0 | falha de fetch (rede) |

### Queries usadas
`CVE-2014-6271`, `CVE-2019-18276`, `PROMPT_COMMAND`/`PS1`, zsh `CVE-2018-0502` `2018-1100` `2018-13259` `2019-20044` `2021-45444`, `fish shell`, `dash shell IFS`, `ksh93 CVE-2019-14868`, `mksh`, `tcsh`, `busybox ash`, `yash shell`, `oils.pub` / `simple_word_eval`, `CVE-2025-9810`.

### Limitações honestas
- NVD keyword search devolve ruído (ex.: “fish.c” do Midnight Commander); CVEs fish relevantes filtradas manualmente (2022-20001, 2023-49284).
- Rate-limit HTTP 429 ao puxar detalhes de alguns CVEs BusyBox; resumos obtidos parcialmente + conhecimento de classe.
- yash/dash: poucos CVE “nomeados”; análise por classe POSIX.
- mksh.org fetch falhou; CVE-2008-1845 via NVD keyword.
- Não clonado nenhum repositório externo (ordem do brief).

---

**Conclusão operacional:** para o mapping “REPL unpriv vs shells maduros”, petrush está **estruturalmente fora** das classes que historicamente geraram RCE em boundary (Shellshock, privileged-mode, prompt-subst, ENV/BASH_ENV, arith/nameref, IFS). Permanece **dentro** das classes de shell interativo cotidiano (rc, alias, bang, redir, glob limitado, history file). O maior erro de política possível não é no parser: é allow-listar `/bin/sh` no `pudod`.
