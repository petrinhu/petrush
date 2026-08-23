# How to inspect the network with `netcom`

**Diátaxis type:** how-to  
**Audience:** intermediate user  
**Item:** DOC-DIA-EN · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## When to use

When you want to **locate** wifi, ethernet, or Bluetooth interfaces (read-only scan) or, with the right privilege, request `-up`/`-down` via system helpers.

## Prerequisites

- `netcom` builtin in `petrush` (ASM-NET slice)
- Scan: no `CAP_NET_ADMIN`
- `-up` / `-down`: CapEff with `CAP_NET_ADMIN`; helpers on `PATH` (`ip`, `iw`, `iwd`, or `bluetoothctl`) when present

## Steps (scan)

1. Full inventory (wifi + eth + bt):

```bash
netcom
```

2. One family only:

```bash
netcom -wifi
netcom -eth
netcom -bt
```

3. Help:

```bash
netcom --help
```

## Steps (bring link up / down)

1. Confirm you have the capability (the builtin itself fails with a clear message if you do not).
2. Do not mix scan flags with `-up`/`-down`.
3. Examples:

```bash
netcom -up wlan0
netcom -down eth0
```

Without `CAP_NET_ADMIN`, expect stderr with `EPERM` and exit 1. It is **not** a silent no-op. On this development machine, `-up` with root/CAP stays out of scope until the leader gives explicit authorization.

## Verification

| Action | Expected |
|--------|----------|
| `netcom -wifi` | wifi section text; exit 0 |
| `netcom -up IFACE` without CAP | EPERM message; exit 1; no hang |
| unknown flag | exit 2 |
| `-up` and `-down` together | exit 2 |

## Variations

- Sysfs overlay for tests: `petrush_netcom_set_root` API (not a CLI flag).
- If no helper is on PATH while CAP is present: `-ENOENT` mapped to exit 1.

## Related

- Reference: [`netcom`](../reference/netcom.md)
- Explanation: [Why sysfs / CAP](../explanation/why-sysfs-wai-netcom.md)
