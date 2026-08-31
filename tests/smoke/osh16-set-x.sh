#!/bin/bash
# OSH-16: set / set -- / $? / $- / -x / +x / +C / unknown
# Usage: ./osh16-set-x.sh /path/to/petrush
# Expects exit 0 se todos passarem.

set -euo pipefail

PETRUSH="${1:-./build/petrush}"
PASS=0
FAIL=0
TMPROOT=$(mktemp -d /var/tmp/petrush-osh16-XXXXXX)
cleanup() { rm -rf "$TMPROOT"; }
trap cleanup EXIT

pass() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; echo "  detail: $2"; FAIL=$((FAIL+1)); }

echo "=== OSH-16 set/-x/\$?/\$- smoke ($PETRUSH) ==="

# --- 1) set -- positionals; $0 intacto ---
script1="$TMPROOT/pos.sh"
cat > "$script1" <<'SCRIPT'
echo "0=$0"
echo "1=$1"
echo "n=$#"
set -- a b
echo "0b=$0"
echo "1b=$1"
echo "2b=$2"
echo "nb=$#"
set --
echo "nc=$#"
SCRIPT
out1=$("$PETRUSH" "$script1" argX 2>&1) || true
if printf '%s\n' "$out1" | grep -q '1b=a' \
   && printf '%s\n' "$out1" | grep -q '2b=b' \
   && printf '%s\n' "$out1" | grep -q 'nb=2' \
   && printf '%s\n' "$out1" | grep -q 'nc=0' \
   && printf '%s\n' "$out1" | grep -q "0b=$script1"; then
    pass "set-double-dash-positionals"
else
    fail "set-double-dash-positionals" "out=[$out1]"
fi

# --- 2) $? apos false ---
script2="$TMPROOT/status.sh"
cat > "$script2" <<'SCRIPT'
false
echo $?
true
echo $?
SCRIPT
out2=$("$PETRUSH" "$script2" 2>&1) || true
if [ "$out2" = $'1\n0' ]; then
    pass "dollar-question-after-false-true"
else
    fail "dollar-question-after-false-true" "out=[$out2]"
fi

# --- 3) -x traça builtin echo; set -x nao se traça ---
script3="$TMPROOT/xtrace.sh"
cat > "$script3" <<'SCRIPT'
set -x
echo hi
SCRIPT
out3=$("$PETRUSH" "$script3" 2>"$TMPROOT/x.err") || true
err3=$(cat "$TMPROOT/x.err")
if [ "$out3" = "hi" ] \
   && printf '%s\n' "$err3" | grep -q '+ echo hi' \
   && ! printf '%s\n' "$err3" | grep -q '+ set -x'; then
    pass "xtrace-traces-echo-not-set-x"
else
    fail "xtrace-traces-echo-not-set-x" "out=[$out3] err=[$err3]"
fi

# --- 4) +x para o rastreio (set +x e tracado; echo depois nao) ---
script4="$TMPROOT/plusx.sh"
cat > "$script4" <<'SCRIPT'
set -x
set +x
echo after
SCRIPT
out4=$("$PETRUSH" "$script4" 2>"$TMPROOT/px.err") || true
err4=$(cat "$TMPROOT/px.err")
if [ "$out4" = "after" ] \
   && printf '%s\n' "$err4" | grep -q '+ set +x' \
   && ! printf '%s\n' "$err4" | grep -q '+ echo after'; then
    pass "plus-x-stops-trace"
else
    fail "plus-x-stops-trace" "out=[$out4] err=[$err4]"
fi

# --- 5) set sem args imprime environ (PATH=) ---
script5="$TMPROOT/dump.sh"
cat > "$script5" <<'SCRIPT'
set
SCRIPT
out5=$("$PETRUSH" "$script5" 2>&1) || true
if printf '%s\n' "$out5" | grep -q '^PATH='; then
    pass "set-noargs-dumps-environ"
else
    fail "set-noargs-dumps-environ" "out=[$out5]"
fi

# --- 6) unknown -z ≠0 em script ---
script6="$TMPROOT/badz.sh"
cat > "$script6" <<'SCRIPT'
set -z
echo should-not-run
SCRIPT
rc6=0
out6=$("$PETRUSH" "$script6" 2>&1) || rc6=$?
if [ "$rc6" -ne 0 ] && ! printf '%s\n' "$out6" | grep -q 'should-not-run'; then
    pass "unknown-z-aborts-script"
else
    fail "unknown-z-aborts-script" "rc=$rc6 out=[$out6]"
fi

# --- 7) set +C ≠0 ---
script7="$TMPROOT/plusC.sh"
cat > "$script7" <<'SCRIPT'
set +C
SCRIPT
rc7=0
out7=$("$PETRUSH" "$script7" 2>&1) || rc7=$?
if [ "$rc7" -ne 0 ]; then
    pass "plus-C-rejected"
else
    fail "plus-C-rejected" "rc=0 out=[$out7]"
fi

# --- 8) $- contem C e x apos set -x ---
script8="$TMPROOT/flags.sh"
cat > "$script8" <<'SCRIPT'
set -x
echo $-
SCRIPT
out8=$("$PETRUSH" "$script8" 2>/dev/null) || true
if printf '%s\n' "$out8" | grep -q 'C' && printf '%s\n' "$out8" | grep -q 'x'; then
    pass "dollar-minus-Cx"
else
    fail "dollar-minus-Cx" "out=[$out8]"
fi

# --- 9) regressao OSH-2 shift ainda funciona ---
script9="$TMPROOT/shift.sh"
cat > "$script9" <<'SCRIPT'
echo "a=$1"
shift
echo "b=$1"
SCRIPT
out9=$("$PETRUSH" "$script9" one two 2>&1) || true
if [ "$out9" = $'a=one\nb=two' ]; then
    pass "osh2-shift-regression"
else
    fail "osh2-shift-regression" "out=[$out9]"
fi

echo "=== OSH-16: $PASS passed, $FAIL failed ==="
if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
if [ "$PASS" -lt 6 ]; then
    echo "FAIL: expected >=6 passes, got $PASS"
    exit 1
fi
exit 0
