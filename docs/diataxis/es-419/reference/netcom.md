# `netcom`

**Tipo Diátaxis:** reference  
**Audiencia:** usuario y contribuidores  
**Item:** DOC-DIA-ES · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Sinopsis

```text
netcom [-wifi] [-eth] [-bt]
netcom -up IFACE
netcom -down IFACE
netcom -h | --help
```

Builtin del `petrush`. Sin flags de scan = wifi + eth + bt.

## Descripción

- **Scan:** sysfs + netlink GET (solo lectura). Sin `CAP_NET_ADMIN`.
- **Link:** `-up` / `-down` vía helpers C (`ip` / `iw` / `iwd` / `bluetoothctl` si existen). Sin CAP → `-EPERM` inmediato (sin hang, sin spawn). Sin setuid.

Entrada ASM: `petrush_netcom_scan`. I/O en C: `petrush_netcom_scan_impl`. Link set: `petrush_netcom_link_set` (solo C).

## Opciones

| Flag | Descripción |
|------|-------------|
| `-wifi` | Sección wifi |
| `-eth` | Sección ethernet |
| `-bt` | Sección Bluetooth |
| `-up IFACE` | Sube la interfaz (necesita CAP) |
| `-down IFACE` | Baja la interfaz (necesita CAP) |
| `-h`, `--help` | Ayuda; exit 0 |

Reglas:

- `-up` y `-down` son mutuamente exclusivos.
- No mezcles flags de scan con `-up`/`-down`.
- `IFACE`: alfanumérico más `_` `.` `:` `-`; longitud &lt; `IFNAMSIZ`.

## Códigos de salida

| Code | Significado |
|------|-------------|
| 0 | Éxito / ayuda |
| 1 | EPERM, helper ausente, timeout, fallo de link/scan |
| 2 | Sintaxis (flag desconocida, IFACE ausente, mezcla ilegal) |

## API C

| Símbolo | Rol |
|---------|-----|
| `petrush_netcom_scan` / `_impl` | Scan texto |
| `petrush_netcom_set_root` | Overlay sysfs (pruebas); bajo overlay se omite el netlink |
| `petrush_netcom_have_cap_net_admin` | 1 si CapEff tiene el bit 12 |
| `petrush_netcom_link_set(iface, up)` | 0 ok; `-EPERM` / `-EINVAL` / `-ENOENT` / `-ETIMEDOUT` |

Macros: `PETRUSH_NETCOM_WIFI`, `_ETH`, `_BT` en `petrush/asm.h`.

## Notas

- Vetos del proyecto: sin `4755`; sin `-up` con root en esta máquina hasta que el líder lo pida.
- Timeout de helper: 2000 ms.

## Ver también

- How-to: [Inspeccionar red](../how-to/inspeccionar-red-con-netcom.md)
- Explanation: [sysfs / CAP](../explanation/por-que-sysfs-wai-netcom.md)
