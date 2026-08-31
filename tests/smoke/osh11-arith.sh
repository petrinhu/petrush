#!/bin/bash
# OSH-11: $(( expr )) arith expansion (int64; + - * / % ( ) unary)
# Usage: ./osh11-arith.sh /path/to/petrush
# Expects exit 0 se todos passarem.

set -euo pipefail

PETRUSH="${1:-./build/petrush}"
PASS=0
FAIL=0
TMPROOT=$(mktemp -d /var/tmp/petrush-osh11-XXXXXX)
cleanup() { rm -rf "$TMPROOT"; }
trap cleanup EXIT

pass() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; echo "  detail: $2"; FAIL=$((FAIL+1)); }

echo "=== OSH-11 arith smoke ($PETRUSH) ==="

# --- 1) 1+1 ---
script1="$TMPROOT/plus.sh"
printf '%s\n' 'echo $((1+1))' > "$script1"
out1=$("$PETRUSH" "$script1" 2>&1) || true
if [[ "$out1" == "2" ]]; then
    pass "arith-1-plus-1"
else
    fail "arith-1-plus-1" "out=[$out1]"
fi

# --- 2) $VAR / bare ident ---
script2="$TMPROOT/var.sh"
printf '%s\n' 'export OSH11_V=9' 'echo $((OSH11_V+1))' > "$script2"
out2=$("$PETRUSH" "$script2" 2>&1) || true
if [[ "$out2" == "10" ]]; then
    pass "arith-var"
else
    fail "arith-var" "out=[$out2]"
fi

# --- 3) unario ---
script3="$TMPROOT/unary.sh"
printf '%s\n' 'echo $((-3+1))' > "$script3"
out3=$("$PETRUSH" "$script3" 2>&1) || true
if [[ "$out3" == "-2" ]]; then
    pass "arith-unary"
else
    fail "arith-unary" "out=[$out3]"
fi

# --- 4) parenteses ---
script4="$TMPROOT/paren.sh"
printf '%s\n' 'echo $((2*(3+4)))' > "$script4"
out4=$("$PETRUSH" "$script4" 2>&1) || true
if [[ "$out4" == "14" ]]; then
    pass "arith-parens"
else
    fail "arith-parens" "out=[$out4]"
fi

# --- 5) concat pre$((1))post ---
script5="$TMPROOT/concat.sh"
printf '%s\n' 'echo pre$((1))post' > "$script5"
out5=$("$PETRUSH" "$script5" 2>&1) || true
if [[ "$out5" == "pre1post" ]]; then
    pass "arith-concat"
else
    fail "arith-concat" "out=[$out5]"
fi

# --- 6) div0: sem crash (rc < 128), status != 0, stderr ---
script6="$TMPROOT/div0.sh"
printf '%s\n' 'echo $((1/0))' > "$script6"
set +e
out6=$("$PETRUSH" "$script6" 2>&1)
rc6=$?
set -e
if [[ "$rc6" -gt 0 && "$rc6" -lt 128 ]]; then
    pass "arith-div0-no-crash"
else
    fail "arith-div0-no-crash" "rc=$rc6 out=[$out6]"
fi

# --- 7) backticks continuam literais ---
script7="$TMPROOT/bt.sh"
printf '%s\n' 'echo `echo hi`' > "$script7"
out7=$("$PETRUSH" "$script7" 2>&1) || true
if echo "$out7" | grep -Fq '`echo hi`'; then
    pass "arith-backticks-literal"
else
    fail "arith-backticks-literal" "out=[$out7]"
fi

echo "OSH-11 smoke: $PASS pass, $FAIL fail"
if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
exit 0
