# Paths XDG del petrush

**Tipo Diátaxis:** reference  
**Audiencia:** usuario, ops local, contribuidores  
**Item:** DOC-DIA-ES · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Sinopsis

Tabla de variables y archivos. Basada en el código actual de `configsh` y `plugin_load`. No afirma conformidad completa con la XDG Base Directory Spec; sigue el subconjunto que el proyecto implementa.

## Configuración (`configsh`)

| Prioridad | Expresión |
|-----------|-----------|
| 1 | `$PETRUSH_CONFIG` |
| 2 | `$XDG_CONFIG_HOME/petrush/config.ini` |
| 3 | `$HOME/.config/petrush/config.ini` |
| 4 | `./.config/petrush/config.ini` (fallback si falta `HOME`) |

Archivo: INI. Secciones: ver [`configsh`](configsh.md).

## Plugins (loader Foundation)

### Carpetas de búsqueda (`.so`)

Orden en `petrush_plugin_search_dirs`:

1. Cada elemento **absoluto** de `$PETRUSH_PLUGIN_PATH` (separador `:`). Los relativos se ignoran.
2. `$XDG_DATA_HOME/petrush/plugins` si `XDG_DATA_HOME` empieza por `/`
3. Si no, `$HOME/.local/share/petrush/plugins` (exige `HOME` absoluto)

Sin `HOME` y sin dirs útiles en `PETRUSH_PLUGIN_PATH`, la lista puede quedar solo con lo que aportó la env.

### Allow-list

| Prioridad | Expresión |
|-----------|-----------|
| 1 | `$PETRUSH_PLUGIN_ALLOW` (path absoluto) |
| 2 | `$XDG_CONFIG_HOME/petrush/plugins.allow` |
| 3 | `$HOME/.config/petrush/plugins.allow` |

Lista ausente o inválida = **default deny**. Entradas: path absoluto + SHA-256 hex 64. Basename solo / glob = rechazado. Archivo world-writable = error.

## Variables de entorno (resumen)

| Variable | Rol |
|----------|-----|
| `PETRUSH_CONFIG` | Override del INI del `configsh` |
| `XDG_CONFIG_HOME` | Base de config (`config.ini`, `plugins.allow`) |
| `XDG_DATA_HOME` | Base de datos (carpeta `plugins`) |
| `HOME` | Fallback clásico `~/.config` y `~/.local/share` |
| `PETRUSH_PLUGIN_PATH` | Extra dirs absolutos de `.so` |
| `PETRUSH_PLUGIN_ALLOW` | Override del archivo allow-list |

## Controles de seguridad (plugins)

- Rechaza path world-writable (`o+w`); dirs con `g+w` solo si sticky
- SHA-256 del contenido **antes** de `dlopen`
- `pudod` **no** carga `.so` ni interpreta estas variables

Detalle: [`docs/security/plugins-threat.md`](../../../security/plugins-threat.md).

## Ver también

- How-to: [Localizar paths](../how-to/localizar-paths-xdg.md)
- Explanation: [Layout XDG](../explanation/layout-xdg-petrush.md)
