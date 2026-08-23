# Tutorial: your first petrush script (OSH-0)

**Diátaxis type:** tutorial  
**Audience:** novice who already built the `petrush` binary  
**Estimated time:** 10 min  
**Prerequisites:** `petrush` binary on `PATH` or a known absolute path; Linux  
**What you will have at the end:** an executable file with a shebang that prints one line and exits with status 0  
**Item:** DOC-DIA-EN · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## 1. Confirm the binary

```bash
which petrush || ls -l ./build/petrush
```

If you have not built it yet, use a local build (example):

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --target petrush
export PATH="$(pwd)/build:$PATH"
```

Expected output: the `petrush` executable exists and is runnable.

## 2. Create the script

Create `hello.sh` (any name works):

```bash
cat > /var/tmp/hello.sh <<'EOF'
#!/usr/bin/env petrush
echo osh0-hello
EOF
chmod 0755 /var/tmp/hello.sh
```

The shebang (`#!/usr/bin/env petrush`) asks the kernel to start `petrush` from `PATH` and pass the file as an argument. This is **OSH-0** mode: script, no banner, no `~/.petrushrc`.

## 3. Run via shebang

```bash
/var/tmp/hello.sh
echo "exit=$?"
```

Expected output:

```text
osh0-hello
exit=0
```

You must not see the `C23 shell` banner or the `petrush>` prompt.

## 4. Run by passing the file to the binary

Equivalent without relying on the executable bit:

```bash
petrush /var/tmp/hello.sh
echo "exit=$?"
```

Same output. Internally `main` calls `petrush_run_script` when `argv[1]` is present.

## 5. See the last command status

```bash
cat > /var/tmp/last.sh <<'EOF'
#!/usr/bin/env petrush
/bin/false
EOF
chmod 0755 /var/tmp/last.sh
/var/tmp/last.sh
echo "exit=$?"
```

Expected output: `exit=1` (status from `/bin/false`).

## 6. (Optional) Look at hardware and config

Builtins also run in OSH-0 scripts:

```bash
cat > /var/tmp/hw.sh <<'EOF'
#!/usr/bin/env petrush
wai -cpu
EOF
chmod 0755 /var/tmp/hw.sh
/var/tmp/hw.sh | head
```

XDG config lives in the separate `configsh` binary (not in the REPL):

```bash
configsh --dump | head
```

If `config.ini` is missing, `configsh` uses in-memory defaults (`ps1=petrush> `, `history.max=1000`).

## Check

- [ ] Shebang script prints and exits 0
- [ ] No REPL banner in script mode
- [ ] `/bin/false` in the script propagates exit 1
- [ ] (Optional) `wai` and `configsh --dump` respond

## Summary

You built: an OSH-0 script with a shebang.  
You learned:

- `petrush file` enters script mode (no rc, no banner)
- process exit is the last command status (or `exit N`)
- OSH-0 does **not** yet expose `$1` or `if` (later waves)
- `wai` and `configsh` are separate surfaces (builtin and binary)

## Next steps

- How-to: [Inventory with `wai`](../how-to/inventory-with-wai.md)
- How-to: [Use `configsh`](../how-to/use-configsh.md)
- Reference: [OSH-0 / shebang](../reference/osh0-shebang.md)
- Explanation: [OSH-0 is not full POSIX](../explanation/osh0-is-not-full-posix.md)
