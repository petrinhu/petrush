#!/bin/bash
# OSH-0: petrush arquivo + shebang (sem REPL/banner/rc; exit = ultimo comando)
# Usage: ./osh0-script.sh /path/to/petrush
# Expects exit 0 se todos passarem.

set -euo pipefail

PETRUSH="${1:-./build/petrush}"
PASS=0
FAIL=0
TMPROOT=$(mktemp -d /var/tmp/petrush-osh0-XXXXXX)
cleanup() { rm -rf "$TMPROOT"; }
trap cleanup EXIT

pass() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; echo "  detail: $2"; FAIL=$((FAIL+1)); }

echo "=== OSH-0 smoke ($PETRUSH) ==="

# --- 1) script mode: sem banner / sem linenoise prompt ---
script1="$TMPROOT/ok.sh"
printf 'echo osh0-hello\n' > "$script1"
chmod 0644 "$script1"
out1=$("$PETRUSH" "$script1" 2>&1) || true
ec1=0
"$PETRUSH" "$script1" >/dev/null 2>&1 || ec1=$?
if echo "$out1" | grep -Eq 'C23 shell|Digite |petrush>'; then
    fail "no-banner" "banner/REPL vazou: $out1"
elif ! echo "$out1" | grep -Fq 'osh0-hello'; then
    fail "no-banner" "faltou output do script: $out1"
elif [[ "$ec1" -ne 0 ]]; then
    fail "no-banner" "exit=$ec1 esperado 0"
else
    pass "no-banner-no-repl"
fi

# --- 2) exit status = ultimo comando ---
script2="$TMPROOT/last.sh"
printf '/bin/false\n' > "$script2"
ec2=0
"$PETRUSH" "$script2" >/dev/null 2>&1 || ec2=$?
if [[ "$ec2" -eq 1 ]]; then
    pass "exit-last-command"
else
    fail "exit-last-command" "exit=$ec2 esperado 1"
fi

# --- 3) arquivo ausente → 127 ---
ec3=0
"$PETRUSH" "$TMPROOT/no-such-osh0-zzz" >/dev/null 2>&1 || ec3=$?
if [[ "$ec3" -eq 127 ]]; then
    pass "missing-127"
else
    fail "missing-127" "exit=$ec3 esperado 127"
fi

# --- 4) exit N no script encerra N ---
script4="$TMPROOT/exitn.sh"
printf 'echo before\nexit 42\necho after\n' > "$script4"
out4=$("$PETRUSH" "$script4" 2>&1) || true
ec4=0
"$PETRUSH" "$script4" >/dev/null 2>&1 || ec4=$?
if [[ "$ec4" -eq 42 ]] && echo "$out4" | grep -Fq 'before' && ! echo "$out4" | grep -Fq 'after'; then
    pass "exit-N"
else
    fail "exit-N" "exit=$ec4 out=$out4"
fi

# --- 5) nao carrega ~/.petrushrc ---
home5="$TMPROOT/home"
mkdir -p "$home5"
printf 'echo RC_LOADED_OSH0\n' > "$home5/.petrushrc"
chmod 0600 "$home5/.petrushrc"
script5="$TMPROOT/norc.sh"
printf 'echo script-body\n' > "$script5"
out5=$(HOME="$home5" "$PETRUSH" "$script5" 2>&1) || true
if echo "$out5" | grep -Fq 'RC_LOADED_OSH0'; then
    fail "no-rc" "rc vazou: $out5"
elif ! echo "$out5" | grep -Fq 'script-body'; then
    fail "no-rc" "script nao rodou: $out5"
else
    pass "no-rc"
fi

# --- 6) group-writable em /var/tmp OK (sem SEC-10 mode&0022 no argv) ---
script6="$TMPROOT/gw.sh"
printf 'echo gw-ok\n' > "$script6"
chmod 0664 "$script6"
out6=$("$PETRUSH" "$script6" 2>&1) || true
ec6=0
"$PETRUSH" "$script6" >/dev/null 2>&1 || ec6=$?
if [[ "$ec6" -eq 0 ]] && echo "$out6" | grep -Fq 'gw-ok'; then
    pass "group-writable-ok"
else
    fail "group-writable-ok" "exit=$ec6 out=$out6"
fi

# --- 7) recusa nao-regular (diretorio) ---
ec7=0
"$PETRUSH" "$TMPROOT" >/dev/null 2>&1 || ec7=$?
if [[ "$ec7" -ne 0 && "$ec7" -ne 127 ]]; then
    pass "refuse-directory"
else
    fail "refuse-directory" "exit=$ec7 (esperado !=0 e !=127)"
fi

# --- 8) shebang #!/usr/bin/env petrush ---
bindir=$(cd "$(dirname "$PETRUSH")" && pwd)
base=$(basename "$PETRUSH")
script8="$TMPROOT/shebang.sh"
printf '#!/usr/bin/env %s\necho shebang-ok\n' "$base" > "$script8"
chmod 0755 "$script8"
out8=$(PATH="$bindir:$PATH" "$script8" 2>&1) || true
ec8=0
PATH="$bindir:$PATH" "$script8" >/dev/null 2>&1 || ec8=$?
if [[ "$ec8" -eq 0 ]] && echo "$out8" | grep -Fq 'shebang-ok' && ! echo "$out8" | grep -Eq 'C23 shell|Digite '; then
    pass "shebang-env"
else
    fail "shebang-env" "exit=$ec8 out=$out8"
fi

# --- 9) interativo sem args ainda mostra banner (regressao) ---
out9=$(printf 'exit\n' | "$PETRUSH" 2>&1) || true
if echo "$out9" | grep -Eq 'C23 shell'; then
    pass "interactive-banner"
else
    fail "interactive-banner" "banner sumiu: $out9"
fi

echo
echo "OSH-0 smoke: $PASS pass, $FAIL fail"
if [[ "$FAIL" -ne 0 ]]; then
    exit 1
fi
exit 0
