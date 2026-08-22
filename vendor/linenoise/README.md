# linenoise (embedded)

Versão embutida de https://github.com/antirez/linenoise

- Arquivos: linenoise.c + linenoise.h
- Licença original: BSD 2-clause (ver cabeçalhos dos arquivos)
- Razão: zero dependências de runtime + line editing + history + completion hook excelente para shells em C.

## Modificações locais (petrush)

1. **SEC-08 / CVE-2025-9810 residual:** `linenoiseHistorySave` usa `open(O_NOFOLLOW|O_CREAT|O_TRUNC|O_CLOEXEC)` + `fchmod(fd, 0600)` em vez de `fopen("w")` (não segue symlink).
2. **API de leitura do history:** `linenoiseHistoryLen()` / `linenoiseHistoryGet(index)` (autosuggest / `!!` / completion).
3. **UX-20 Ctrl-R:** `linenoiseHistorySearch(query, start_exclusive)` (substring, newest-first) + mini-modo reverse-i-search em `linenoiseEditFeed` (`CTRL_R=18`). Enter aceita; ESC aborta; sem Ctrl-S / regex / vi.
4. **UX-21 highlight:** `linenoiseSetHighlightCallback` — colorize do slice visível no refresh; largura/cursor no buf cru; skip em mask/`in_search`/fold; fast path de insert força refresh se callback ligado.

**Não editar diretamente** — se precisar de patch, documentar aqui + no commit.
