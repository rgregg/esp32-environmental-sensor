#!/usr/bin/env bash
# Tests for check_lib.sh: a broken render must never be reported as a pass.
# Run: tools/test_check_lib.sh
set -uo pipefail
cd "$(dirname "$0")/.."
source tools/check_lib.sh
t_fail=0

helper_must_fail() {  # helper_must_fail DESCRIPTION HELPER ARGS...
  local desc=$1; shift
  fail=0
  "$@" >/dev/null
  if [[ $fail -eq 1 ]]; then echo "ok   $desc"
  else echo "FAIL $desc: helper reported a pass"; t_fail=1; fi
}

# An assertion error (e.g. a mistyped part name) also prints "object is empty".
helper_must_fail "expect_empty fails on an unknown part" expect_empty no_such_part
# An undefined parameter drops keep-out geometry with only a WARNING.
helper_must_fail "expect_empty fails when a render warns" expect_empty clash_base -D idc_h=undef

[[ $t_fail -eq 0 ]] && echo "ALL TESTS PASSED" || echo "TESTS FAILED"
exit $t_fail
