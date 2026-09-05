#!/bin/bash
# OSH-21: trap ACTION INT|TERM|… (handler adiado + poll; modo script)
# Usage: ./osh21-trap-signals.sh /path/to/petrush
# Expects exit 0 se todos passarem.
# Harness mata o PID do petrush (nao o wrapper). Sem $$. Watchdog 5s.

set -euo pipefail

PETRUSH="${1:-./build/petrush}"
PASS=0
FAIL=0
TMPROOT=$(mktemp -d /var/tmp/petrush-osh21-XXXXXX)
WATCHDOG_PIDS=()
IGNORE_PID=

cleanup() {
    local p
    for p in "${WATCHDOG_PIDS[@]:-}"; do
        kill "$p" 2>/dev/null || true
    done
    if [ -n "${IGNORE_PID}" ] && kill -0 "$IGNORE_PID" 2>/dev/null; then
        kill -s TERM "$IGNORE_PID" 2>/dev/null || true
        wait "$IGNORE_PID" 2>/dev/null || true
    fi
    rm -rf "$TMPROOT"
}
trap cleanup EXIT

pass() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; echo "  detail: $2"; FAIL=$((FAIL+1)); }

# Watchdog: mata $1 apos 5s (evita pendurar Docker).
start_watchdog() {
    local target="$1"
    (
        sleep 5
        if kill -0 "$target" 2>/dev/null; then
            kill -s KILL "$target" 2>/dev/null || true
        fi
    ) &
    WATCHDOG_PIDS+=($!)
}

# Arranca petrush em background, espera, envia sinal ao PID do petrush, wait.
# Echo do rc na stdout da funcao (capturar com rc=$(...)).
run_kill_wait() {
    local script="$1" out="$2" sig="$3" presleep="${4:-0.2}"
    local err="${out}.err"
    local rc=0
    local pid
    "$PETRUSH" "$script" >"$out" 2>"$err" &
    pid=$!
    start_watchdog "$pid"
    sleep "$presleep"
    if kill -0 "$pid" 2>/dev/null; then
        kill -s "$sig" "$pid" 2>/dev/null || true
    fi
    wait "$pid" 2>/dev/null || rc=$?
    echo "$rc"
}

echo "=== OSH-21 trap signals smoke ($PETRUSH) ==="

# --- 1) INT: acao corre (loop builtin) e processo nao morre no default ---
script1="$TMPROOT/int-catch.sh"
cat > "$script1" <<'SCRIPT'
trap 'echo caught' INT
while true; do :; done
echo after
SCRIPT
out1="$TMPROOT/int-catch.out"
rc1=0
"$PETRUSH" "$script1" >"$out1" 2>"$TMPROOT/int-catch.err" &
pid1=$!
start_watchdog "$pid1"
sleep 0.2
alive_after_int=0
if kill -0 "$pid1" 2>/dev/null; then
    kill -s INT "$pid1" 2>/dev/null || true
    sleep 0.4
    if kill -0 "$pid1" 2>/dev/null; then
        alive_after_int=1
        kill -s TERM "$pid1" 2>/dev/null || true
    fi
fi
wait "$pid1" 2>/dev/null || rc1=$?
body1=$(cat "$out1" 2>/dev/null || true)
if [ "$alive_after_int" -eq 1 ] && printf '%s\n' "$body1" | grep -qx 'caught'; then
    pass "int-runs-and-continues"
else
    fail "int-runs-and-continues" "rc=$rc1 alive=$alive_after_int out=[$body1]"
fi

# --- 2) INT + exit 0 na acao ---
script2="$TMPROOT/int-exit0.sh"
cat > "$script2" <<'SCRIPT'
trap 'echo caught; exit 0' INT
while true; do :; done
echo never
SCRIPT
out2="$TMPROOT/int-exit0.out"
rc2=$(run_kill_wait "$script2" "$out2" INT)
body2=$(cat "$out2" 2>/dev/null || true)
if [ "$rc2" -eq 0 ] && printf '%s\n' "$body2" | grep -qx 'caught' \
   && ! printf '%s\n' "$body2" | grep -qx 'never'; then
    pass "int-catch-exit0"
else
    fail "int-catch-exit0" "rc=$rc2 out=[$body2]"
fi

# --- 3) TERM ---
script3="$TMPROOT/term.sh"
cat > "$script3" <<'SCRIPT'
trap 'echo term; exit 0' TERM
while true; do :; done
SCRIPT
out3="$TMPROOT/term.out"
rc3=$(run_kill_wait "$script3" "$out3" TERM)
body3=$(cat "$out3" 2>/dev/null || true)
if [ "$rc3" -eq 0 ] && printf '%s\n' "$body3" | grep -qx 'term'; then
    pass "term-catch-exit0"
else
    fail "term-catch-exit0" "rc=$rc3 out=[$body3]"
fi

# --- 4) sem trap: INT mata (rc != 0), sem caught ---
script4="$TMPROOT/notrap.sh"
cat > "$script4" <<'SCRIPT'
while true; do :; done
echo caught
SCRIPT
out4="$TMPROOT/notrap.out"
rc4=$(run_kill_wait "$script4" "$out4" INT)
body4=$(cat "$out4" 2>/dev/null || true)
if [ "$rc4" -ne 0 ] && ! printf '%s\n' "$body4" | grep -q 'caught'; then
    pass "notrap-int-dies"
