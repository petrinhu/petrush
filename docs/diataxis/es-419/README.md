# Documentación Diátaxis (es-419)

> **Tipo:** hub de navegación (no es uno de los cuatro tipos Diátaxis).
> **Audiencia:** usuario de petrush (novato a intermedio) y contribuidores.
> **Idioma canónico de este árbol:** es-419 (español latinoamericano).
> **Item:** DOC-DIA-ES · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23
> **Versión del producto:** post OSH-0 / CXX-TUI / ASM-WAI / ASM-NET (sin tag de release en esta rebanada).

Mapa Diátaxis del petrush en español latinoamericano. Cada página tiene **un tipo**, **una audiencia** y cubre solo lo que el código hace hoy. Fuente canónica de contenido: [`docs/diataxis/pt-BR/`](../pt-BR/). Otras traducciones: DOC-DIA-EN (onda siguiente).

## Aviso de alcance

- OSH-0 **no** es POSIX completo. Es modo script + shebang + exit del último comando. Posicionales (`$1`), `if`/`while` y el resto del riel OSH vienen después.
- Sin producción en esta máquina: pruebas en Docker; sin `4755`; `netcom -up`/`-down` exige `CAP_NET_ADMIN` (si no, EPERM claro).
- Fuentes de verdad: código en `src/`, contrato en [`docs/architecture.md`](../../architecture.md) y ADR-001.

## Tutorial (aprender haciendo)

| Página | Lo que construyes |
|--------|-------------------|
| [Primer script OSH-0](tutorial/primer-script-osh0.md) | Script ejecutable con shebang, sin REPL |

## How-to (resolver un objetivo)

| Página | Objetivo |
|--------|----------|
| [Inventariar hardware con `wai`](how-to/inventariar-con-wai.md) | Listar disco, CPU, memoria, etc. sin root |
| [Inspeccionar red con `netcom`](how-to/inspeccionar-red-con-netcom.md) | Scan wifi/eth/bt; entender EPERM en `-up` |
| [Usar el `configsh`](how-to/usar-configsh.md) | Dump, check y TUI de la config INI |
| [Localizar paths XDG](how-to/localizar-paths-xdg.md) | Hallar `config.ini`, plugins y allow-list |

## Reference (consultar)

| Página | Superficie |
|--------|------------|
| [`wai`](reference/wai.md) | Builtin de inventario sysfs/proc |
| [`netcom`](reference/netcom.md) | Builtin de scan / link up-down |
| [`configsh`](reference/configsh.md) | Binario C++23 de configuración |
| [OSH-0 / shebang](reference/osh0-shebang.md) | Modo `petrush archivo` |
| [Paths XDG](reference/xdg.md) | Variables y archivos bajo XDG |

## Explanation (entender)

| Página | Pregunta |
|--------|----------|
| [OSH-0 no es POSIX completo](explanation/osh0-no-es-posix-completo.md) | Por qué el riel empieza por el shebang |
| [Por qué `wai` y `netcom` leen sysfs](explanation/por-que-sysfs-wai-netcom.md) | Modelo mental y límites de privilegio |
| [Layout XDG del petrush](explanation/layout-xdg-petrush.md) | Por qué config y plugins viven en sitios distintos |

## Relacionados (fuera de este árbol)

- Guía para principiantes (mezcla rápida): [`docs/beginner-guide.md`](../../beginner-guide.md)
- Arquitectura / stack triple: [`docs/architecture.md`](../../architecture.md)
- Threat model de plugins: [`docs/security/plugins-threat.md`](../../security/plugins-threat.md)
- Plan OSH: [`docs/plano-shell-avancado.md`](../../plano-shell-avancado.md)
