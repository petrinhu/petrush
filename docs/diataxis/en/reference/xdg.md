# Petrush XDG paths

**Diátaxis type:** reference  
**Audience:** users, local ops, contributors  
**Item:** DOC-DIA-EN · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Synopsis

Table of variables and files. Based on the current `configsh` and `plugin_load` code. Does not claim full conformance with the XDG Base Directory Spec; it follows the subset the project implements.

## Configuration (`configsh`)

| Priority | Expression |
|----------|------------|
| 1 | `$PETRUSH_CONFIG` |
| 2 | `$XDG_CONFIG_HOME/petrush/config.ini` |
| 3 | `$HOME/.config/petrush/config.ini` |
| 4 | `./.config/petrush/config.ini` (fallback if `HOME` is missing) |

File: INI. Sections: see [`configsh`](configsh.md).

## Plugins (Foundation loader)

### Search directories (`.so`)

Order in `petrush_plugin_search_dirs`:

1. Each **absolute** element of `$PETRUSH_PLUGIN_PATH` (separator `:`). Relative ones are ignored.
2. `$XDG_DATA_HOME/petrush/plugins` if `XDG_DATA_HOME` starts with `/`
3. Else `$HOME/.local/share/petrush/plugins` (requires absolute `HOME`)

Without `HOME` and without useful dirs in `PETRUSH_PLUGIN_PATH`, the list may contain only what the env supplied.

### Allow-list

| Priority | Expression |
|----------|------------|
| 1 | `$PETRUSH_PLUGIN_ALLOW` (absolute path) |
| 2 | `$XDG_CONFIG_HOME/petrush/plugins.allow` |
| 3 | `$HOME/.config/petrush/plugins.allow` |

Missing or invalid list = **default deny**. Entries: absolute path + 64 hex SHA-256. Basename alone / glob = rejected. World-writable file = error.

## Environment variables (summary)

| Variable | Role |
|----------|------|
| `PETRUSH_CONFIG` | Override for the `configsh` INI |
| `XDG_CONFIG_HOME` | Config base (`config.ini`, `plugins.allow`) |
| `XDG_DATA_HOME` | Data base (`plugins` folder) |
| `HOME` | Classic fallback `~/.config` and `~/.local/share` |
| `PETRUSH_PLUGIN_PATH` | Extra absolute `.so` dirs |
| `PETRUSH_PLUGIN_ALLOW` | Allow-list file override |

## Security controls (plugins)

- Refuse world-writable paths (`o+w`); dirs with `g+w` only if sticky
- SHA-256 of contents **before** `dlopen`
- `pudod` does **not** load `.so` or interpret these variables

Detail: [`docs/security/plugins-threat.md`](../../../security/plugins-threat.md).

## See also

- How-to: [Locate paths](../how-to/locate-xdg-paths.md)
- Explanation: [XDG layout](../explanation/petrush-xdg-layout.md)
