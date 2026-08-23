# Layout XDG do petrush

**Tipo Diátaxis:** explanation  
**Audiência:** contribuidores e quem configura o ambiente  
**Item:** DOC-DIA-PT · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Contexto

O petrush separa **configuração editável** (INI, allow-list) de **artefactos de dados** (plugins `.so`). Isso espelha a ideia XDG de `CONFIG` versus `DATA`, sem pretender implementar a especificação inteira (dirs de cache/state do trilho XDG-1 ainda estão no plano).

## Modelo mental

| Tipo de ficheiro | Casa | Exemplos |
|------------------|------|----------|
| Preferências / política | config home | `config.ini`, `plugins.allow` |
| Binários carregáveis | data home (+ env) | `…/petrush/plugins/*.so` |
| Override pontual | env própria | `PETRUSH_CONFIG`, `PETRUSH_PLUGIN_PATH`, `PETRUSH_PLUGIN_ALLOW` |

O `configsh` só fala com o INI. O loader de plugins (`plugin_load.c`) fala com data dirs + allow-list. O REPL interativo ainda usa `~/.petrushrc` no arranque clássico; o **script OSH-0** não carrega esse rc. Unificar rc sob XDG é fatia futura (XDG-1 no plano), não estado atual.

## Trade-offs

- **Pros:** utilizador sabe onde olhar; CI injeta `XDG_*` em `/var/tmp` sem tocar o `$HOME` real; threat model de plugins fecha em paths previsíveis.
- **Contras aceites:** duas árvores (`config` e `data`); quem só conhece `~/.petrushrc` precisa desta nota; fallback `./.config/...` do `configsh` é último recurso raro.

## Quando aplicar / não aplicar

- Aplique overrides `XDG_CONFIG_HOME` / `PETRUSH_CONFIG` em testes e sandboxes.
- Não coloque `.so` world-writable "só para experimentar": o loader recusa.
- Não documente `~/.cache/petrush` como oficial até existir código.

## Referências

- Reference: [Paths XDG](../reference/xdg.md)
- How-to: [Localizar paths](../how-to/localizar-paths-xdg.md)
- Plano (XDG-1 futuro): [`docs/plano-shell-avancado.md`](../../../plano-shell-avancado.md)
