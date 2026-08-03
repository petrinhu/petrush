# Pesquisa: features celebradas em shells (2025–2026)

**Data:** 2026-08-03  
**Fontes:** JetBrains Developer Ecosystem 2025 (via tech-insider 2026), Stack Overflow 2025, discussões Fish/Zsh (HN, Reddit, blogs), features out-of-box do Fish 4.x.

## O que usuários mais festejam (interativo)

| # | Feature | Onde brilha | Adoção / sinal |
|---|---------|-------------|----------------|
| 1 | **Autosuggestions** (ghost text do histórico) | Fish nativo; zsh-autosuggestions | Feature #1 “wow” do Fish |
| 2 | **Tab completion** (PATH + arquivos) | Todos; Fish “just works” | Expectativa mínima |
| 3 | **Syntax highlighting** na linha | Fish; zsh-syntax-highlighting | Forte em UX; custo em C |
| 4 | **Prompt rico / custom** (Starship, PS1) | Zsh+OMZ, Fish | 62% customizam (Zsh) |
| 5 | **Aliases / abbreviations** | Universal | Workflow diário |
| 6 | **Listas `&&` / `||`** | Bash/Zsh POSIX | Scripting leve no REPL |
| 7 | **History search** (Ctrl-R / setas) | linenoise já cobre setas | Já parcial no petrush |
| 8 | **which / type** | Debug diário | Baixo custo, alto ROI |

## Fora de escopo v0.3 (anti-OE)

- Plugin system tipo Oh-My-Zsh  
- Syntax highlighting completo (token colors) — adiado  
- Globbing avançado, job control completo, scripting de arquivo  
- POSIX 100% / bash compatibility  

## Roadmap petrush v0.3 (esta onda)

1. **Aliases** (`alias`/`unalias` + expansão 1ª palavra)  
2. **PETRUSH_PS1** prompt customizável  
3. **Tab completion** (builtins + PATH + arquivos) via linenoise  
4. **History hints** (autosuggest cinza) via linenoise  
5. **`&&` / `||`** no REPL  
6. **`which`** builtin  

Cada fatia: TDD red→green→refactor → commit → push.
