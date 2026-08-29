#!/usr/bin/env bash
# CXX-TUI: configsh TUI raw (ANSI, sem ncurses) + --section/--dump/--check + XDG.
# EXECUTAR TESTE [configsh --dump] [NA FATIA]. Sem 4755.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WORKDIR="${TMPDIR:-/var/tmp}/petrush-cxx-tui-$$"
mkdir -p "$WORKDIR"
cleanup() { rm -rf "$WORKDIR"; }
trap cleanup EXIT

fail() { echo "FAIL: $*" >&2; exit 1; }

echo "=== CXX-TUI: cmake configure + build configsh ==="
cmake -B "$WORKDIR/build" -S "$ROOT" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="${CMAKE_C_COMPILER:-clang}" \
  -DCMAKE_CXX_COMPILER="${CMAKE_CXX_COMPILER:-clang++}" \
  -DPETRUSH_ASM=ON \
  -DENABLE_COVERAGE=OFF
cmake --build "$WORKDIR/build" -j"${CMAKE_BUILD_PARALLEL_LEVEL:-2}" --target configsh

CONFIGSH_BIN="$WORKDIR/build/configsh"
[[ -x "$CONFIGSH_BIN" ]] || fail "configsh ausente"

echo "=== CXX-TUI: sem ncurses / sem mode 4755 ==="
ldd "$CONFIGSH_BIN" | grep -Eiq 'ncurses|libtinfo' && fail "configsh ligou ncurses/tinfo"
MODE="$(stat -c '%a' "$CONFIGSH_BIN")"
[[ "$MODE" != *4755* && "$MODE" != 4755 ]] || fail "mode 4755 proibido (got $MODE)"
echo "OK: ldd sem ncurses; mode=$MODE"

XDG="$WORKDIR/xdg"
mkdir -p "$XDG/petrush"
CFG="$XDG/petrush/config.ini"
cat > "$CFG" <<'EOF'
# sample config for CXX-TUI smoke
[prompt]
ps1=test> 

[aliases]
ll=ls -la

[env]
EDITOR=vi

[history]
max=500
EOF

export XDG_CONFIG_HOME="$XDG"

echo "=== CXX-TUI: --dump (XDG) ==="
DUMP="$("$CONFIGSH_BIN" --dump)"
printf '%s\n' "$DUMP"
echo "$DUMP" | grep -Fq '[prompt]' || fail "--dump missing [prompt]"
echo "$DUMP" | grep -Fq 'ps1=test>' || fail "--dump missing ps1"
echo "$DUMP" | grep -Fq '[aliases]' || fail "--dump missing [aliases]"
echo "$DUMP" | grep -Fq 'll=ls -la' || fail "--dump missing alias"
echo "$DUMP" | grep -Fq '[history]' || fail "--dump missing [history]"
echo "$DUMP" | grep -Fq 'max=500' || fail "--dump missing max"
echo "OK: --dump"

echo "=== CXX-TUI: --section prompt --dump ==="
SEC="$("$CONFIGSH_BIN" --section prompt --dump)"
printf '%s\n' "$SEC"
echo "$SEC" | grep -Fq '[prompt]' || fail "section dump missing [prompt]"
echo "$SEC" | grep -Fq 'ps1=test>' || fail "section dump missing ps1"
echo "$SEC" | grep -Fq '[aliases]' && fail "section dump leaked [aliases]"
echo "OK: --section --dump"

echo "=== CXX-TUI: --check good ==="
set +e
"$CONFIGSH_BIN" --check >"$WORKDIR/check_ok.out" 2>&1
RC=$?
set -e
[[ "$RC" -eq 0 ]] || fail "--check good exit=$RC ($(cat "$WORKDIR/check_ok.out"))"
grep -Eiq 'usage|CXX-TUI lands' "$WORKDIR/check_ok.out" && fail "--check printed stub help"
echo "OK: --check good"

