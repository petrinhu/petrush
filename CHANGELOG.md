# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
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
