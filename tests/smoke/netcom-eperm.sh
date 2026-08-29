#!/usr/bin/env bash
# ASM-NET: netcom -up sem CAP_NET_ADMIN → EPERM claro, exit 1, sem hang.
# Preferencia: unshare user+net + capsh --drop=cap_net_admin (CI-NETCOM-UNSHARE).
# Se unshare falhar com EPERM/not permitted (GHA container): fallback petrush direto.
# Se o fallback tambem nao expor EPERM (runner com CAP): skip ctest (exit 77).
# Hang (timeout 124) continua FAIL. Sem root. Sem 4755.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-$ROOT/build}"
PETRUSH="${BUILD}/petrush"

fail() { echo "FAIL: $*" >&2; exit 1; }
skip() { echo "SKIP: $*" >&2; exit 77; }

[[ -x "$PETRUSH" ]] || fail "petrush missing: $PETRUSH (pass build dir as \$1)"

WORKDIR="${TMPDIR:-/var/tmp}/petrush-netcom-eperm-$$"
mkdir -p "$WORKDIR"
cleanup() { rm -rf "$WORKDIR"; }
trap cleanup EXIT

SCRIPT="$WORKDIR/up.petrush"
printf 'netcom -up lo\n' > "$SCRIPT"
chmod 0644 "$SCRIPT"

echo "=== ASM-NET smoke: netcom -up without CAP_NET_ADMIN (netns) ==="

unshare_denied() {
  # util-linux: "unshare: unshare failed: Operation not permitted"
  echo "$1" | grep -Eqi 'unshare:.*(Operation not permitted|not permitted|EPERM)'
}

has_eperm_msg() {
  echo "$1" | grep -Eqi 'EPERM|CAP_NET_ADMIN'
}

run_via_unshare() {
  timeout 5s unshare --user --map-root-user --net \
    capsh --drop=cap_net_admin -- -c \
    "timeout 3s '$PETRUSH' '$SCRIPT'" 2>&1
}

run_bare() {
  timeout 3s "$PETRUSH" "$SCRIPT" 2>&1
}

# Prefer unshare user+net + capsh drop. Fallback: bare (host CapEff=0).
# CI-NETCOM-UNSHARE: GHA container sem userns → unshare EPERM → bare ou skip 77.
run_eperm() {
  local out rc mode="bare"

  if command -v unshare >/dev/null 2>&1 && command -v capsh >/dev/null 2>&1; then
    set +e
    out="$(run_via_unshare)"
    rc=$?
    set -e
    if unshare_denied "$out"; then
      echo "NOTE: CI-NETCOM-UNSHARE: unshare EPERM/not permitted; fallback bare petrush" >&2
      set +e
      out="$(run_bare)"
      rc=$?
      set -e
      mode="bare-fallback"
    else
      mode="unshare"
    fi
  else
    set +e
    out="$(run_bare)"
    rc=$?
    set -e
  fi

  printf '%s\n' "$out"

  # timeout(1) → 124; must not happen (hang)
  if [[ "$rc" -eq 124 ]]; then
    fail "netcom -up hung (timeout); expected fast EPERM"
  fi

  # Runner privilegiado com CAP_NET_ADMIN: bare nao observa EPERM → skip (nao FAIL)
  if [[ "$mode" != "unshare" ]] && ! has_eperm_msg "$out"; then
    skip "CI-NETCOM-UNSHARE: no EPERM on bare runner (likely CAP_NET_ADMIN present)"
  fi

  # petrush script exit propagates; expect 1
  if [[ "$rc" -ne 1 ]]; then
    fail "expected exit 1, got $rc; out=$out"
  fi
  has_eperm_msg "$out" \
    || fail "stderr/out missing EPERM/CAP_NET_ADMIN; out=$out"
}

run_eperm
echo "OK: ASM-NET EPERM sem CAP (mode ok), exit 1, sem hang"
