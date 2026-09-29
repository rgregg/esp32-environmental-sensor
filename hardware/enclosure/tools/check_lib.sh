# Check helpers, sourced by check.sh and tools/test_check_lib.sh.
# Run from hardware/enclosure/. Each helper sets fail=1 on failure.
SCAD=tools/openscad.sh
OUT=build/check
mkdir -p "$OUT"
fail=0

# render PART [EXTRA OPENSCAD ARGS...] -> sets $log, $rc and $stl
render() {
  local part=$1; shift
  stl="$OUT/$part$(printf '%s' "$*" | tr -c 'A-Za-z0-9_.=' '_').stl"
  log=$("$SCAD" -D "part=\"$part\"" "$@" -o "$stl" enclosure.scad 2>&1); rc=$?
}

expect_solid() {  # part must render to a non-empty mesh with no warnings
  render "$@"
  if [[ $rc -ne 0 ]] || grep -qE "WARNING|ERROR" <<<"$log"; then
    echo "FAIL $*: render failed or warned"; echo "$log" | sed 's/^/    /'; fail=1
  else echo "ok   $* renders cleanly"; fi
}

expect_empty() {  # part must have no solid volume (touching faces are fine)
  render "$@"
  # checked first: an ERROR still prints "object is empty", and a WARNING
  # usually means keep-out geometry was silently dropped
  if grep -qE "WARNING|ERROR" <<<"$log"; then
    echo "FAIL $*: render failed or warned"; echo "$log" | sed 's/^/    /'; fail=1
  elif grep -q "Current top level object is empty" <<<"$log"; then
    echo "ok   $* is empty"
  elif [[ $rc -ne 0 ]]; then
    echo "FAIL $*: render failed"; echo "$log" | sed 's/^/    /'; fail=1
  elif report=$(python3 tools/stl_report.py "$stl"); then
    echo "ok   $* touches only ($report)"
  else
    echo "FAIL $*: solid overlap"; echo "$report" | sed 's/^/    /'; fail=1
  fi
}

expect_nonempty() {  # part must have solid volume
  render "$@"
  if [[ $rc -eq 0 ]] && ! python3 tools/stl_report.py "$stl" >/dev/null; then
    echo "ok   $* has volume"
  else echo "FAIL $*: expected solid geometry"; fail=1; fi
}
