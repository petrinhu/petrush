#!/bin/bash
# OSH-19: trap dump / reset / ignore (ainda sem disparar EXIT/sinais)
# Usage: ./osh19-trap.sh /path/to/petrush
# Expects exit 0 se todos passarem.

set -euo pipefail

PETRUSH="${1:-./build/petrush}"
PASS=0
FAIL=0
TMPROOT=$(mktemp -d /var/tmp/petrush-osh19-XXXXXX)
cleanup() { rm -rf "$TMPROOT"; }
trap cleanup EXIT

pass() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; echo "  detail: $2"; FAIL=$((FAIL+1)); }

echo "=== OSH-19 trap dump/ignore/reset smoke ($PETRUSH) ==="

# --- 1) dump EXIT ---
script1="$TMPROOT/dump-exit.sh"
cat > "$script1" <<'SCRIPT'
trap 'echo x' EXIT
trap
SCRIPT
out1=$("$PETRUSH" "$script1" 2>&1) || true
if printf '%s\n' "$out1" | grep -q "trap -- 'echo x' EXIT"; then
    pass "dump-exit"
else
    fail "dump-exit" "out=[$out1]"
fi

# --- 2) reset remove EXIT do dump ---
script2="$TMPROOT/reset-exit.sh"
cat > "$script2" <<'SCRIPT'
trap 'echo x' EXIT
trap - EXIT
trap
echo after
SCRIPT
out2=$("$PETRUSH" "$script2" 2>&1) || true
if printf '%s\n' "$out2" | grep -q '^after$' \
   && ! printf '%s\n' "$out2" | grep -q 'EXIT'; then
    pass "reset-exit"
else
    fail "reset-exit" "out=[$out2]"
fi

# --- 3) ignore INT no dump ---
script3="$TMPROOT/ignore-int.sh"
cat > "$script3" <<'SCRIPT'
trap '' INT
trap
SCRIPT
out3=$("$PETRUSH" "$script3" 2>&1) || true
if printf '%s\n' "$out3" | grep -q "trap -- '' INT"; then
    pass "ignore-int-dump"
else
    fail "ignore-int-dump" "out=[$out3]"
fi

# --- 4) INT / SIGINT / 2 equivalentes (dump = INT) ---
script4="$TMPROOT/alias-int.sh"
cat > "$script4" <<'SCRIPT'
trap 'echo i' SIGINT
trap
trap - INT
trap 'echo j' 2
trap
SCRIPT
out4=$("$PETRUSH" "$script4" 2>&1) || true
if printf '%s\n' "$out4" | grep -c " INT" | grep -q '^[12]$' \
   && printf '%s\n' "$out4" | grep -q "trap -- 'echo i' INT" \
   && printf '%s\n' "$out4" | grep -q "trap -- 'echo j' INT" \
   && ! printf '%s\n' "$out4" | grep -q 'SIGINT'; then
    pass "int-sigint-2-equivalent"
else
    fail "int-sigint-2-equivalent" "out=[$out4]"
fi

# --- 5) ERR / CHLD / KILL → status ≠0 ---
script5="$TMPROOT/bad-names.sh"
cat > "$script5" <<'SCRIPT'
trap 'x' ERR
SCRIPT
rc5=0
"$PETRUSH" "$script5" >/dev/null 2>&1 || rc5=$?
script5b="$TMPROOT/bad-chld.sh"
cat > "$script5b" <<'SCRIPT'
trap 'x' CHLD
SCRIPT
rc5b=0
"$PETRUSH" "$script5b" >/dev/null 2>&1 || rc5b=$?
script5c="$TMPROOT/bad-kill.sh"
cat > "$script5c" <<'SCRIPT'
trap 'x' KILL
SCRIPT
rc5c=0
"$PETRUSH" "$script5c" >/dev/null 2>&1 || rc5c=$?
if [ "$rc5" -ne 0 ] && [ "$rc5b" -ne 0 ] && [ "$rc5c" -ne 0 ]; then
    pass "err-chld-kill-rejected"
else
    fail "err-chld-kill-rejected" "rcERR=$rc5 rcCHLD=$rc5b rcKILL=$rc5c"
fi

# --- 6) nome invalido aborta (nao imprime eco seguinte) ---
script6="$TMPROOT/invalid.sh"
cat > "$script6" <<'SCRIPT'
trap 'x' NOTASIGNAL
echo should-not-run
SCRIPT
rc6=0
out6=$("$PETRUSH" "$script6" 2>&1) || rc6=$?
if [ "$rc6" -ne 0 ] && ! printf '%s\n' "$out6" | grep -q 'should-not-run'; then
    pass "invalid-aborts-script"
else
    fail "invalid-aborts-script" "rc=$rc6 out=[$out6]"
fi

# --- 7) help/info sem "sem trap"; help menciona trap ---
out_help=$("$PETRUSH" -c 'help' 2>&1) || true
# petrush pode nao ter -c; usar script
script7="$TMPROOT/help.sh"
cat > "$script7" <<'SCRIPT'
help
info
SCRIPT
out7=$("$PETRUSH" "$script7" 2>&1) || true
if printf '%s\n' "$out7" | grep -q 'trap' \
   && ! printf '%s\n' "$out7" | grep -q 'sem trap'; then
    pass "help-info-mentions-trap"
else
    fail "help-info-mentions-trap" "out=[$out7]"
fi

# --- 8) trap sem ACTION com condition = usage error ---
script8="$TMPROOT/usage.sh"
cat > "$script8" <<'SCRIPT'
trap EXIT
echo should-not-run
SCRIPT
rc8=0
out8=$("$PETRUSH" "$script8" 2>&1) || rc8=$?
if [ "$rc8" -ne 0 ] && ! printf '%s\n' "$out8" | grep -q 'should-not-run'; then
    pass "usage-no-action-aborts"
else
    fail "usage-no-action-aborts" "rc=$rc8 out=[$out8]"
fi

echo "=== OSH-19: $PASS passed, $FAIL failed ==="
if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
if [ "$PASS" -lt 6 ]; then
    echo "FAIL: expected >=6 passes, got $PASS"
    exit 1
fi
exit 0
