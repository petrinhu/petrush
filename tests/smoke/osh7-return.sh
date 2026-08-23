#!/bin/bash
# OSH-7: builtin return [n] so dentro de funcao (default n=0; fora = erro, nao exit)
# Usage: ./osh7-return.sh /path/to/petrush
# Expects exit 0 se todos passarem.

set -euo pipefail

PETRUSH="${1:-./build/petrush}"
PASS=0
FAIL=0
TMPROOT=$(mktemp -d /var/tmp/petrush-osh7-XXXXXX)
cleanup() { rm -rf "$TMPROOT"; }
trap cleanup EXIT

pass() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; echo "  detail: $2"; FAIL=$((FAIL+1)); }

echo "=== OSH-7 return smoke ($PETRUSH) ==="

# --- 1) return N: status da chamada = N ---
script1="$TMPROOT/ret-n.sh"
printf '%s\n' 'f() { return 3; }; f' > "$script1"
st1=0
"$PETRUSH" "$script1" >/dev/null 2>&1 || st1=$?
if [[ "$st1" -eq 3 ]]; then
    pass "return-n-status"
else
    fail "return-n-status" "st=$st1 expected 3"
fi

# --- 2) return sem arg: default 0 (documentado; nao usa ultimo cmd) ---
script2="$TMPROOT/ret-default.sh"
printf '%s\n' 'f() { false; return; }; f' > "$script2"
st2=0
"$PETRUSH" "$script2" >/dev/null 2>&1 || st2=$?
if [[ "$st2" -eq 0 ]]; then
    pass "return-default-zero"
else
    fail "return-default-zero" "st=$st2 expected 0"
fi

# --- 3) return no meio do body: nao executa o resto ---
script3="$TMPROOT/ret-mid.sh"
printf '%s\n' 'f() { echo before; return 0; echo after; }; f' > "$script3"
out3=$("$PETRUSH" "$script3" 2>&1) || true
if echo "$out3" | grep -Fq 'before' && ! echo "$out3" | grep -Fq 'after'; then
    pass "return-skips-rest"
else
    fail "return-skips-rest" "out=$out3"
fi

# --- 4) fora de funcao: status != 0 e NAO mata o script (diferente de exit) ---
script4="$TMPROOT/ret-outside.sh"
printf '%s\n' 'return 1; echo SURVIVED' > "$script4"
out4=$("$PETRUSH" "$script4" 2>&1) || true
if echo "$out4" | grep -Fq 'SURVIVED'; then
    pass "return-outside-continues"
else
    fail "return-outside-continues" "out=$out4 (exit matou o script?)"
fi

script4b="$TMPROOT/ret-outside-st.sh"
printf '%s\n' 'return' > "$script4b"
st4b=0
"$PETRUSH" "$script4b" >/dev/null 2>&1 || st4b=$?
if [[ "$st4b" -ne 0 ]]; then
    pass "return-outside-status"
else
    fail "return-outside-status" "st=0 (esperava !=0)"
fi

# --- 5) apos return, caller segue; status da fn propaga via || ---
script5="$TMPROOT/ret-or.sh"
printf '%s\n' 'f() { return 5; echo no; }; f || echo FAIL' > "$script5"
out5=$("$PETRUSH" "$script5" 2>&1) || true
if echo "$out5" | grep -Fq 'FAIL' && ! echo "$out5" | grep -Fq 'no'; then
    pass "return-status-or"
else
    fail "return-status-or" "out=$out5"
fi

# --- 6) return dentro de if no body da fn ---
script6="$TMPROOT/ret-if.sh"
printf '%s\n' 'f() { if true; then return 9; fi; echo no; }; f' > "$script6"
st6=0
out6=$("$PETRUSH" "$script6" 2>&1) || st6=$?
if [[ "$st6" -eq 9 ]] && ! echo "$out6" | grep -Fq 'no'; then
    pass "return-inside-if"
else
    fail "return-inside-if" "st=$st6 out=$out6"
fi

echo
echo "OSH-7 smoke: $PASS pass, $FAIL fail"
if [[ "$FAIL" -ne 0 ]]; then
    exit 1
fi
exit 0
