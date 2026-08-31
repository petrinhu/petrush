#!/bin/bash
# OSH-10: case word in pat) list ;; esac (;; only; | ; glob * ?; sem ;&)
# Usage: ./osh10-case.sh /path/to/petrush
# Expects exit 0 se todos passarem.

set -euo pipefail

PETRUSH="${1:-./build/petrush}"
PASS=0
FAIL=0
TMPROOT=$(mktemp -d /var/tmp/petrush-osh10-XXXXXX)
cleanup() { rm -rf "$TMPROOT"; }
trap cleanup EXIT

pass() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; echo "  detail: $2"; FAIL=$((FAIL+1)); }

echo "=== OSH-10 case smoke ($PETRUSH) ==="

# --- 1) match simples ---
script1="$TMPROOT/case-simple.sh"
printf '%s\n' 'case x in y) echo NO ;; x) echo YES ;; esac' > "$script1"
out1=$("$PETRUSH" "$script1" 2>&1) || true
if [[ "$out1" == "YES" ]]; then
    pass "case-simple-match"
else
    fail "case-simple-match" "out=$out1"
fi

# --- 2) alternacao pat1|pat2 ---
script2="$TMPROOT/case-or.sh"
printf '%s\n' 'case b in a|b|c) echo OR ;; *) echo NO ;; esac' > "$script2"
out2=$("$PETRUSH" "$script2" 2>&1) || true
if [[ "$out2" == "OR" ]]; then
    pass "case-or-patterns"
else
    fail "case-or-patterns" "out=$out2"
fi

# --- 3) glob * ---
script3="$TMPROOT/case-star.sh"
printf '%s\n' 'case foo in bar) echo NO ;; f*) echo STAR ;; esac' > "$script3"
out3=$("$PETRUSH" "$script3" 2>&1) || true
if [[ "$out3" == "STAR" ]]; then
    pass "case-glob-star"
else
    fail "case-glob-star" "out=$out3"
fi

# --- 4) nenhum match → status 0, body default so se * ---
script4="$TMPROOT/case-nomatch.sh"
printf '%s\n' 'case z in a) echo NO ;; b) echo NO ;; esac' 'echo AFTER' > "$script4"
out4=$("$PETRUSH" "$script4" 2>&1) || true
st4=0
"$PETRUSH" "$script4" >/dev/null 2>&1 || st4=$?
if echo "$out4" | grep -Fq 'NO'; then
    fail "case-no-match" "body rodou: $out4"
elif ! echo "$out4" | grep -Fq 'AFTER'; then
    fail "case-no-match" "sem AFTER: $out4"
elif [[ "$st4" -ne 0 ]]; then
    fail "case-no-match" "status=$st4 (esperado 0)"
else
    pass "case-no-match"
fi

# --- 5) "esac" quoted nao e keyword ---
script5="$TMPROOT/case-quoted-esac.sh"
printf '%s\n' 'case x in x) echo "esac" ;; esac' > "$script5"
out5=$("$PETRUSH" "$script5" 2>/dev/null) || true
if [[ "$out5" == "esac" ]]; then
    pass "case-esac-quoted"
else
    fail "case-esac-quoted" "out=$out5"
fi

# --- 6) status = ultimo cmd do braco ---
script6="$TMPROOT/case-status.sh"
printf '%s\n' 'case x in x) true; false ;; esac' > "$script6"
st6=0
"$PETRUSH" "$script6" >/dev/null 2>&1 || st6=$?
if [[ "$st6" -ne 0 ]]; then
    pass "case-arm-status"
else
    fail "case-arm-status" "status=$st6 (esperado !=0 de false)"
fi

echo
echo "OSH-10 smoke: $PASS pass, $FAIL fail"
if [[ "$FAIL" -ne 0 ]]; then
    exit 1
fi
exit 0
