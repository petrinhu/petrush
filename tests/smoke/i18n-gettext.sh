#!/usr/bin/env bash
# I18N-GETTEXT: po/ (en, pt_BR, es_419), msgfmt, bindtextdomain smoke.
# msgid = English. ASM must not call gettext. Sem 4755.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WORKDIR="${TMPDIR:-/var/tmp}/petrush-i18n-$$"
mkdir -p "$WORKDIR"
cleanup() { rm -rf "$WORKDIR"; }
trap cleanup EXIT

fail() { echo "FAIL: $*" >&2; exit 1; }

echo "=== I18N-GETTEXT: catalogs po/ (msgid=en) ==="
for lang in en pt_BR es_419; do
  po="$ROOT/po/${lang}.po"
  [[ -f "$po" ]] || fail "missing $po"
  grep -q 'msgid "I18N-GETTEXT probe"' "$po" || fail "$po missing probe msgid"
done
# en: msgstr equals msgid for probe
en_msgstr="$(awk '/msgid "I18N-GETTEXT probe"/{getline; print}' "$ROOT/po/en.po")"
[[ "$en_msgstr" == 'msgstr "I18N-GETTEXT probe"' ]] || fail "en.po probe msgstr must equal msgid"
# pt_BR / es_419 must differ from msgid
pt_msgstr="$(awk '/msgid "I18N-GETTEXT probe"/{getline; print}' "$ROOT/po/pt_BR.po")"
[[ "$pt_msgstr" != 'msgstr "I18N-GETTEXT probe"' ]] || fail "pt_BR.po probe not translated"
es_msgstr="$(awk '/msgid "I18N-GETTEXT probe"/{getline; print}' "$ROOT/po/es_419.po")"
[[ "$es_msgstr" != 'msgstr "I18N-GETTEXT probe"' ]] || fail "es_419.po probe not translated"
echo "OK: po/en pt_BR es_419 present"

echo "=== I18N-GETTEXT: msgfmt compiles catalogs ==="
command -v msgfmt >/dev/null 2>&1 || fail "msgfmt not in PATH"
for lang in en pt_BR es_419; do
  mkdir -p "$WORKDIR/locale/${lang}/LC_MESSAGES"
  msgfmt -o "$WORKDIR/locale/${lang}/LC_MESSAGES/petrush.mo" "$ROOT/po/${lang}.po"
  [[ -s "$WORKDIR/locale/${lang}/LC_MESSAGES/petrush.mo" ]] || fail "empty mo for $lang"
done
echo "OK: msgfmt -> .mo"

echo "=== I18N-GETTEXT: cmake configure + msgfmt target + probe ==="
cmake -B "$WORKDIR/build" -S "$ROOT" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DPETRUSH_ASM=ON \
  -DENABLE_COVERAGE=OFF
# CMake must know msgfmt / i18n wiring (target i18n_gettext runs THIS script)
grep -E 'find_package\(Gettext|GETTEXT_MSGFMT|msgfmt|I18N-GETTEXT|petrush_i18n' \
  "$ROOT/CMakeLists.txt" >/dev/null \
  || fail "CMakeLists missing Gettext/msgfmt/i18n wiring"
# Build probe + mo only (never --target i18n_gettext here: that re-enters this script)
cmake --build "$WORKDIR/build" -j"${CMAKE_BUILD_PARALLEL_LEVEL:-2}" \
  --target i18n_probe petrush_mo
PROBE="$WORKDIR/build/i18n_probe"
[[ -x "$PROBE" ]] || fail "i18n_probe missing after build"
# mo must land under build localedir
for lang in en pt_BR es_419; do
  mo="$WORKDIR/build/locale/${lang}/LC_MESSAGES/petrush.mo"
  [[ -s "$mo" ]] || fail "CMake msgfmt did not produce $mo"
done
echo "OK: cmake msgfmt + i18n_probe"

echo "=== I18N-GETTEXT: bindtextdomain runtime (LANGUAGE=pt_BR / es_419) ==="
# gettext ignores LANGUAGE when LC_ALL/LANG is C/POSIX. Need a non-C locale
# as carrier; catalog selection still follows LANGUAGE (es_419 has no system locale).
export PETRUSH_LOCALEDIR="$WORKDIR/build/locale"
carrier=
for cand in pt_BR.UTF-8 pt_BR.utf8 en_US.UTF-8 en_US.utf8 C.UTF-8; do
  if LC_ALL="$cand" locale >/dev/null 2>&1; then
    carrier="$cand"
    break
  fi
done
[[ -n "$carrier" ]] || fail "no non-C locale available for gettext LANGUAGE"
echo "locale carrier: $carrier"

out_pt="$(LANGUAGE=pt_BR LC_ALL="$carrier" "$PROBE")"
echo "probe pt_BR: $out_pt"
echo "$out_pt" | grep -Fq 'I18N-GETTEXT sonda' \
  || fail "pt_BR bindtextdomain did not return translated probe"
out_es="$(LANGUAGE=es_419 LC_ALL="$carrier" "$PROBE")"
echo "probe es_419: $out_es"
echo "$out_es" | grep -Fq 'I18N-GETTEXT prueba' \
  || fail "es_419 bindtextdomain did not return translated probe (got: $out_es)"
out_en="$(LANGUAGE=en LC_ALL="$carrier" "$PROBE")"
echo "probe en: $out_en"
echo "$out_en" | grep -Fq 'I18N-GETTEXT probe' \
  || fail "en catalog must echo msgid"
echo "OK: bindtextdomain translations"

echo "=== I18N-GETTEXT: ASM nao chama gettext ==="
if grep -REn 'gettext|libintl|bindtextdomain|textdomain|dgettext|ngettext' \
  "$ROOT/src/asm" --include='*.S' --include='*.inc' >/dev/null 2>&1; then
  fail "ASM sources must not call gettext/libintl"
fi
echo "OK: ASM locale-agnostic"

echo "=== I18N-GETTEXT: sem mode 4755 nos binarios da fatia ==="
cmake --build "$WORKDIR/build" -j"${CMAKE_BUILD_PARALLEL_LEVEL:-2}" --target petrush
[[ -x "$WORKDIR/build/petrush" ]] || fail "petrush missing"
for bin in "$PROBE" "$WORKDIR/build/petrush"; do
  if [[ -u "$bin" ]]; then
    fail "setuid bit set on $bin"
  fi
  mode="$(stat -c '%a' "$bin")"
  if [[ "$mode" == "4755" ]]; then
    fail "mode 4755 on $bin"
  fi
done
echo "OK: no 4755/setuid"

echo "=== I18N-GETTEXT PASS ==="
