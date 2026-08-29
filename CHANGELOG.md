# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
- **CI-XDG-TRUNC:** `tests/test_xdg_paths.c` destinos PATH_MAX (+margem sufixo) para silenciar gcc `-Werror=format-truncation` sem relaxar `-Werror`; host+Docker fedora:44 gcc PASS. Sem 4755.
- **OSH-9:** command subst `$(cmd)` (sem backticks); `petrush_cmdsubst_span` no lexer; expand via hook DIP com strip de newlines finais; runner `petrush_run_cmdsubst` (pipe+fork, depth<=2, max 1MiB); status do inner nao sobe ao pai; smoke `tests/smoke/osh9-cmdsubst.sh` + regressao osh8. Docker fedora:44 clang PASS. Sem 4755.
- **OSH-8:** builtin `local name[=value]` so dentro de funcao; valor visivel no body; restaura (ou unset) ao sair do frame inclusive apos `return`; `local name` sem `=` faz unset local; sem flags `local -*`; fora de funcao → status != 0; smoke `tests/smoke/osh8-local.sh` + regressao osh7. Docker fedora:44 clang PASS. Sem 4755.
- **OSH-7:** builtin `return` [n] so dentro de funcao (`g_fn_depth` + flag de unwind em `dispatch_list`/if/while/for); default n=0 (nao usa ultimo cmd; documentado); fora de funcao → status != 0 sem `exit`; return no meio do body nao executa o resto; smoke `tests/smoke/osh7-return.sh` + regressao osh6. Docker fedora:44 clang PASS. Sem 4755.
- **OSH-6:** `name() { list; }` e `function name [()] { list; }` (`PETRUSH_ITEM_FN`); chamada seta posicionais so durante o body e restaura depois; status = ultimo comando do body; `"}"` quoted nao fecha; sem `local`/`return` nesta onda; smoke `tests/smoke/osh6-fn.sh`. Docker fedora:44 clang PASS. Sem 4755.
- **OSH-3:** `if`/`then`/`elif`/`else`/`fi` (condicao = status do ultimo comando; builtins `true`/`false` e `/bin/true`/`/bin/false`); parser compound sobre `parse_list`; `"fi"` quoted nao e keyword; sem `[[`; smoke `tests/smoke/osh3-if.sh`. Docker fedora:44 clang PASS. Sem 4755.
- **OSH-2:** builtin `shift` [n] (default 1; `shift 0` no-op; n>$# → status != 0 e posicionais intactos; `$0` intacto); API `petrush_positional_shift`; smoke `tests/smoke/osh2-shift.sh`. Docker fedora:44 clang PASS. Sem 4755.
- **XDG-1:** rc `$XDG_CONFIG_HOME/petrush/rc` (fallback `~/.config/petrush/rc`); history `$XDG_STATE_HOME/petrush/history` (fallback `~/.local/state/petrush/history`); compat `~/.petrushrc` / `~/.petrush_history` se XDG ausente; `mkdir -p` 0700 ao gravar history; script mode sem rc (OSH-0). `xdg_paths.c` + `test_xdg_paths` + smoke `xdg-paths.sh`. Docker fedora:44 PASS. Sem 4755.
- **CI-OPENSSL:** GHA instala OpenSSL devel (`openssl-devel` / `libssl-dev` / `openssl`) em dnf/apt/pacman/CachyOS + ASan + fedora-next; CMake separa `PETRUSH_ASM_WAI_NET` (trampolines wai/netcom) dos atomos ASM core para TUs sem `wai.c`/`netcom.c` nao falharem no link. Smoke fedora:44 + CachyOS clang. Sem relaxar `-Werror`. Sem 4755.
- **ASM-NET:** builtin `netcom` (`-wifi`/`-eth`/`-bt` scan sysfs+netlink GET; `-up`/`-down` helpers; EPERM sem CAP_NET_ADMIN sem hang); `netcom_scan.S`; ctest `asm_netcom` + smoke netns; Docker fedora:44. Sem 4755.

### Changed
- **ARCH-03 / P2.1:** porta DIP `petrush/ui_port.h` (`clear` + history get/len); Mid (`dispatcher`, `hist_expand`) sem `linenoise.h`; adapter no Front (`petrush_setup_linenoise_ux`). Sem lib STATIC (P2.4 fora).
- **exit N:** `builtin_exit` honra argumento numérico (0..255); inválido → stderr + exit 2.

### Added
- **TST-CXX:** `add_test(NAME configsh)` (smoke `cxx-tui.sh`, labels `cxx`/`configsh`/`pty`); `ctest -R configsh` host + Docker fedora:44 PTY PASS. Relatório `docs/memory/tst-cxx.md`. Sem 4755.
- **PLG-LOAD:** `src/foundation/plugin_load.c` + `include/petrush/plugin_load.h` (XDG data + `PETRUSH_PLUGIN_PATH`, allow-list path canónico + SHA-256 OpenSSL EVP, recusa `S_IWOTH` em ficheiro/dirs, `dlopen` só após checks, ABI major antes de `init`); `pudod` sem `dlopen`/libcrypto; ctest `plugin_load` + `plugin_pudod_no_dl`; Docker fedora:44 clang PASS. Sem 4755. Consome `docs/security/plugins-threat.md`.
- **CXX-TUI:** `configsh` TUI raw ANSI (sem ncurses/Qt); `--section`/`--dump`/`--check`; XDG `$XDG_CONFIG_HOME/petrush/config.ini`; `src/cxx/{main,config,tui}.cpp`; smoke `tests/smoke/cxx-tui.sh` + target `cxx_tui`; PTY fedora:44 PASS. Sem 4755.
- **I18N-GETTEXT:** `po/` catalogs `en` / `pt_BR` / `es_419` (msgid=English); `include/petrush/i18n.h` + `src/foundation/i18n.c` (`bindtextdomain`/`textdomain`, `PETRUSH_LOCALEDIR` env/compile); CMake `FindGettext` + `msgfmt` → `build/locale/<lang>/LC_MESSAGES/petrush.mo` (`petrush_mo`); probe + smoke `tests/smoke/i18n-gettext.sh` + target `i18n_gettext`; ASM does not call gettext; Docker fedora:44 clang PASS. Sem 4755.
- **CXX-00:** alvo `configsh` C++23 (`src/cxx/main.cpp` stub help+exit 0; `-fno-exceptions -fno-rtti`); liga `tty_mode.S` + `utf8_width.S` quando `PETRUSH_ASM`; `petrush` sem `libstdc++` (prova `ldd`); smoke `tests/smoke/cxx00-ldd.sh` + target `cxx00`; Docker fedora:44 clang PASS. TUI raw = CXX-TUI.
- **ASM-TTY:** `src/asm/tty_mode.S` (`petrush_tty_mode`: RAW/COOKED via ioctl TCGETS/TCSETSF; 0 sucesso / `-errno`; nao-TTY → `-ENOTTY`; modo invalido → `-EINVAL`; sem tocar errno TLS); harness `tests/asm/test_tty_mode.c` (PTY via openpty) + ctest `asm_tty` 5/5; Docker fedora:44 clang PASS.
- **ASM-UTF8:** `src/asm/utf8_width.S` (`petrush_utf8_width`: UAX#11 subset; ASCII=1, combining Mn=0, CJK/wide=2, UTF-8 invalido=-1; decode estrito sem overlong/surrogate); harness `tests/asm/test_utf8.c` + ctest `asm_utf8` 17/17; Docker fedora:44 clang PASS.
- **PLG-ABI:** `plugins/abi.h` C11 (`PETRUSH_PLUGIN_ABI_MAJOR=1` / `MINOR=0`; entry points `query`/`init`/`cmd`/`fini` + vtable `petrush_plugin_abi_t`); smoke `tests/smoke/plg-abi-header.sh` + target `plugin_abi`; main sem dlopen (loader = PLG-LOAD). Docker fedora:44 clang PASS. Sem 4755. UX-25 intocado.
- **ASM-PGID:** `src/asm/job_setpgid.S` (`petrush_job_setpgid`: SYS_setpgid=109; 0 sucesso / `-errno` em falha; sem tocar errno TLS); `process.c`/`dispatcher.c` via wrapper (`PETRUSH_HAVE_ASM`); harness `tests/asm/test_job_setpgid.c` + ctest `asm_job_setpgid` 6/6 + `test_job`; Docker fedora:44 clang PASS.
- **ASM-HASH:** `src/asm/hash_path.S` (`petrush_hash_path`: FNV-1a 64, semente `0xcbf29ce484222325`, prime `0x100000001b3`; `len==0` devolve semente; nao exige NUL); harness `tests/asm/test_hash_path.c` + ctest `asm_hash_path` 9/9; Docker fedora:44 clang PASS.
- **ASM-I64:** `src/asm/parse_i64.S` (`petrush_parse_i64`: decimal signed 64 com `+`/`-` opcional; INT64_MIN/MAX; overflow/invalido → -1 sem escrever `*out`); harness `tests/asm/test_parse_i64.c` + ctest `asm_parse_i64` 16/16; Docker fedora:44 clang PASS.
- **ASM-CRC:** `src/asm/crc32.S` (`petrush_crc32`: IEEE poly 0xEDB88320, tabela 256, incremental; XOR final a cargo do caller); vetor `"123456789"` → digest `0xCBF43926`; harness `tests/asm/test_crc32.c` + ctest `asm_crc32` 8/8; Docker fedora:44 clang PASS.
- **ASM-GLOB:** `src/asm/glob_match.S` (`petrush_glob_match`: `*` `?` iterativo; `[` literal); `expand.c` `match_pat` chama o atomo quando `PETRUSH_ASM`; fallback C se OFF; `ctest -R test_glob` PASS; Docker fedora:44 clang PASS.
- **ASM-MEMEQ:** `src/asm/memeq_ct.S` (`petrush_memeq_ct`: 0 iguais / 1 diferem; n==0 igual; NULL so se n==0; XOR|OR acumula sem early-out); harness `tests/asm/test_memeq.c` + ctest `asm_memeq`; Docker fedora:44 clang PASS.
- **ASM-ABI:** `include/petrush/asm.h` (10 simbolos extern C: glob/utf8/parse_i64/crc32/memeq_ct/tty/hash_path/job_setpgid + stubs wai/netcom); `src/asm/abi.inc` macros System V AMD64 PIC; smoke `tests/smoke/asm-abi-header.sh` + target `asm_abi` (trava memeq_ct 0=iguais/1=diferem; rejeita -1=diferente no preambulo). Sem corpos nesta fatia.
- **ASM-00:** CMake `LANGUAGES C CXX ASM`, `PETRUSH_ASM` (default ON em x86_64; FATAL noutro arch), stub `src/asm/empty.S` com `.note.GNU-stack`, ASan/UBSan so em C/C++, gate `tests/smoke/asm00-toolchain.sh` + target `asm00`; CI instala binutils+gettext.
- **OSH-1:** posicionais `$0` `$1`.. `$#` `$@` `$*` (struct, nao environ); `"$@"` N palavras; `"$*"` uma (IFS[0], default espaco); shebang passa args; interativo `$0`=argv[0]; sem `shift` / `${1:-}` / arrays. Smoke: `tests/smoke/osh1-positional.sh`.
- **OSH-0:** `petrush arquivo` / shebang (`#!/usr/bin/env petrush`) em modo script (sem banner/linenoise/rc); runner de `source.c`; ausente → 127; recusa não-regular; sem SEC-10 `mode&0022` no argv; posicionais `$1` fora desta fatia. Smoke: `tests/smoke/osh0-script.sh`.

- **CI-DISTROS:** matrix GHA com `fedora:44` Debug+Release gcc/clang; `ubuntu:latest` (nao rolling) / `debian:latest` / `archlinux:latest` / `docker.io/cachyos/cachyos:latest` Release gcc+clang; `BUILD_TARGETS` inclui `test_xdg_paths` + `asm_*` + `configsh` (matrix e ASan); job ASan+UBSan (`Sanitize`, `halt_on_error`); lint duro so em Fedora 44 clang Debug; `check`+smoke obrigatorios; Release via `/usr/local` (SEC-02, sem setuid). Sem relaxar `-Werror`. Sem 4755.
- **CI:** matrix multi-distro no GHA (`fedora:44` Debug+Release gcc/clang; Ubuntu/Debian/Arch/CachyOS Release gcc+clang); job ASan+UBSan (`Sanitize`, `halt_on_error`); lint duro so em Fedora 44 clang Debug; `check`+smoke obrigatorios; Release via `/usr/local` (SEC-02, sem setuid); lista de targets alinhada ao CMake.
- **FEAT-TEST:** builtins `test` / `[` com primaries curtos (`-f -d -e -z -n = != -eq -ne -lt -gt`); `[` exige `]` final; status 0/1/2; sem `[[`, sem `-a`/`-o`/`!`; help + testes em `test_info`.
- **FEAT-NOCLOBBER:** política UX de noclobber sempre ligada (`>`/`2>` com `O_EXCL` já em SEC-09); `help`/`info` documentam; sem `set -C`/`set -o noclobber`.
- **FEAT-BANG:** word designators `!$` (último arg) e `!^` (primeiro arg) sobre o último evento; sem `!str`, sem modifiers `:h`/`:t`.
- **FEAT-TRUE:** builtins `true` / `false` / `:` (status 0/1/0, silent no-op; sem `printf`; tabela `dispatcher.c` + help).
- **FEAT-UMASK:** builtin `umask` (print/set máscara octal do processo do shell; sem `-S`; help + testes em `test_info`).
- **FEAT-READ:** builtin `read NAME` (1 linha de stdin → 1 variável; sem `-a`/`-d`/timeout/IFS split; help + testes em `test_info`).
- **UX-22:** `source` / `.` roda arquivo linha a linha no processo atual (teto depth 8, sem PATH search, `fopen`+`fstat`+`petrush_rc_stat_ok`, argc==2, sem `$1`/`return`; `load_rc_file` reusa o runner com `missing_ok=1`).
- **UX-21:** syntax highlight mínimo no REPL (aspas fechadas/não fechadas + token grosso CMD/OP; CSI 8 cores; `NO_COLOR` desliga; scanner Front sem parser; sem PATH/keywords/`$VAR`/`#`).
- **UX-20:** Ctrl-R reverse-i-search no linenoise (`linenoiseHistorySearch` substring newest-first; Enter aceita; ESC aborta; sem Ctrl-S/regex/vi).
- **UX-19:** builtins no pipe (cada estágio `n>=2` em fork; hook `find_builtin` antes do PATH; sem lastpipe/pipefail; `cd`/`export`/`exit` no pipe não alteram o pai; estágio único intacto).
- **UX-18:** glob simples `*` `?` em tokens unquoted (depois de `~`/`$VAR`; quoted fica literal; sem `[]`/`**`; sem glob em redir).
- **UX-17:** listas sequenciais `;` (`a; b` sempre roda `b`; reusa `PETRUSH_COND_ALWAYS`).
- **UX-16:** redirecionamento de stderr `2>` `2>>` `2>&1` `&>` (parser + apply em externos e builtins).

## [0.3.2.1] - 2026-08-14

### Fixed
- **SEC-01:** `pudo` não faz mais `unsetenv` no processo do shell. Env limpo só no `execve` do filho.

## [0.3.2.0] - 2026-08-14

### Added
- **PETRUSH_PS1 escapes** `\w` `\u` `\h` `\n` `\$` `\\` (UX-15). Default continua `petrush> `.
- Tilde `~` / `$VAR` (UX-12/13) e `cd -` (UX-14), já no `main` sem tag.

### Fixed
- Smoke `echo ~` no GitHub Actions (`HOME=/github/home`).

## [0.3.1] - 2026-08-03 - pushd/popd + history bangs

### Added
- **pushd / popd / dirs** — directory stack (NEW-25)
- **!!** and **!n** history expansion (NEW-26)
- Tests: `test_dirstack`, `test_hist_expand`; smoke 22 cases

## [0.3.0] - 2026-08-03 - UX wave (features festejadas em shells)

Pesquisa: Fish/Zsh/Bash — autosuggest, tab-complete, prompt, aliases, `&&`/`||`, which  
(ver `docs/research-shell-features.md`).

### Added
- **alias / unalias** + expansão da 1ª palavra (NEW-22)
- **which** (builtin vs PATH)
- **PETRUSH_PS1** prompt customizável
- **Tab completion** (builtins + PATH + arquivos) via linenoise
- **History autosuggest** (ghost text) via linenoise hints + API HistoryGet
- **Listas `&&` / `||`** com short-circuit (NEW-24)
- Testes: `test_alias`, `test_complete`, parser list; smoke 20 cases

### Notes
- Syntax highlighting adiado (anti-OE; alto custo em C sem lib extra).
- Host CI: Fedora 44.

## [0.2.0] - 2026-08-03 - Onda 3 (pipes + redir)

### Added
- Pipes `|` (estágios externos; multi-stage via `execute_pipeline`)
- Redirecionamento `>`, `>>`, `<` (externos e builtins)
- API `petrush_parse_pipeline` / `dispatch_pipeline` / `execute_pipeline`
- Smoke cobrindo pipe e redir; testes unitários de parser NEW-20
- Wiki GitHub publicada (Home + guia iniciante + arquitetura/security)

### Changed
- Versão `0.2.0`; REPL com line-buffer em stdout/stderr (evita vazar banner em redir sob pipe)
- Host GitHub-only; `.forgejo/` removido localmente

### Notes
- **Decisão autônoma (líder: modo autônomo 1+3+4):** ROI = pedido explícito do líder; escopo mínimo anti-OE.
- Fora de escopo v0.2: background `&`, `2>`/`&>`, globbing, builtins no meio de pipe, scripting de arquivo.
- Confirmar retroativamente com o líder se o escopo de redir/pipe basta.

## [0.1.0] - 2026-07-01 - S0 Gate

### Added
- Complete Onda 1 (Core MVP + Gate S0)
- `pudo` secure helper (`pudod`) with root-owned allow-list, environment sanitization, and minimal privileged code
- Automated gate via `cmake --build build --target verify` (build + lint + smoke + valgrind)
- Smoke tests for integration (≥13 commands including `pudo`)
- Unit tests expanded (env, info/diagnostics)
- `info` builtin (Onda 3 placeholder for diagnostics)
- Robust build targets: `clean-build`, `smoke`, `pudod-valgrind`
- Beginner documentation (`docs/beginner-guide.md`) explaining all jargon for newcomers
- CI workflow (`.github/workflows/ci.yml`) with matrix, lint, smoke, valgrind
- `docs/roadmap.md` and `docs/architecture.md` for layers and future
- `docs/security/` with audit and install guides for `pudo`

### Changed
- Pragmatic 4-layer architecture documented (no physical split for solo/anti-OE)
- README updated with current status, automated gate, examples
- TODO.md updated with all waves planning and statuses
- Anti-over-engineering reinforced: no pipes, redirection, scripting in v0.1
- Build system: isolated `pudod` from sanitize builds, FORTIFY, hardening

### Fixed
- Parser realloc OOB risk (NEW-01)
- Clang-tidy noise tuned with documented suppressions (NEW-02)
- Environment mutation avoided in `pudo` path (clean envp passed to pudod)
- Various small issues in tests, docs, CMake

### Notes
- Onda 1 gate achieved: Sanitize builds, tests, lint, smoke, valgrind all passing/automated
- Onda 2 (CI, docs, polish): Complete
- Onda 3 (future): Planning complete; implementation only on clear demand + ROI. No advanced features added (pipes etc. deferred)
- Version bumped to 0.1.0 for S0
- `pudo` setuid still requires manual review/approval (per security rules)
- License: GNU Affero General Public License v3.0 (AGPL-3.0)
- Host: GitHub (`petrinhu/petrush`); Codeberg/Forgejo deprecado

[0.3.2.1]: https://github.com/petrinhu/petrush/releases/tag/v0.3.2.1
[0.3.2.0]: https://github.com/petrinhu/petrush/releases/tag/v0.3.2.0
[0.3.1]: https://github.com/petrinhu/petrush/releases/tag/v0.3.1
[0.3.0]: https://github.com/petrinhu/petrush/releases/tag/v0.3.0
[0.2.0]: https://github.com/petrinhu/petrush/releases/tag/v0.2.0
[0.1.0]: https://github.com/petrinhu/petrush/releases/tag/v0.1.0
