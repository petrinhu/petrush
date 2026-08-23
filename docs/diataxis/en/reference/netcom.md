# `netcom`

**Diátaxis type:** reference  
**Audience:** users and contributors  
**Item:** DOC-DIA-EN · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Synopsis

```text
netcom [-wifi] [-eth] [-bt]
netcom -up IFACE
netcom -down IFACE
netcom -h | --help
```

Builtin of `petrush`. No scan flags = wifi + eth + bt.

## Description

- **Scan:** sysfs + netlink GET (read-only). No `CAP_NET_ADMIN`.
- **Link:** `-up` / `-down` via C helpers (`ip` / `iw` / `iwd` / `bluetoothctl` when present). Without CAP → immediate `-EPERM` (no hang, no spawn). No setuid.

ASM entry: `petrush_netcom_scan`. I/O in C: `petrush_netcom_scan_impl`. Link set: `petrush_netcom_link_set` (C only).

## Options

| Flag | Description |
|------|-------------|
| `-wifi` | Wifi section |
| `-eth` | Ethernet section |
| `-bt` | Bluetooth section |
| `-up IFACE` | Bring the interface up (needs CAP) |
| `-down IFACE` | Bring the interface down (needs CAP) |
| `-h`, `--help` | Help; exit 0 |

Rules:

- `-up` and `-down` are mutually exclusive.
- Do not mix scan flags with `-up`/`-down`.
- `IFACE`: alphanumeric plus `_` `.` `:` `-`; length &lt; `IFNAMSIZ`.

## Exit codes

| Code | Meaning |
|------|---------|
| 0 | Success / help |
| 1 | EPERM, missing helper, timeout, link/scan failure |
| 2 | Syntax (unknown flag, missing IFACE, illegal mix) |

## C API

| Symbol | Role |
|--------|------|
| `petrush_netcom_scan` / `_impl` | Text scan |
| `petrush_netcom_set_root` | Sysfs overlay (tests); under overlay, netlink is skipped |
| `petrush_netcom_have_cap_net_admin` | 1 if CapEff has bit 12 |
| `petrush_netcom_link_set(iface, up)` | 0 ok; `-EPERM` / `-EINVAL` / `-ENOENT` / `-ETIMEDOUT` |

Macros: `PETRUSH_NETCOM_WIFI`, `_ETH`, `_BT` in `petrush/asm.h`.

## Notes

- Project vetos: no `4755`; no `-up` with root on this machine until the leader asks.
- Helper timeout: 2000 ms.

## See also

- How-to: [Inspect the network](../how-to/inspect-network-with-netcom.md)
- Explanation: [sysfs / CAP](../explanation/why-sysfs-wai-netcom.md)