else
    fail "notrap-int-dies" "rc=$rc4 out=[$body4]"
fi

# --- 5) trap '' INT: ignore — sobrevive ao INT; cleanup com TERM ---
script5="$TMPROOT/ignore.sh"
cat > "$script5" <<'SCRIPT'
trap '' INT
while true; do :; done
SCRIPT
out5="$TMPROOT/ignore.out"
rc5=0
"$PETRUSH" "$script5" >"$out5" 2>"$TMPROOT/ignore.err" &
IGNORE_PID=$!
start_watchdog "$IGNORE_PID"
sleep 0.2
alive_ignore=0
if kill -0 "$IGNORE_PID" 2>/dev/null; then
    kill -s INT "$IGNORE_PID" 2>/dev/null || true
    sleep 0.3
    if kill -0 "$IGNORE_PID" 2>/dev/null; then
        alive_ignore=1
    fi
fi
if kill -0 "$IGNORE_PID" 2>/dev/null; then
    kill -s TERM "$IGNORE_PID" 2>/dev/null || true
fi
wait "$IGNORE_PID" 2>/dev/null || rc5=$?
IGNORE_PID=
if [ "$alive_ignore" -eq 1 ]; then
    pass "ignore-int-survives"
else
    fail "ignore-int-survives" "rc=$rc5 alive=$alive_ignore"
fi

# --- 6) trap - INT restaura default ---
script6="$TMPROOT/reset.sh"
cat > "$script6" <<'SCRIPT'
trap 'echo should-not' INT
trap - INT
while true; do :; done
SCRIPT
out6="$TMPROOT/reset.out"
rc6=$(run_kill_wait "$script6" "$out6" INT)
body6=$(cat "$out6" 2>/dev/null || true)
if [ "$rc6" -ne 0 ] && ! printf '%s\n' "$body6" | grep -q 'should-not'; then
    pass "reset-int-restores-default"
else
    fail "reset-int-restores-default" "rc=$rc6 out=[$body6]"
fi

# --- 7) $? na acao = 128+n (SIGINT=2 → 130) ---
script7="$TMPROOT/status.sh"
cat > "$script7" <<'SCRIPT'
trap 'echo $?; exit 0' INT
while true; do :; done
SCRIPT
out7="$TMPROOT/status.out"
rc7=$(run_kill_wait "$script7" "$out7" INT)
body7=$(cat "$out7" 2>/dev/null || true)
if [ "$rc7" -eq 0 ] && printf '%s\n' "$body7" | grep -qx '130'; then
    pass "trap-status-128-plus-n"
else
    fail "trap-status-128-plus-n" "rc=$rc7 out=[$body7]"
fi

# --- 8) cmdsubst nao herda acao comando do pai ---
script8="$TMPROOT/cmdsubst.sh"
cat > "$script8" <<'SCRIPT'
trap 'echo parent-int' INT
echo $(trap; echo n)
echo done
SCRIPT
rc8=0
out8=$("$PETRUSH" "$script8" 2>&1) || rc8=$?
if [ "$rc8" -eq 0 ] \
   && printf '%s\n' "$out8" | grep -qx 'n' \
   && printf '%s\n' "$out8" | grep -qx 'done' \
   && ! printf '%s\n' "$out8" | grep -q 'parent-int' \
   && ! printf '%s\n' "$out8" | grep -q "trap -- 'echo parent-int' INT"; then
    pass "cmdsubst-no-inherit-command"
else
    fail "cmdsubst-no-inherit-command" "rc=$rc8 out=[$out8]"
fi

# --- 9) sleep: INT durante waitpid (gancho process.c) ---
script9="$TMPROOT/sleep-wait.sh"
cat > "$script9" <<'SCRIPT'
trap 'echo slept; exit 0' INT
sleep 30
echo never
SCRIPT
out9="$TMPROOT/sleep-wait.out"
rc9=$(run_kill_wait "$script9" "$out9" INT 0.3)
body9=$(cat "$out9" 2>/dev/null || true)
if [ "$rc9" -eq 0 ] && printf '%s\n' "$body9" | grep -qx 'slept' \
   && ! printf '%s\n' "$body9" | grep -qx 'never'; then
    pass "int-during-sleep-wait"
else
    fail "int-during-sleep-wait" "rc=$rc9 out=[$body9]"
fi

# --- 10) set -e isento na acao do sinal ---
script10="$TMPROOT/errexit-sig.sh"
cat > "$script10" <<'SCRIPT'
set -e
trap 'false; echo survived; exit 0' INT
while true; do :; done
SCRIPT
out10="$TMPROOT/errexit-sig.out"
rc10=$(run_kill_wait "$script10" "$out10" INT)
body10=$(cat "$out10" 2>/dev/null || true)
if [ "$rc10" -eq 0 ] && printf '%s\n' "$body10" | grep -qx 'survived'; then
    pass "errexit-exempt-in-signal-action"
else
    fail "errexit-exempt-in-signal-action" "rc=$rc10 out=[$body10]"
fi

echo "=== OSH-21: $PASS passed, $FAIL failed ==="
if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
if [ "$PASS" -lt 6 ]; then
    echo "FAIL: expected >=6 passes, got $PASS"
    exit 1
fi
exit 0
