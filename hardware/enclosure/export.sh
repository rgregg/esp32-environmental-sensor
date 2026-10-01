#!/usr/bin/env bash
# Render the printable STLs into build/stl/. Run ./check.sh first.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build/stl
for part in base lid fit_test; do
  echo "rendering $part..."
  if ! out=$(tools/openscad.sh -D "part=\"$part\"" -o "build/stl/$part.stl" enclosure.scad 2>&1); then
    echo "$out"; exit 1
  fi
done
grep ECHO <<<"$out" | sed 's/^ECHO: //'
ls -l build/stl
