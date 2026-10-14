#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-release}"
DIST_DIR="${DIST_DIR:-${ROOT_DIR}/dist}"
JOBS="${JOBS:-$(nproc)}"

for command in cmake cpack; do
    if ! command -v "$command" >/dev/null 2>&1; then
        echo "Error: required command '$command' is not installed." >&2
        exit 1
    fi
done

if ! command -v rpmbuild >/dev/null 2>&1; then
    echo "Error: rpmbuild is missing. On Debian/LMDE install the package named 'rpm'." >&2
    exit 1
fi

mkdir -p "$DIST_DIR"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DONPOINT_PORTABLE_DEPLOY=OFF
cmake --build "$BUILD_DIR" --parallel "$JOBS"

(
    cd "$BUILD_DIR"
    cpack -G DEB
    cpack -G RPM
    cpack -G TGZ
)

find "$BUILD_DIR" -maxdepth 1 -type f \
    \( -name '*.deb' -o -name '*.rpm' -o -name '*.tar.gz' \) \
    -exec cp -fv {} "$DIST_DIR/" \;

printf '\nFinished packages:\n'
find "$DIST_DIR" -maxdepth 1 -type f -printf '  %f\n' | sort
