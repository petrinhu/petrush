# Cómo inventariar hardware con `wai`

**Tipo Diátaxis:** how-to  
**Audiencia:** usuario intermedio en el REPL o en script  
**Item:** DOC-DIA-ES · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Cuándo usar

Cuando quieras listar piezas de la máquina (disco, video, memoria, CPU, …) **sin root**, leyendo `/sys` y `/proc`. No lo uses para leer serial/uuid: el `wai` omite esos campos a propósito.

## Prerrequisitos

- Binario `petrush` con el builtin `wai` (rebanada ASM-WAI)
- Linux con sysfs montado (caso normal)
- Sin privilegio especial

## Pasos

1. Abre el REPL o un script OSH-0.
2. Pide todas las secciones (sin flags):

```bash
wai
```

3. O filtra por pieza:

```bash
wai -disk
wai -mem -cpu
wai -battery -thermal
```

4. Para ayuda local:

```bash
wai --help
```

## Verificación

- La salida trae encabezados `# …` por sección pedida.
- Líneas con `serial` / `uuid` en el nombre **no** aparecen.
- Exit 0 en éxito; exit 2 si la flag es desconocida.

## Variaciones

- En pruebas, el código acepta overlay vía `petrush_wai_set_root` (API C de prueba). El usuario normal no necesita eso: rutas absolutas `/sys` y `/proc`.
- Sin flags = `PETRUSH_WAI_ALL` (las 12 piezas).

## Relacionados

- Reference: [`wai`](../reference/wai.md)
- Explanation: [Por qué sysfs](../explanation/por-que-sysfs-wai-netcom.md)
- How-to red: [`netcom`](inspeccionar-red-con-netcom.md)
