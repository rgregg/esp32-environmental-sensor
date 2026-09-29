#!/usr/bin/env bash
# Automated enclosure checks. Exit 0 only if every check passes.
set -uo pipefail
cd "$(dirname "$0")"
source tools/check_lib.sh

expect_solid keepouts
expect_nonempty clash_selftest
expect_nonempty usb_exits_right
expect_nonempty rj45_exits_top
expect_solid base
expect_solid lid
expect_solid fit_test
expect_empty clash_base
expect_empty clash_lid
expect_empty clash_base_lid
expect_empty outside_base
expect_empty outside_lid

[[ $fail -eq 0 ]] && echo "ALL CHECKS PASSED" || echo "CHECKS FAILED"
exit $fail
