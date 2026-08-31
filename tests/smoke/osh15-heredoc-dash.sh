#!/bin/bash
# OSH-15: <<- strip de tabs no corpo e na linha do delim (espaco nao e tab)
# Usage: ./osh15-heredoc-dash.sh /path/to/petrush
# Expects exit 0 se todos passarem.

set -euo pipefail

PETRUSH="${1:-./build/petrush}"
PASS=0
FAIL=0
TMPROOT=$(mktemp -d /var/tmp/petrush-osh15-XXXXXX)
cleanup() { rm -rf "$TMPROOT"; }
trap cleanup EXIT

pass() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; echo "  detail: $2"; FAIL=$((FAIL+1)); }

echo "=== OSH-15 <<- heredoc dash strip smoke ($PETRUSH) ==="

# --- 1) strip tabs no corpo; delim indentado com tab fecha ---
script1="$TMPROOT/strip.sh"
{
    echo "cat <<-'EOF'"
    printf '\thello\n'
    printf '\tworld\n'
    printf '\tEOF\n'
} > "$script1"
out1=$("$PETRUSH" "$script1" 2>&1) || true
if [ "$out1" = $'hello\nworld' ]; then
    pass "strip-tabs-body-and-delim"
else
    fail "strip-tabs-body-and-delim" "out=[$out1]"
fi

# --- 2) espaco a esquerda NAO e tab (preserva) ---
script2="$TMPROOT/space.sh"
{
    echo "cat <<-'EOF'"
    printf ' hello\n'
    echo "EOF"
} > "$script2"
out2=$("$PETRUSH" "$script2" 2>&1) || true
if [ "$out2" = ' hello' ]; then
    pass "space-not-stripped"
else
    fail "space-not-stripped" "out=[$out2]"
fi

# --- 3) << sem dash preserva tab no corpo ---
script3="$TMPROOT/nodash.sh"
{
    echo "cat <<'EOF'"
    printf '\ttabbed\n'
    echo "EOF"
} > "$script3"
out3=$("$PETRUSH" "$script3" 2>&1) || true
if [ "$out3" = $'\ttabbed' ]; then
    pass "nodash-preserves-tab"
else
    fail "nodash-preserves-tab" "out=[$out3]"
fi

# --- 4) ' EOF' (espaco) NAO fecha <<- ; sem delim real → erro ---
script4="$TMPROOT/spacedelim.sh"
{
    echo "cat <<-'EOF'"
    echo "body"
    printf ' EOF\n'
} > "$script4"
rc4=0
out4=$("$PETRUSH" "$script4" 2>&1) || rc4=$?
if [ "$rc4" -ne 0 ]; then
    pass "space-delim-does-not-close"
else
    fail "space-delim-does-not-close" "rc=0 out=[$out4]"
fi

# --- 5) quoted <<- 'E' strip + literal (sem expand) ---
script5="$TMPROOT/quoted.sh"
{
    echo 'export OSH15_V=hello'
    echo "cat <<-'EOF'"
    printf '\tval=$OSH15_V\n'
    printf '\tEOF\n'
} > "$script5"
out5=$("$PETRUSH" "$script5" 2>&1) || true
if [ "$out5" = 'val=$OSH15_V' ]; then
    pass "quoted-dash-literal"
else
    fail "quoted-dash-literal" "out=[$out5]"
fi

# --- 6) unquoted <<-EOF strip + expand $VAR ---
script6="$TMPROOT/unquoted.sh"
{
    echo 'export OSH15_V=hi'
    echo 'cat <<-EOF'
    printf '\tval=$OSH15_V\n'
    printf '\tEOF\n'
} > "$script6"
out6=$("$PETRUSH" "$script6" 2>&1) || true
if [ "$out6" = 'val=hi' ]; then
    pass "unquoted-dash-expand"
else
    fail "unquoted-dash-expand" "out=[$out6]"
fi

# --- 7) regressao OSH-13: quoted <<'E' sem strip ---
script7="$TMPROOT/osh13reg.sh"
cat > "$script7" <<'SCRIPT'
cat <<'E'
line-a
line-b
E
SCRIPT
out7=$("$PETRUSH" "$script7" 2>&1) || true
if [ "$out7" = $'line-a\nline-b' ]; then
    pass "osh13-quoted-regression"
else
    fail "osh13-quoted-regression" "out=[$out7]"
fi

# --- 8) regressao OSH-14: unquoted <<EOF expand ---
script8="$TMPROOT/osh14reg.sh"
cat > "$script8" <<'SCRIPT'
export OSH15_R=ok
cat <<EOF
x=$OSH15_R
EOF
SCRIPT
out8=$("$PETRUSH" "$script8" 2>&1) || true
if [ "$out8" = 'x=ok' ]; then
    pass "osh14-expand-regression"
else
    fail "osh14-expand-regression" "out=[$out8]"
fi

echo "=== OSH-15: $PASS passed, $FAIL failed ==="
if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
exit 0