echo "=== CXX-TUI: --check bad (malformed) ==="
cat > "$CFG" <<'EOF'
[prompt]
this-line-is-broken
EOF
set +e
"$CONFIGSH_BIN" --check >"$WORKDIR/check_bad.out" 2>&1
RC=$?
set -e
[[ "$RC" -ne 0 ]] || fail "--check bad must be non-zero"
echo "OK: --check bad exit=$RC"

# restore valid config for dump-defaults / TUI
cat > "$CFG" <<'EOF'
[prompt]
ps1=test> 
[history]
max=500
EOF

echo "=== CXX-TUI: --help ==="
HELP="$("$CONFIGSH_BIN" --help)"
echo "$HELP" | grep -qi 'configsh' || fail "--help missing configsh"
echo "$HELP" | grep -Fq -- '--dump' || fail "--help missing --dump"
echo "$HELP" | grep -Fq -- '--check' || fail "--help missing --check"
echo "$HELP" | grep -Fq -- '--section' || fail "--help missing --section"
echo "OK: --help"

echo "=== CXX-TUI: missing config --dump defaults ==="
rm -f "$CFG"
DEF="$("$CONFIGSH_BIN" --dump)"
echo "$DEF" | grep -Fq '[prompt]' || fail "defaults missing [prompt]"
echo "$DEF" | grep -Eq 'ps1=' || fail "defaults missing ps1"
echo "OK: defaults"

echo "=== CXX-TUI: PTY smoke quit with q ==="
# Allocate a PTY; feed 'q' so TUI exits cleanly. Never touch display :0.
# Prefer python3 pty.fork: util-linux `script -qefc` hangs in GHA containers
# (ubuntu/debian/arch) and never returns after 'q' (ctest Timeout 300s).
# Wait for the TUI banner BEFORE sending q: TCSETSF(RAW) flushes pending input,
# so an early 'q' is discarded and read_key blocks forever.
PTY_OK=0
if command -v python3 >/dev/null 2>&1; then
  set +e
  CONFIGSH_BIN="$CONFIGSH_BIN" python3 - <<'PY' >"$WORKDIR/pty.out" 2>"$WORKDIR/pty.err"
import os, pty, select, signal, sys, time

bin_path = os.environ["CONFIGSH_BIN"]
pid, fd = pty.fork()
if pid == 0:
    os.execv(bin_path, [bin_path])

out = b""
ready_deadline = time.time() + 5.0
ready = False
while time.time() < ready_deadline:
    r, _, _ = select.select([fd], [], [], 0.2)
    if fd in r:
        try:
            chunk = os.read(fd, 4096)
        except OSError:
            break
        if not chunk:
            break
        out += chunk
        if b"q quit" in out or b"Sections" in out:
            ready = True
            break
    wpid, st = os.waitpid(pid, os.WNOHANG)
    if wpid == pid:
        sys.stdout.buffer.write(out)
        sys.stderr.write("PTY TUI exited before ready\n")
        rc = os.waitstatus_to_exitcode(st) if hasattr(os, "waitstatus_to_exitcode") else (st >> 8)
        sys.exit(rc if rc else 1)

if not ready:
    try:
        os.kill(pid, signal.SIGKILL)
    except OSError:
        pass
    try:
        os.waitpid(pid, 0)
    except ChildProcessError:
        pass
    sys.stdout.buffer.write(out)
    sys.stderr.write("PTY TUI banner not seen; killed\n")
    sys.exit(1)

os.write(fd, b"q")

deadline = time.time() + 5.0
status = None
while time.time() < deadline:
    r, _, _ = select.select([fd], [], [], 0.1)
    if fd in r:
        try:
            chunk = os.read(fd, 4096)
        except OSError:
            chunk = b""
        if chunk:
            out += chunk
    wpid, st = os.waitpid(pid, os.WNOHANG)
    if wpid == pid:
        status = st
        break

