#!/bin/bash
# OSH-3: if/then/else/elif/fi (status do ultimo comando; sem [[)
# Usage: ./osh3-if.sh /path/to/petrush
# Expects exit 0 se todos passarem.

set -euo pipefail

PETRUSH="${1:-./build/petrush}"
PASS=0
FAIL=0
TMPROOT=$(mktemp -d /var/tmp/petrush-osh3-XXXXXX)
cleanup() { rm -rf "$TMPROOT"; }
trap cleanup EXIT

pass() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; echo "  detail: $2"; FAIL=$((FAIL+1)); }

echo "=== OSH-3 if smoke ($PETRUSH) ==="

# --- 1) if true builtin; then body ---
script1="$TMPROOT/if-true.sh"
printf '%s\n' 'if true; then echo OK; fi' > "$script1"
out1=$("$PETRUSH" "$script1" 2>&1) || true
if echo "$out1" | grep -Fq 'OK'; then
    pass "if-true-then"
else
    fail "if-true-then" "out=$out1"
fi

# --- 2) if false; then nao roda ---
script2="$TMPROOT/if-false.sh"
printf '%s\n' 'if false; then echo NO; fi' 'echo AFTER' > "$script2"
out2=$("$PETRUSH" "$script2" 2>&1) || true
if echo "$out2" | grep -Fq 'NO'; then
    fail "if-false-skip" "then rodou: $out2"
elif ! echo "$out2" | grep -Fq 'AFTER'; then
    fail "if-false-skip" "sem AFTER: $out2"
else
    pass "if-false-skip"
fi

# --- 3) else ---
script3="$TMPROOT/if-else.sh"
printf '%s\n' 'if false; then echo A; else echo B; fi' > "$script3"
out3=$("$PETRUSH" "$script3" 2>&1) || true
if echo "$out3" | grep -Fq 'B' && ! echo "$out3" | grep -Fq 'A'; then
    pass "if-else"
else
    fail "if-else" "out=$out3"
fi

# --- 4) elif ---
script4="$TMPROOT/if-elif.sh"
printf '%s\n' \
  'if false; then echo A; elif true; then echo B; else echo C; fi' \
  > "$script4"
out4=$("$PETRUSH" "$script4" 2>&1) || true
if echo "$out4" | grep -Fq 'B' \
   && ! echo "$out4" | grep -Fq 'A' \
   && ! echo "$out4" | grep -Fq 'C'; then
    pass "if-elif"
else
    fail "if-elif" "out=$out4"
fi

# --- 5) /bin/true e /bin/false como condicao ---
script5="$TMPROOT/if-bin.sh"
printf '%s\n' \
  'if /bin/false; then echo A; elif /bin/true; then echo BIN; fi' \
  > "$script5"
out5=$("$PETRUSH" "$script5" 2>&1) || true
if echo "$out5" | grep -Fq 'BIN'; then
    pass "if-bin-true-false"
else
    fail "if-bin-true-false" "out=$out5"
fi

# --- 6) fi quoted nao e keyword ---
script6="$TMPROOT/if-quoted-fi.sh"
printf '%s\n' 'if true; then echo "fi"; fi' > "$script6"
out6=$("$PETRUSH" "$script6" 2>&1) || true
if [[ "$out6" == "fi" ]]; then
    pass "if-fi-quoted"
else
    fail "if-fi-quoted" "out=$out6"
fi

echo
echo "OSH-3 smoke: $PASS pass, $FAIL fail"
if [[ "$FAIL" -ne 0 ]]; then
    exit 1
fi
exit 0
