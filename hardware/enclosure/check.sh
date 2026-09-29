#!/usr/bin/env bash
# Automated enclosure checks. Exit 0 only if every check passes.
set -uo pipefail
cd "$(dirname "$0")"
SCAD=tools/openscad.sh
OUT=build/check
mkdir -p "$OUT"
fail=0

# render PART -> sets $log and $rc
render() {
  log=$("$SCAD" -D "part=\"$1\"" -o "$OUT/$1.stl" enclosure.scad 2>&1); rc=$?
}

expect_solid() {  # part must render to a non-empty mesh with no warnings
  render "$1"
  if [[ $rc -ne 0 ]] || grep -qE "WARNING|ERROR" <<<"$log"; then
    echo "FAIL $1: render failed or warned"; echo "$log" | sed 's/^/    /'; fail=1
  else echo "ok   $1 renders cleanly"; fi
}

expect_empty() {  # part must have no solid volume (touching faces are fine)
  render "$1"
  if grep -q "Current top level object is empty" <<<"$log"; then
    echo "ok   $1 is empty"
  elif [[ $rc -ne 0 ]]; then
    echo "FAIL $1: render failed"; echo "$log" | sed 's/^/    /'; fail=1
  elif report=$(python3 tools/stl_report.py "$OUT/$1.stl"); then
    echo "ok   $1 touches only ($report)"
  else
    echo "FAIL $1: solid overlap"; echo "$report" | sed 's/^/    /'; fail=1
  fi
}

expect_nonempty() {  # part must have solid volume
  render "$1"
  if [[ $rc -eq 0 ]] && ! python3 tools/stl_report.py "$OUT/$1.stl" >/dev/null; then
    echo "ok   $1 has volume"
  else echo "FAIL $1: expected solid geometry"; fail=1; fi
}

expect_nonempty clash_selftest
expect_nonempty usb_exits_right
expect_nonempty rj45_exits_top

[[ $fail -eq 0 ]] && echo "ALL CHECKS PASSED" || echo "CHECKS FAILED"
exit $fail
