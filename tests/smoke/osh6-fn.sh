#!/bin/bash
# OSH-6: name() { list; } / function name { list; } (sem local; sem return)
# Usage: ./osh6-fn.sh /path/to/petrush
# Expects exit 0 se todos passarem.

set -euo pipefail

PETRUSH="${1:-./build/petrush}"
PASS=0
FAIL=0
TMPROOT=$(mktemp -d /var/tmp/petrush-osh6-XXXXXX)
cleanup() { rm -rf "$TMPROOT"; }
trap cleanup EXIT

pass() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; echo "  detail: $2"; FAIL=$((FAIL+1)); }

echo "=== OSH-6 function smoke ($PETRUSH) ==="

# --- 1) name() { list; }; name ---
script1="$TMPROOT/fn-basic.sh"
printf '%s\n' 'f() { echo hi; }; f' > "$script1"
out1=$("$PETRUSH" "$script1" 2>&1) || true
if [[ "$out1" == "hi" ]]; then
    pass "fn-posix-call"
else
    fail "fn-posix-call" "out=$out1"
fi

# --- 2) function name { list; } ---
script2="$TMPROOT/fn-keyword.sh"
printf '%s\n' 'function g { echo kw; }; g' > "$script2"
out2=$("$PETRUSH" "$script2" 2>&1) || true
if [[ "$out2" == "kw" ]]; then
    pass "fn-keyword-call"
else
    fail "fn-keyword-call" "out=$out2"
fi

# --- 3) posicionais so durante a funcao; restaura depois ---
script3="$TMPROOT/fn-pos.sh"
printf '%s\n' 'f() { echo in:$1; }; f hello; echo out:$1' > "$script3"
out3=$("$PETRUSH" "$script3" world 2>&1) || true
if echo "$out3" | grep -Fq 'in:hello' && echo "$out3" | grep -Fq 'out:world'; then
    pass "fn-pos-restore"
else
    fail "fn-pos-restore" "out=$out3"
fi

# --- 4) } quoted nao fecha o body ---
script4="$TMPROOT/fn-quoted-rbrace.sh"
printf '%s\n' 'f() { echo "}"; }; f' > "$script4"
out4=$("$PETRUSH" "$script4" 2>/dev/null) || true
if [[ "$out4" == "}" ]]; then
    pass "fn-rbrace-quoted"
else
    fail "fn-rbrace-quoted" "out=$out4"
fi

# --- 5) status = ultimo comando do body (sem return nesta onda) ---
# Exige status 1 do builtin false (nao 127 de comando ausente).
script5="$TMPROOT/fn-status.sh"
printf '%s\n' 'f() { false; }; f' > "$script5"
st5=0
"$PETRUSH" "$script5" >/dev/null 2>&1 || st5=$?
if [[ "$st5" -eq 1 ]]; then
    pass "fn-status-last"
else
    fail "fn-status-last" "st=$st5 expected 1"
fi

# --- 6) function name() com args multiplos ---
script6="$TMPROOT/fn-args.sh"
printf '%s\n' 'function h() { echo $1-$2; }; h a b' > "$script6"
out6=$("$PETRUSH" "$script6" 2>&1) || true
if [[ "$out6" == "a-b" ]]; then
    pass "fn-keyword-parens-args"
else
    fail "fn-keyword-parens-args" "out=$out6"
fi

echo
echo "OSH-6 smoke: $PASS pass, $FAIL fail"
if [[ "$FAIL" -ne 0 ]]; then
    exit 1
fi
exit 0
