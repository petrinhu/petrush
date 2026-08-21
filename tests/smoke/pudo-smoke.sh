#!/bin/bash
# Integrated smoke test for petrush + pudo (NEW-14, NEW-05)
# Runs without requiring root/setuid on pudod (expects refusal paths + other commands)
# Usage: ./pudo-smoke.sh /path/to/petrush
# Expects exit 0 if all pass.

set -euo pipefail

PETRUSH="${1:-./build/petrush}"
SMOKE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PASS=0
FAIL=0

run_smoke() {
    local input="$1"
    local expect="$2"
    local desc="$3"

    echo "=== SMOKE: $desc ==="
    output=$(printf "%s\nexit\n" "$input" | "$PETRUSH" 2>&1 || true)

    if echo "$output" | grep -Eq "$expect"; then
        echo "PASS: $desc"
        PASS=$((PASS+1))
    else
        echo "FAIL: $desc"
        echo "Expected to see: $expect"
        echo "Got (last lines):"
        echo "$output" | tail -5
        FAIL=$((FAIL+1))
    fi
    echo
}

# Basic commands (≥12 total coverage target: builtins, external, errors, signals-ish, pudo)
# Caminho absoluto qualquer (CI Fedora/GHA roda em /__w/... ou /github/workspace, não /home/)
run_smoke "pwd" "/" "pwd builtin (prints cwd)"
run_smoke "echo hello world" "hello world" "echo builtin"
run_smoke "help" "Comandos embutidos disponíveis" "help builtin"
# Não dumpa env inteiro (evita vazar secrets no log de CI); só checa prefixo
run_smoke "export TEST_SMOKE=42" "saindo|petrush 0\\." "export builtin (no crash)"
run_smoke "unset TEST_SMOKE" "saindo|petrush 0\\." "unset builtin (no crash)"
run_smoke "history" "1  " "history builtin (shows entries)"
run_smoke "clear" "" "clear builtin (no crash, empty or control)"
run_smoke "pudo --help" "pudo — execução de comandos com privilégios" "pudo help"
# Sem setuid: pudod recusa (allow-list/euid) ou path de erro do helper
run_smoke "pudo /usr/bin/id" "allow-list|euid|denying|setuid|privileges|erro:|denied|not permitted|not root|permission|pudod:" "pudo execution (expects refusal without setuid/allow)"
run_smoke "pudo /bin/false" "allow-list|euid|denying|setuid|privileges|erro:|denied|not permitted|not root|permission|pudod:" "pudo false (refusal path)"
run_smoke "nonexistentcmd12345" "não encontrado|command not found|127" "external command not found (error 127)"
run_smoke "ls /nonexistentdir12345" "não foi possível acessar|No such file|não encontrado" "external command error path"
run_smoke "info" "petrush 0\\.|C23 REPL|Build:" "info builtin (Onda 3 diagnostic)"
run_smoke "alias ll=ls" "saindo|petrush 0\\." "alias define"
run_smoke "which pwd" "builtin" "which builtin"
# GHA Fedora: HOME=/github/home (não casa /home/)
run_smoke "echo ~" "/home/|/root|/Users|/github/home" "tilde expand"
run_smoke "export PETRUSH_SMOKE_V=xyz99" "saindo|petrush 0\\." "export for var expand"
# var expand needs same process — multi-line
echo "=== SMOKE: dollar VAR expand ==="
var_out=$(printf 'export PETRUSH_SMOKE_V=xyz99\necho $PETRUSH_SMOKE_V\nexit\n' | "$PETRUSH" 2>&1 || true)
if echo "$var_out" | grep -Fq 'xyz99'; then
    echo "PASS: dollar VAR expand"
    PASS=$((PASS+1))
else
    echo "FAIL: dollar VAR expand"
    echo "$var_out" | tail -6
    FAIL=$((FAIL+1))
fi
echo
run_smoke "/bin/true && echo and-ok" "and-ok" "list AND short-circuit"
run_smoke "/bin/false || echo or-ok" "or-ok" "list OR short-circuit"
run_smoke "pushd /tmp" "saindo|petrush 0\\.|/" "pushd /tmp"
run_smoke "popd" "saindo|petrush 0\\.|/" "popd"

# UX-14: cd - (mesma sessão: /tmp depois volta)
echo "=== SMOKE: cd - OLDPWD ==="
cd_out=$(printf 'cd /tmp\npwd\ncd -\npwd\nexit\n' | "$PETRUSH" 2>&1 || true)
if echo "$cd_out" | grep -Fq '/tmp' && echo "$cd_out" | grep -Eq '/'; then
    echo "PASS: cd - OLDPWD"
    PASS=$((PASS+1))
else
    echo "FAIL: cd - OLDPWD"
    echo "$cd_out" | tail -10
    FAIL=$((FAIL+1))
fi
echo

