#!/bin/bash
# OSH-18: set -e / errexit (dash-like)
# Usage: ./osh18-set-e.sh /path/to/petrush
# Expects exit 0 se todos passarem.

set -euo pipefail

PETRUSH="${1:-./build/petrush}"
PASS=0
FAIL=0
TMPROOT=$(mktemp -d /var/tmp/petrush-osh18-XXXXXX)
cleanup() { rm -rf "$TMPROOT"; }
trap cleanup EXIT

pass() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; echo "  detail: $2"; FAIL=$((FAIL+1)); }

echo "=== OSH-18 set -e errexit smoke ($PETRUSH) ==="

# --- 1) false aborta; nao imprime x ---
script1="$TMPROOT/abort.sh"
cat > "$script1" <<'SCRIPT'
set -e
false
echo x
SCRIPT
rc1=0
out1=$("$PETRUSH" "$script1" 2>&1) || rc1=$?
if [ "$rc1" -ne 0 ] && ! printf '%s\n' "$out1" | grep -q '^x$'; then
    pass "false-aborts"
else
    fail "false-aborts" "rc=$rc1 out=[$out1]"
fi

# --- 2) sem -e imprime x ---
script2="$TMPROOT/no-e.sh"
cat > "$script2" <<'SCRIPT'
false
echo x
SCRIPT
rc2=0
out2=$("$PETRUSH" "$script2" 2>&1) || rc2=$?
if [ "$out2" = "x" ]; then
    pass "without-e-continues"
else
    fail "without-e-continues" "rc=$rc2 out=[$out2]"
fi

# --- 3) if false nao aborta; imprime y ---
script3="$TMPROOT/if-cond.sh"
cat > "$script3" <<'SCRIPT'
set -e
if false; then echo x; fi
echo y
SCRIPT
rc3=0
out3=$("$PETRUSH" "$script3" 2>&1) || rc3=$?
if [ "$rc3" -eq 0 ] && [ "$out3" = "y" ]; then
    pass "if-false-cond-ok"
else
    fail "if-false-cond-ok" "rc=$rc3 out=[$out3]"
fi

# --- 4) body false aborta ---
script4="$TMPROOT/if-body.sh"
cat > "$script4" <<'SCRIPT'
set -e
if true; then false; fi
echo y
SCRIPT
rc4=0
out4=$("$PETRUSH" "$script4" 2>&1) || rc4=$?
if [ "$rc4" -ne 0 ] && ! printf '%s\n' "$out4" | grep -q '^y$'; then
    pass "if-body-false-aborts"
else
    fail "if-body-false-aborts" "rc=$rc4 out=[$out4]"
fi

# --- 5) false && nao aborta ---
script5="$TMPROOT/and-left.sh"
cat > "$script5" <<'SCRIPT'
set -e
false && echo x
echo y
SCRIPT
rc5=0
out5=$("$PETRUSH" "$script5" 2>&1) || rc5=$?
if [ "$rc5" -eq 0 ] && [ "$out5" = "y" ]; then
    pass "and-or-non-last-ok"
else
    fail "and-or-non-last-ok" "rc=$rc5 out=[$out5]"
fi

# --- 6) true && false aborta ---
script6="$TMPROOT/and-last.sh"
cat > "$script6" <<'SCRIPT'
set -e
true && false
echo y
SCRIPT
rc6=0
out6=$("$PETRUSH" "$script6" 2>&1) || rc6=$?
if [ "$rc6" -ne 0 ] && ! printf '%s\n' "$out6" | grep -q '^y$'; then
    pass "and-or-last-aborts"
else
    fail "and-or-last-aborts" "rc=$rc6 out=[$out6]"
fi

# --- 7) false | true nao aborta (status = ultimo) ---
script7="$TMPROOT/pipe-ok.sh"
cat > "$script7" <<'SCRIPT'
set -e
false | true
echo y
SCRIPT
rc7=0
out7=$("$PETRUSH" "$script7" 2>&1) || rc7=$?
if [ "$rc7" -eq 0 ] && [ "$out7" = "y" ]; then
    pass "pipe-false-true-ok"
else
    fail "pipe-false-true-ok" "rc=$rc7 out=[$out7]"
fi

# --- 8) true | false aborta ---
script8="$TMPROOT/pipe-fail.sh"
cat > "$script8" <<'SCRIPT'
set -e
true | false
echo y
SCRIPT
rc8=0
out8=$("$PETRUSH" "$script8" 2>&1) || rc8=$?
if [ "$rc8" -ne 0 ] && ! printf '%s\n' "$out8" | grep -q '^y$'; then
    pass "pipe-true-false-aborts"
else
    fail "pipe-true-false-aborts" "rc=$rc8 out=[$out8]"
fi

# --- 9) funcao: -e no body aborta o script ---
script9="$TMPROOT/fn.sh"
cat > "$script9" <<'SCRIPT'
f() { false; echo inner; }
set -e
f
echo outer
SCRIPT
rc9=0
out9=$("$PETRUSH" "$script9" 2>&1) || rc9=$?
if [ "$rc9" -ne 0 ] \
   && ! printf '%s\n' "$out9" | grep -q 'inner' \
   && ! printf '%s\n' "$out9" | grep -q 'outer'; then
    pass "fn-body-aborts-script"
else
    fail "fn-body-aborts-script" "rc=$rc9 out=[$out9]"
fi

# --- 10) cmdsubst $(false) nao mata o pai (dash-like; export = assign petrush) ---
script10="$TMPROOT/cmdsubst.sh"
cat > "$script10" <<'SCRIPT'
set -e
export x=$(false)
echo still
SCRIPT
rc10=0
out10=$("$PETRUSH" "$script10" 2>&1) || rc10=$?
if [ "$rc10" -eq 0 ] && [ "$out10" = "still" ]; then
    pass "cmdsubst-false-parent-ok"
else
    fail "cmdsubst-false-parent-ok" "rc=$rc10 out=[$out10]"
fi

# --- 11) set -eux liga e,u,x em $- ---
script11="$TMPROOT/eux.sh"
cat > "$script11" <<'SCRIPT'
set -eux
echo $-
SCRIPT
rc11=0
out11=$("$PETRUSH" "$script11" 2>/dev/null) || rc11=$?
if [ "$rc11" -eq 0 ] \
   && printf '%s\n' "$out11" | grep -q 'e' \
   && printf '%s\n' "$out11" | grep -q 'u' \
   && printf '%s\n' "$out11" | grep -q 'x' \
   && printf '%s\n' "$out11" | grep -q 'C'; then
    pass "set-eux-dollar-minus"
else
    fail "set-eux-dollar-minus" "rc=$rc11 out=[$out11]"
fi

# --- 12) set +e restaura ---
script12="$TMPROOT/plus-e.sh"
cat > "$script12" <<'SCRIPT'
set -e
set +e
false
echo after
SCRIPT
rc12=0
out12=$("$PETRUSH" "$script12" 2>&1) || rc12=$?
if [ "$rc12" -eq 0 ] && [ "$out12" = "after" ]; then
    pass "plus-e-restores"
else
    fail "plus-e-restores" "rc=$rc12 out=[$out12]"
fi

echo "=== OSH-18: $PASS passed, $FAIL failed ==="
if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
if [ "$PASS" -lt 8 ]; then
    echo "FAIL: expected >=8 passes, got $PASS"
    exit 1
fi
exit 0
