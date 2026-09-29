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
# the fit test must stay a quick print but keep every feature it exists to check
expect_max_volume 3000 fit_test
expect_empty outside_fit
expect_empty fit_wall_slivers
expect_empty fit_thin
expect_empty fit_unsupported
for i in 0 1 2 3 4 5 6; do expect_nonempty fit_probe -D probe=$i; done
expect_empty clash_base
expect_empty clash_lid
expect_empty clash_base_lid
expect_empty outside_base
expect_empty outside_lid
# a measured IDC plug taller than the estimate must still fit (lid depth grows)
expect_empty clash_lid -D idc_h=21
expect_empty clash_base_lid -D idc_h=21

[[ $fail -eq 0 ]] && echo "ALL CHECKS PASSED" || echo "CHECKS FAILED"
exit $fail
