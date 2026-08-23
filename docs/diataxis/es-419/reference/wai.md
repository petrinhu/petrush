# `wai`

**Tipo Diátaxis:** reference  
**Audiencia:** usuario y contribuidores  
**Item:** DOC-DIA-ES · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Sinopsis

```text
wai [-disk] [-video] [-mem] [-audio] [-camera] [-keyboard]
    [-usb] [-pci] [-battery] [-thermal] [-cpu] [-board]
wai -h | --help
```

Builtin del `petrush`. Sin flags = todas las secciones.

## Descripción

Inventario de hardware vía sysfs/proc. No pide root. Omite campos cuyo nombre contenga `serial` o `uuid` (case-insensitive). Implementación: entrada ASM `petrush_wai_scan` + cuerpo I/O en C (`petrush_wai_scan_impl`) bajo ASan.

## Opciones

| Flag | Máscara C | Descripción |
|------|-----------|-------------|
| `-disk` | `PETRUSH_WAI_DISK` | Discos / block |
| `-video` | `PETRUSH_WAI_VIDEO` | GPU / DRM |
| `-mem` | `PETRUSH_WAI_MEM` | Memoria |
| `-audio` | `PETRUSH_WAI_AUDIO` | Audio |
| `-camera` | `PETRUSH_WAI_CAMERA` | Cámara |
| `-keyboard` | `PETRUSH_WAI_KEYBOARD` | Teclado |
| `-usb` | `PETRUSH_WAI_USB` | USB |
| `-pci` | `PETRUSH_WAI_PCI` | PCI |
| `-battery` | `PETRUSH_WAI_BATTERY` | Batería |
| `-thermal` | `PETRUSH_WAI_THERMAL` | Térmico |
| `-cpu` | `PETRUSH_WAI_CPU` | CPU |
| `-board` | `PETRUSH_WAI_BOARD` | Board / DMI filtrado |
| `-h`, `--help` | (n/a) | Ayuda; exit 0 |

Las flags pueden combinarse (`wai -disk -cpu`). Flag desconocida → stderr + exit 2.

## Salida

Texto con secciones `# nombre` seguido de líneas leídas de sysfs/proc. Buffer interno típico 64 KiB; el truncamiento reporta error.

## Códigos de salida

| Code | Significado |
|------|-------------|
| 0 | Éxito (incluida la ayuda) |
| 1 | Fallo de scan / memoria / truncamiento |
| 2 | Flag desconocida |

## API C (pruebas / embedding)

| Símbolo | Rol |
|---------|-----|
| `petrush_wai_scan(flags, out, cap)` | Entrada pública |
| `petrush_wai_scan_impl(...)` | Cuerpo I/O |
| `petrush_wai_set_root(root)` | Overlay de prueba (`NULL`/"" = `/sys`+`/proc`) |

Header: `include/petrush/asm.h`.

## Notas

- Sin setuid / sin `4755`.
- No inventa dispositivos: archivo ausente = sección vacía.

## Ver también

- How-to: [Inventariar con `wai`](../how-to/inventariar-con-wai.md)
- [`netcom`](netcom.md)
