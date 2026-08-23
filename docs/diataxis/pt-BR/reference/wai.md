# `wai`

**Tipo Diátaxis:** reference  
**Audiência:** utilizador e contribuidores  
**Item:** DOC-DIA-PT · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Sinopse

```text
wai [-disk] [-video] [-mem] [-audio] [-camera] [-keyboard]
    [-usb] [-pci] [-battery] [-thermal] [-cpu] [-board]
wai -h | --help
```

Builtin do `petrush`. Sem flags = todas as secções.

## Descrição

Inventário de hardware via sysfs/proc. Não pede root. Omite campos cujo nome contenha `serial` ou `uuid` (case-insensitive). Implementação: entrada ASM `petrush_wai_scan` + corpo I/O em C (`petrush_wai_scan_impl`) sob ASan.

## Opções

| Flag | Máscara C | Descrição |
|------|-----------|-----------|
| `-disk` | `PETRUSH_WAI_DISK` | Discos / block |
| `-video` | `PETRUSH_WAI_VIDEO` | GPU / DRM |
| `-mem` | `PETRUSH_WAI_MEM` | Memória |
| `-audio` | `PETRUSH_WAI_AUDIO` | Áudio |
| `-camera` | `PETRUSH_WAI_CAMERA` | Câmara |
| `-keyboard` | `PETRUSH_WAI_KEYBOARD` | Teclado |
| `-usb` | `PETRUSH_WAI_USB` | USB |
| `-pci` | `PETRUSH_WAI_PCI` | PCI |
| `-battery` | `PETRUSH_WAI_BATTERY` | Bateria |
| `-thermal` | `PETRUSH_WAI_THERMAL` | Térmico |
| `-cpu` | `PETRUSH_WAI_CPU` | CPU |
| `-board` | `PETRUSH_WAI_BOARD` | Board / DMI filtrado |
| `-h`, `--help` | (n/a) | Ajuda; exit 0 |

Flags podem combinar-se (`wai -disk -cpu`). Flag desconhecida → stderr + exit 2.

## Saída

Texto com secções `# nome` seguido de linhas lidas de sysfs/proc. Buffer interno típico 64 KiB; truncamento reporta erro.

## Códigos de saída

| Code | Significado |
|------|-------------|
| 0 | Sucesso (incluindo ajuda) |
| 1 | Falha de scan / memória / truncamento |
| 2 | Flag desconhecida |

## API C (testes / embedding)

| Símbolo | Papel |
|---------|-------|
| `petrush_wai_scan(flags, out, cap)` | Entrada pública |
| `petrush_wai_scan_impl(...)` | Corpo I/O |
| `petrush_wai_set_root(root)` | Overlay de teste (`NULL`/"" = `/sys`+`/proc`) |

Header: `include/petrush/asm.h`.

## Notas

- Sem setuid / sem `4755`.
- Não inventa dispositivos: ficheiro em falta = secção vazia.

## Ver também

- How-to: [Inventariar com `wai`](../how-to/inventariar-com-wai.md)
- [`netcom`](netcom.md)
