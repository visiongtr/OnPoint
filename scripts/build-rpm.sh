#!/usr/bin/env bash
set -Eeuo pipefail
ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-release}"
DIST_DIR="${DIST_DIR:-${ROOT_DIR}/dist}"
command -v rpmbuild >/dev/null 2>&1 || { echo "Install 'rpm' on Debian/LMDE to obtain rpmbuild." >&2; exit 1; }
mkdir -p "$DIST_DIR"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DONPOINT_PORTABLE_DEPLOY=OFF
cmake --build "$BUILD_DIR" --parallel "${JOBS:-$(nproc)}"
(cd "$BUILD_DIR" && cpack -G RPM)
find "$BUILD_DIR" -maxdepth 1 -type f -name '*.rpm' -exec cp -fv {} "$DIST_DIR/" \;