if status is None:
    # Child still alive after deadline: never unbounded waitpid.
    try:
        os.kill(pid, signal.SIGTERM)
    except OSError:
        pass
    time.sleep(0.3)
    try:
        wpid, st = os.waitpid(pid, os.WNOHANG)
    except ChildProcessError:
        wpid, st = 0, 0
    if wpid != pid:
        try:
            os.kill(pid, signal.SIGKILL)
        except OSError:
            pass
        try:
            wpid, st = os.waitpid(pid, 0)
        except ChildProcessError:
            st = 0
    status = st
    sys.stdout.buffer.write(out)
    sys.stderr.write("PTY TUI still alive after q; killed\n")
    sys.exit(1)

sys.stdout.buffer.write(out)
rc = os.waitstatus_to_exitcode(status) if hasattr(os, "waitstatus_to_exitcode") else (status >> 8)
sys.exit(rc)
PY
  RC=$?
  set -e
  if [[ "$RC" -eq 0 ]]; then
    PTY_OK=1
  else
    echo "WARN: python3 PTY exit=$RC stderr=$(cat "$WORKDIR/pty.err" 2>/dev/null || true)" >&2
  fi
fi
# Fallback: script under a hard timeout (hangs unbounded without it in containers).
if [[ "$PTY_OK" -eq 0 ]] && command -v script >/dev/null 2>&1; then
  set +e
  if command -v timeout >/dev/null 2>&1; then
    # Delay so TCSETSF(RAW) does not flush 'q'; timeout wraps script (hangs in GHA).
    (sleep 0.8; printf 'q') | timeout 5 script -qefc "$CONFIGSH_BIN" "$WORKDIR/typescript" \
      >"$WORKDIR/pty.out" 2>"$WORKDIR/pty.err"
  else
    (sleep 0.8; printf 'q') | script -qefc "$CONFIGSH_BIN" "$WORKDIR/typescript" \
      >"$WORKDIR/pty.out" 2>"$WORKDIR/pty.err" &
    spid=$!
    for _ in 1 2 3 4 5; do
      if ! kill -0 "$spid" 2>/dev/null; then
        break
      fi
      sleep 1
    done
    if kill -0 "$spid" 2>/dev/null; then
      kill -TERM "$spid" 2>/dev/null || true
      sleep 0.2
      kill -KILL "$spid" 2>/dev/null || true
    fi
    wait "$spid"
  fi
  RC=$?
  set -e
  if [[ "$RC" -eq 0 ]]; then
    PTY_OK=1
  else
    echo "WARN: script PTY exit=$RC (timeout/hang expected without python3)" >&2
  fi
fi
[[ "$PTY_OK" -eq 1 ]] || fail "no PTY helper succeeded (python3/script); stderr=$(cat "$WORKDIR/pty.err" 2>/dev/null || true)"
if ! grep -Eaq 'configsh|prompt|history|\[prompt\]' "$WORKDIR/pty.out" 2>/dev/null \
   && ! grep -Eaq 'configsh|prompt|history|\[prompt\]' "$WORKDIR/typescript" 2>/dev/null; then
  # ANSI clear may dominate; accept non-empty PTY capture
  [[ -s "$WORKDIR/pty.out" || -s "$WORKDIR/typescript" ]] \
    || fail "PTY produced no output"
fi
echo "OK: PTY TUI quit"

echo "=== CXX-TUI: flags -fno-exceptions -fno-rtti ==="
CCDB="$WORKDIR/build/compile_commands.json"
grep -E 'src/cxx/.*\.cpp' "$CCDB" | grep -q -- '-fno-exceptions' \
  || fail "-fno-exceptions ausente"
grep -E 'src/cxx/.*\.cpp' "$CCDB" | grep -q -- '-fno-rtti' \
  || fail "-fno-rtti ausente"
echo "OK: C++ flags"

echo "=== CXX-TUI PASS ==="
