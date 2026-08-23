# Paths XDG do petrush

**Tipo Diátaxis:** reference  
**Audiência:** utilizador, ops local, contribuidores  
**Item:** DOC-DIA-PT · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Sinopse

Tabela de variáveis e ficheiros. Baseada no código atual de `configsh` e `plugin_load`. Não afirma conformidade completa com a XDG Base Directory Spec; segue o subconjunto que o projeto implementa.

## Configuração (`configsh`)

| Prioridade | Expressão |
|------------|-----------|
| 1 | `$PETRUSH_CONFIG` |
| 2 | `$XDG_CONFIG_HOME/petrush/config.ini` |
| 3 | `$HOME/.config/petrush/config.ini` |
| 4 | `./.config/petrush/config.ini` (fallback se `HOME` ausente) |

Ficheiro: INI. Secções: ver [`configsh`](configsh.md).

## Plugins (loader Foundation)

### Pastas de busca (`.so`)

Ordem em `petrush_plugin_search_dirs`:

1. Cada elemento **absoluto** de `$PETRUSH_PLUGIN_PATH` (separador `:`). Relativos são ignorados.
2. `$XDG_DATA_HOME/petrush/plugins` se `XDG_DATA_HOME` começar por `/`
3. Senão `$HOME/.local/share/petrush/plugins` (exige `HOME` absoluto)

Sem `HOME` e sem dirs úteis em `PETRUSH_PLUGIN_PATH`, a lista pode ficar só com o que a env forneceu.

### Allow-list

| Prioridade | Expressão |
|------------|-----------|
| 1 | `$PETRUSH_PLUGIN_ALLOW` (path absoluto) |
| 2 | `$XDG_CONFIG_HOME/petrush/plugins.allow` |
| 3 | `$HOME/.config/petrush/plugins.allow` |

Lista ausente ou inválida = **default deny**. Entradas: path absoluto + SHA-256 hex 64. Basename sozinho / glob = rejeitado. Ficheiro world-writable = erro.

## Variáveis de ambiente (resumo)

| Variável | Papel |
|----------|-------|
| `PETRUSH_CONFIG` | Override do INI do `configsh` |
| `XDG_CONFIG_HOME` | Base de config (`config.ini`, `plugins.allow`) |
| `XDG_DATA_HOME` | Base de dados (pasta `plugins`) |
| `HOME` | Fallback clássico `~/.config` e `~/.local/share` |
| `PETRUSH_PLUGIN_PATH` | Extra dirs absolutos de `.so` |
| `PETRUSH_PLUGIN_ALLOW` | Override do ficheiro allow-list |

## Controles de segurança (plugins)

- Recusa path world-writable (`o+w`); dirs com `g+w` só se sticky
- SHA-256 do conteúdo **antes** de `dlopen`
- `pudod` **não** carrega `.so` nem interpreta estas variáveis

Detalhe: [`docs/security/plugins-threat.md`](../../../security/plugins-threat.md).

## Ver também

- How-to: [Localizar paths](../how-to/localizar-paths-xdg.md)
- Explanation: [Layout XDG](../explanation/layout-xdg-petrush.md)
