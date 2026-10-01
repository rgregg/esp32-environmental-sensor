#!/usr/bin/env bash
# Run OpenSCAD (Manifold backend) from a pinned Docker image, with the
# enclosure directory mounted at /w. Falls back to an `openscad` on PATH
# (e.g. the AppImage in ~/.local/bin) when OPENSCAD_NO_DOCKER=1 or Docker is absent.
set -euo pipefail
IMAGE="openscad/openscad@sha256:992508950d86ed5ea6a6ed19934e7d65aa6b1959df69823666f575e9c1579b49"
DIR="$(cd "$(dirname "$0")/.." && pwd)"
if [[ "${OPENSCAD_NO_DOCKER:-0}" != 1 ]] && command -v docker >/dev/null 2>&1; then
  exec docker run --rm -u "$(id -u):$(id -g)" -v "$DIR:/w" -w /w "$IMAGE" \
    openscad --backend=manifold "$@"
fi
cd "$DIR"
exec openscad --backend=manifold "$@"