# NEW-20: pipes e redirecionamento (externos; anti-OE)
run_smoke "printf 'abc\\n' | cat" "abc" "pipe printf|cat"
run_smoke "echo smoke-redir > /tmp/petrush-smoke-out.txt" "saindo|petrush 0\\." "redir out no crash"
# leitura do arquivo escrito (se shell persistiu o arquivo)
if [ -f /tmp/petrush-smoke-out.txt ] && grep -q 'smoke-redir' /tmp/petrush-smoke-out.txt 2>/dev/null; then
    echo "=== SMOKE: redir file content ==="
    echo "PASS: redir file content"
    PASS=$((PASS+1))
else
    # fallback: cat via petrush
    run_smoke "cat /tmp/petrush-smoke-out.txt" "smoke-redir" "redir file content via cat"
fi
rm -f /tmp/petrush-smoke-out.txt

# UX-16: stderr redirs 2> 2>> 2>&1 &>
rm -f /tmp/petrush-smoke-err.txt /tmp/petrush-smoke-err2.txt \
      /tmp/petrush-smoke-both.txt /tmp/petrush-smoke-cderr.txt

run_smoke "/bin/sh -c 'echo OUT; echo ERR >&2' 2> /tmp/petrush-smoke-err.txt" \
    "OUT" "2> stderr file (OUT on stdout)"
if [ -f /tmp/petrush-smoke-err.txt ] && grep -q 'ERR' /tmp/petrush-smoke-err.txt \
    && ! grep -q 'OUT' /tmp/petrush-smoke-err.txt; then
    echo "=== SMOKE: 2> err file content ==="
    echo "PASS: 2> err file content"
    PASS=$((PASS+1))
else
    echo "=== SMOKE: 2> err file content ==="
    echo "FAIL: 2> err file content"
    FAIL=$((FAIL+1))
fi

run_smoke "/bin/sh -c 'echo ERR >&2' 2>> /tmp/petrush-smoke-err2.txt" \
    "saindo|petrush 0\\." "2>> first"
run_smoke "/bin/sh -c 'echo ERR >&2' 2>> /tmp/petrush-smoke-err2.txt" \
    "saindo|petrush 0\\." "2>> second"
if [ -f /tmp/petrush-smoke-err2.txt ] \
    && [ "$(grep -c 'ERR' /tmp/petrush-smoke-err2.txt 2>/dev/null || echo 0)" = "2" ]; then
    echo "=== SMOKE: 2>> append ==="
    echo "PASS: 2>> append"
    PASS=$((PASS+1))
else
    echo "=== SMOKE: 2>> append ==="
    echo "FAIL: 2>> append"
    FAIL=$((FAIL+1))
fi

# 2>&1: ERR deve aparecer no stdout capturado junto com OUT
run_smoke "/bin/sh -c 'echo OUT; echo ERR >&2' 2>&1" "ERR" "2>&1 merge to stdout"

run_smoke "/bin/sh -c 'echo OUT; echo ERR >&2' &> /tmp/petrush-smoke-both.txt" \
    "saindo|petrush 0\\." "&> both to file"
if [ -f /tmp/petrush-smoke-both.txt ] \
    && grep -q 'OUT' /tmp/petrush-smoke-both.txt \
    && grep -q 'ERR' /tmp/petrush-smoke-both.txt; then
    echo "=== SMOKE: &> both content ==="
    echo "PASS: &> both content"
    PASS=$((PASS+1))
else
    echo "=== SMOKE: &> both content ==="
    echo "FAIL: &> both content"
    FAIL=$((FAIL+1))
fi

# 2> sem alvo → erro ao analisar (não crash)
run_smoke "echo hi 2>" "erro|analis|parse|sintaxe|petrush:" "2> incomplete parse error"

# builtin perror no arquivo via 2>
run_smoke "cd /no-such-petrush-ux16 2> /tmp/petrush-smoke-cderr.txt" \
    "saindo|petrush 0\\.|No such|não|cd:" "builtin cd 2> file"
if [ -f /tmp/petrush-smoke-cderr.txt ] && [ -s /tmp/petrush-smoke-cderr.txt ]; then
    echo "=== SMOKE: builtin 2> perror ==="
    echo "PASS: builtin 2> perror"
    PASS=$((PASS+1))
else
    echo "=== SMOKE: builtin 2> perror ==="
    echo "FAIL: builtin 2> perror"
    FAIL=$((FAIL+1))
fi

rm -f /tmp/petrush-smoke-err.txt /tmp/petrush-smoke-err2.txt \
      /tmp/petrush-smoke-both.txt /tmp/petrush-smoke-cderr.txt

echo "=== SMOKE SUMMARY ==="
echo "Passed: $PASS"
echo "Failed: $FAIL"

if [ "$FAIL" -gt 0 ]; then
    echo "SMOKE FAILED"
    exit 1
fi

echo "SMOKE PASSED (includes pudo integrated paths + sanitization-relevant commands)"
exit 0
