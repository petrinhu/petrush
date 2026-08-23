#!/bin/bash
# OSH-5: for name in words; do list; done (setenv; sem for ((; in obrigatorio)
# Usage: ./osh5-for.sh /path/to/petrush
# Expects exit 0 se todos passarem.

set -euo pipefail

PETRUSH="${1:-./build/petrush}"
PASS=0
FAIL=0
TMPROOT=$(mktemp -d /var/tmp/petrush-osh5-XXXXXX)
cleanup() { rm -rf "$TMPROOT"; }
trap cleanup EXIT

pass() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; echo "  detail: $2"; FAIL=$((FAIL+1)); }

echo "=== OSH-5 for smoke ($PETRUSH) ==="

# --- 1) for i in a b c; do echo $i; done ---
script1="$TMPROOT/for-basic.sh"
printf '%s\n' 'for i in a b c; do echo $i; done' > "$script1"
out1=$("$PETRUSH" "$script1" 2>&1) || true
if [[ "$out1" == $'a\nb\nc' ]]; then
    pass "for-basic-abc"
else
    fail "for-basic-abc" "out=$out1"
fi

# --- 2) variavel no env (setenv): valor sobrevive apos o loop ---
script2="$TMPROOT/for-env.sh"
printf '%s\n' 'for i in last; do echo in; done' 'echo $i' > "$script2"
out2=$("$PETRUSH" "$script2" 2>&1) || true
if echo "$out2" | grep -Fq 'in' && echo "$out2" | grep -Fq 'last'; then
    pass "for-env-setenv"
else
    fail "for-env-setenv" "out=$out2"
fi

# --- 3) lista vazia: body nao roda ---
script3="$TMPROOT/for-empty.sh"
printf '%s\n' 'for i in; do echo NO; done' 'echo AFTER' > "$script3"
out3=$("$PETRUSH" "$script3" 2>&1) || true
if echo "$out3" | grep -Fq 'NO'; then
    fail "for-empty-skip" "body rodou: $out3"
elif ! echo "$out3" | grep -Fq 'AFTER'; then
    fail "for-empty-skip" "sem AFTER: $out3"
else
    pass "for-empty-skip"
fi

# --- 4) done quoted nao e keyword ---
script4="$TMPROOT/for-quoted-done.sh"
printf '%s\n' 'for i in x; do echo "done"; done' > "$script4"
out4=$("$PETRUSH" "$script4" 2>/dev/null) || true
if [[ "$out4" == "done" ]]; then
    pass "for-done-quoted"
else
    fail "for-done-quoted" "out=$out4"
fi

# --- 5) for apos ; ---
script5="$TMPROOT/for-seq.sh"
printf '%s\n' 'echo A; for i in B; do echo $i; done; echo C' > "$script5"
out5=$("$PETRUSH" "$script5" 2>&1) || true
if echo "$out5" | grep -Fq 'A' \
   && echo "$out5" | grep -Fq 'B' \
   && echo "$out5" | grep -Fq 'C'; then
    pass "for-after-seq"
else
    fail "for-after-seq" "out=$out5"
fi

# --- 6) in obrigatorio: for i; do deve falhar (nao executa body) ---
# stdout only: stderr ecoa a linha fonte (contem NOIN) no erro de parse
script6="$TMPROOT/for-no-in.sh"
printf '%s\n' 'for i; do echo NOIN; done' 'echo AFTER6' > "$script6"
out6=$("$PETRUSH" "$script6" 2>/dev/null) || true
st6=0
"$PETRUSH" "$script6" >/dev/null 2>&1 || st6=$?
if printf '%s\n' "$out6" | grep -qx 'NOIN'; then
    fail "for-requires-in" "body rodou sem in: $out6"
elif echo "$out6" | grep -Fq 'AFTER6'; then
    pass "for-requires-in"
elif [[ "$st6" -ne 0 ]]; then
    pass "for-requires-in"
else
    fail "for-requires-in" "st=$st6 out=$out6"
fi

echo
echo "OSH-5 smoke: $PASS pass, $FAIL fail"
if [[ "$FAIL" -ne 0 ]]; then
    exit 1
fi
exit 0
