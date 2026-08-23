# `netcom`

**Tipo Diátaxis:** reference  
**Audiência:** utilizador e contribuidores  
**Item:** DOC-DIA-PT · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Sinopse

```text
netcom [-wifi] [-eth] [-bt]
netcom -up IFACE
netcom -down IFACE
netcom -h | --help
```

Builtin do `petrush`. Sem flags de scan = wifi + eth + bt.

## Descrição

- **Scan:** sysfs + netlink GET (read-only). Sem `CAP_NET_ADMIN`.
- **Link:** `-up` / `-down` via helpers C (`ip` / `iw` / `iwd` / `bluetoothctl` se existirem). Sem CAP → `-EPERM` imediato (sem hang, sem spawn). Sem setuid.

Entrada ASM: `petrush_netcom_scan`. I/O em C: `petrush_netcom_scan_impl`. Link set: `petrush_netcom_link_set` (só C).

## Opções

| Flag | Descrição |
|------|-----------|
| `-wifi` | Secção wifi |
| `-eth` | Secção ethernet |
| `-bt` | Secção Bluetooth |
| `-up IFACE` | Sobe a interface (precisa CAP) |
| `-down IFACE` | Desce a interface (precisa CAP) |
| `-h`, `--help` | Ajuda; exit 0 |

Regras:

- `-up` e `-down` são mutuamente exclusivos.
- Não misture flags de scan com `-up`/`-down`.
- `IFACE`: alfanumérico mais `_` `.` `:` `-`; comprimento &lt; `IFNAMSIZ`.

## Códigos de saída

| Code | Significado |
|------|-------------|
| 0 | Sucesso / ajuda |
| 1 | EPERM, helper em falta, timeout, falha de link/scan |
| 2 | Sintaxe (flag desconhecida, IFACE em falta, mistura ilegal) |

## API C

| Símbolo | Papel |
|---------|-------|
| `petrush_netcom_scan` / `_impl` | Scan texto |
| `petrush_netcom_set_root` | Overlay sysfs (testes); sob overlay o netlink é omitido |
| `petrush_netcom_have_cap_net_admin` | 1 se CapEff tem bit 12 |
| `petrush_netcom_link_set(iface, up)` | 0 ok; `-EPERM` / `-EINVAL` / `-ENOENT` / `-ETIMEDOUT` |

Macros: `PETRUSH_NETCOM_WIFI`, `_ETH`, `_BT` em `petrush/asm.h`.

## Notas

- Vetos do projeto: sem `4755`; sem `-up` com root nesta máquina até o líder pedir.
- Timeout de helper: 2000 ms.

## Ver também

- How-to: [Inspecionar rede](../how-to/inspecionar-rede-com-netcom.md)
- Explanation: [sysfs / CAP](../explanation/por-que-sysfs-wai-netcom.md)
