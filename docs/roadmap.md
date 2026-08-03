# Roadmap (Onda 3+)

## Entregue em v0.2.0 (2026-08-03)

**Demanda/ROI**: pedido explícito do líder em modo autônomo (`1+3+4` = wiki + limpar forgejo + Onda 3 real).

### NEW-19
- `info` (placeholder) — já em v0.1.

### NEW-20 (mínimo anti-OE)
- [x] Pipes `|` (somente estágios **externos**; builtins no pipe → erro claro)
- [x] Redirecionamento `>`, `>>`, `<` (externos e builtins)
- [ ] Scripting leve de arquivo — **não** nesta fatia
- [ ] Background `&`, `2>`, globbing — **não** nesta fatia

### NEW-21
- Porte continua solo; sem re-escalar constelação.

## Próximo (só com demanda)

- Builtins no meio de pipeline (subshell)
- `2>` / `&>` / `2>&1`
- Scripting de arquivo (`.petrush` batch)
- Globbing simples

## Gate v0.2
- Testes unitários + smoke (pipes/redir) verdes
- Decisão autônoma registrada no CHANGELOG (confirmar retroativamente)

Ver: CLAUDE.md, TODO.md, beginner-guide.md.