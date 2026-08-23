# OSH-0: modo script y shebang

**Tipo Diátaxis:** reference  
**Audiencia:** usuario y contribuidores  
**Item:** DOC-DIA-ES · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Sinopsis

```text
petrush <archivo>
#!/usr/bin/env petrush
```

## Descripción

Cuando `argc >= 2` y `argv[1]` no está vacío, el `main` **no** entra al REPL: llama `petrush_run_script(argv[1])` y termina. Esto cubre shebang (`env` pasa el path del script como `argv[1]`).

OSH-0 **no** es el dialecto OSH completo ni un claim de conformidad POSIX 100%. Es la rebanada mínima: archivo regular, línea a línea, exit = último status.

## Comportamiento garantizado en esta rebanada

| Comportamiento | Detalle |
|----------------|---------|
| Sin banner | No imprime `C23 shell` / prompt |
| Sin `~/.petrushrc` | El rc del REPL no corre |
| Exit = último comando | Ej.: `/bin/false` → exit 1 |
| `exit N` en el script | Termina con N; las líneas siguientes no corren |
| Archivo ausente | Exit 127 |
| No regular (ej.: directorio) | Rechaza; exit ≠ 0 y ≠ 127 |
| Comentarios / líneas vacías | Ignorados (`#` …) |
| `argv[2+]` | Ignorados (sin `$1` en esta rebanada) |
| Mode group-writable | Aceptado en script mode (sin la regla SEC-10 `mode&0022` de `source`) |

## Shebang recomendado

```text
#!/usr/bin/env petrush
```

Exige `petrush` en el `PATH` del proceso que ejecuta el archivo. Alternativa: path absoluto en el shebang, si está instalado (instalación de sistema **no** es meta en esta máquina).

## Lo que OSH-0 aún **no** hace

Lista honestamente incompleta frente a IEEE 1003.1-2017 XCU cap. 2:

- parámetros posicionales (`$1` … `$n`, `$@`)
- `if` / `while` / `for` / `case` / funciones
- `$( )`, `$(( ))`, here-doc
- modo `--posix` estricto
- cargar rc en script mode

Esos ítems están en el riel OSH-1+ del plan; no los inventes en la documentación como si ya existieran.

## Smoke

`tests/smoke/osh0-script.sh` (Docker Fedora 44 / clang en el DoD del TODO).

## Ver también

- Tutorial: [Primer script](../tutorial/primer-script-osh0.md)
- Explanation: [OSH-0 no es POSIX completo](../explanation/osh0-no-es-posix-completo.md)
- Plan: [`docs/plano-shell-avancado.md`](../../../plano-shell-avancado.md)
