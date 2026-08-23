# Layout XDG del petrush

**Tipo Diátaxis:** explanation  
**Audiencia:** contribuidores y quien configura el entorno  
**Item:** DOC-DIA-ES · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Contexto

El petrush separa **configuración editable** (INI, allow-list) de **artefactos de datos** (plugins `.so`). Eso refleja la idea XDG de `CONFIG` versus `DATA`, sin pretender implementar la especificación entera (dirs de cache/state del riel XDG-1 aún están en el plan).

## Modelo mental

| Tipo de archivo | Casa | Ejemplos |
|-----------------|------|----------|
| Preferencias / política | config home | `config.ini`, `plugins.allow` |
| Binarios cargables | data home (+ env) | `…/petrush/plugins/*.so` |
| Override puntual | env propia | `PETRUSH_CONFIG`, `PETRUSH_PLUGIN_PATH`, `PETRUSH_PLUGIN_ALLOW` |

El `configsh` solo habla con el INI. El loader de plugins (`plugin_load.c`) habla con data dirs + allow-list. El REPL interactivo aún usa `~/.petrushrc` en el arranque clásico; el **script OSH-0** no carga ese rc. Unificar rc bajo XDG es rebanada futura (XDG-1 en el plan), no estado actual.

## Trade-offs

- **Pros:** el usuario sabe dónde mirar; CI inyecta `XDG_*` en `/var/tmp` sin tocar el `$HOME` real; el threat model de plugins cierra en paths predecibles.
- **Contras aceptados:** dos árboles (`config` y `data`); quien solo conoce `~/.petrushrc` necesita esta nota; el fallback `./.config/...` del `configsh` es último recurso raro.

## Cuándo aplicar / no aplicar

- Aplica overrides `XDG_CONFIG_HOME` / `PETRUSH_CONFIG` en pruebas y sandboxes.
- No coloques `.so` world-writable "solo para experimentar": el loader rechaza.
- No documentes `~/.cache/petrush` como oficial hasta que exista código.

## Referencias

- Reference: [Paths XDG](../reference/xdg.md)
- How-to: [Localizar paths](../how-to/localizar-paths-xdg.md)
- Plan (XDG-1 futuro): [`docs/plano-shell-avancado.md`](../../../plano-shell-avancado.md)
