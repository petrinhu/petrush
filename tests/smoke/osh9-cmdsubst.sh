#!/bin/bash
# OSH-9: command subst $(cmd); trailing newlines stripped; sem backticks
# Usage: ./osh9-cmdsubst.sh /path/to/petrush
# Expects exit 0 se todos passarem.

set -euo pipefail

PETRUSH="${1:-./build/petrush}"
PASS=0
FAIL=0
TMPROOT=$(mktemp -d /var/tmp/petrush-osh9-XXXXXX)
cleanup() { rm -rf "$TMPROOT"; }
trap cleanup EXIT

pass() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; echo "  detail: $2"; FAIL=$((FAIL+1)); }

echo "=== OSH-9 cmdsubst smoke ($PETRUSH) ==="

# --- 1) echo $(echo hi) → hi ---
script1="$TMPROOT/basic.sh"
printf '%s\n' 'echo $(echo hi)' > "$script1"
out1=$("$PETRUSH" "$script1" 2>&1) || true
if [ "$out1" = "hi" ]; then
    pass "basic-cmdsubst"
else
    fail "basic-cmdsubst" "out=[$out1]"
fi

# --- 2) trailing newlines stripped (echo hi; echo → hi\n\n) ---
script2="$TMPROOT/strip.sh"
printf '%s\n' 'echo Z$(echo hi; echo)Z' > "$script2"
out2=$("$PETRUSH" "$script2" 2>&1) || true
if [ "$out2" = "ZhiZ" ]; then
    pass "trailing-newlines-stripped"
else
    fail "trailing-newlines-stripped" "out=[$out2]"
fi

# --- 3) nested 1: echo $(echo $(echo hi)) → hi ---
script3="$TMPROOT/nest.sh"
printf '%s\n' 'echo $(echo $(echo hi))' > "$script3"
out3=$("$PETRUSH" "$script3" 2>&1) || true
if [ "$out3" = "hi" ]; then
    pass "nested-one"
else
    fail "nested-one" "out=[$out3]"
fi

# --- 4) concat pre$(echo X)post ---
script4="$TMPROOT/concat.sh"
printf '%s\n' 'echo pre$(echo X)post' > "$script4"
out4=$("$PETRUSH" "$script4" 2>&1) || true
if [ "$out4" = "preXpost" ]; then
    pass "concat"
else
    fail "concat" "out=[$out4]"
fi

# --- 5) echo "$(echo hi)" → hi ---
script5="$TMPROOT/quoted.sh"
printf '%s\n' 'echo "$(echo hi)"' > "$script5"
out5=$("$PETRUSH" "$script5" 2>&1) || true
if [ "$out5" = "hi" ]; then
    pass "double-quoted-cmdsubst"
else
    fail "double-quoted-cmdsubst" "out=[$out5]"
fi

# --- 6) backticks NAO expandem ---
script6="$TMPROOT/bt.sh"
printf '%s\n' 'echo `echo hi`' > "$script6"
out6=$("$PETRUSH" "$script6" 2>&1) || true
if echo "$out6" | grep -Fq '`echo hi`'; then
    pass "backticks-literal"
else
    fail "backticks-literal" "out=[$out6]"
fi

# --- 7) $(( NAO e cmdsubst: OSH-11 avalia para 2 (sem hook $( )) ---
script7="$TMPROOT/arith.sh"
printf '%s\n' 'echo $((1+1))' > "$script7"
out7=$("$PETRUSH" "$script7" 2>&1) || true
if [ "$out7" = '2' ]; then
    pass "arith-not-cmdsubst"
else
    fail "arith-not-cmdsubst" "out=[$out7]"
fi

# --- 8) profundidade 3 nao explode (sem crash; saida finita) ---
script8="$TMPROOT/depth.sh"
printf '%s\n' 'echo $(echo $(echo $(echo hi)))' > "$script8"
set +e
out8=$("$PETRUSH" "$script8" 2>&1)
rc8=$?
set -e
# Aceita vazio ou qualquer texto curto; recusa so se sinal/crash tipico
if [ "$rc8" -lt 128 ]; then
    pass "depth3-no-explode"
else
    fail "depth3-no-explode" "rc=$rc8 out=[$out8]"
fi

echo "OSH-9 smoke: $PASS pass, $FAIL fail"
if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
exit 0
