# `wai`

**Diátaxis type:** reference  
**Audience:** users and contributors  
**Item:** DOC-DIA-EN · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Synopsis

```text
wai [-disk] [-video] [-mem] [-audio] [-camera] [-keyboard]
    [-usb] [-pci] [-battery] [-thermal] [-cpu] [-board]
wai -h | --help
```

Builtin of `petrush`. No flags = every section.

## Description

Hardware inventory via sysfs/proc. Does not require root. Omits fields whose name contains `serial` or `uuid` (case-insensitive). Implementation: ASM entry `petrush_wai_scan` + I/O body in C (`petrush_wai_scan_impl`) under ASan.

## Options

| Flag | C mask | Description |
|------|--------|-------------|
| `-disk` | `PETRUSH_WAI_DISK` | Disks / block |
| `-video` | `PETRUSH_WAI_VIDEO` | GPU / DRM |
| `-mem` | `PETRUSH_WAI_MEM` | Memory |
| `-audio` | `PETRUSH_WAI_AUDIO` | Audio |
| `-camera` | `PETRUSH_WAI_CAMERA` | Camera |
| `-keyboard` | `PETRUSH_WAI_KEYBOARD` | Keyboard |
| `-usb` | `PETRUSH_WAI_USB` | USB |
| `-pci` | `PETRUSH_WAI_PCI` | PCI |
| `-battery` | `PETRUSH_WAI_BATTERY` | Battery |
| `-thermal` | `PETRUSH_WAI_THERMAL` | Thermal |
| `-cpu` | `PETRUSH_WAI_CPU` | CPU |
| `-board` | `PETRUSH_WAI_BOARD` | Board / filtered DMI |
| `-h`, `--help` | (n/a) | Help; exit 0 |

Flags may be combined (`wai -disk -cpu`). Unknown flag → stderr + exit 2.

## Output

Text with `# name` sections followed by lines read from sysfs/proc. Typical internal buffer 64 KiB; truncation reports an error.

## Exit codes

| Code | Meaning |
|------|---------|
| 0 | Success (including help) |
| 1 | Scan / memory / truncation failure |
| 2 | Unknown flag |

## C API (tests / embedding)

| Symbol | Role |
|--------|------|
| `petrush_wai_scan(flags, out, cap)` | Public entry |
| `petrush_wai_scan_impl(...)` | I/O body |
| `petrush_wai_set_root(root)` | Test overlay (`NULL`/"" = `/sys`+`/proc`) |

Header: `include/petrush/asm.h`.

## Notes

- No setuid / no `4755`.
- Does not invent devices: missing file = empty section.

## See also

- How-to: [Inventory with `wai`](../how-to/inventory-with-wai.md)
- [`netcom`](netcom.md)
