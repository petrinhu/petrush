# How to inventory hardware with `wai`

**Diátaxis type:** how-to  
**Audience:** intermediate user in the REPL or in a script  
**Item:** DOC-DIA-EN · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## When to use

When you want to list machine parts (disk, video, memory, CPU, …) **without root**, by reading `/sys` and `/proc`. Do not use it to read serial/uuid: `wai` omits those fields on purpose.

## Prerequisites

- `petrush` binary with the `wai` builtin (ASM-WAI slice)
- Linux with sysfs mounted (the usual case)
- No special privilege

## Steps

1. Open the REPL or an OSH-0 script.
2. Ask for every section (no flags):

```bash
wai
```

3. Or filter by part:

```bash
wai -disk
wai -mem -cpu
wai -battery -thermal
```

4. For local help:

```bash
wai --help
```

## Verification

- Output shows `# …` headers per requested section.
- Lines whose name contains `serial` / `uuid` do **not** appear.
- Exit 0 on success; exit 2 if the flag is unknown.

## Variations

- In tests, the code accepts an overlay via `petrush_wai_set_root` (C test API). Normal users do not need that: absolute `/sys` and `/proc` paths.
- No flags = `PETRUSH_WAI_ALL` (all 12 parts).

## Related

- Reference: [`wai`](../reference/wai.md)
- Explanation: [Why sysfs](../explanation/why-sysfs-wai-netcom.md)
- Network how-to: [`netcom`](inspect-network-with-netcom.md)
