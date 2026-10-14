#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFEST="${ROOT_DIR}/packaging/flatpak/com.onpoint.OnPointPhotoEditor.yml"
BUILD_DIR="${FLATPAK_BUILD_DIR:-${ROOT_DIR}/build-flatpak}"
REPO_DIR="${FLATPAK_REPO_DIR:-${ROOT_DIR}/flatpak-repo}"
DIST_DIR="${DIST_DIR:-${ROOT_DIR}/dist}"
BUNDLE="${DIST_DIR}/OnPointPhotoEditor-1.0.0-rc1-$(uname -m).flatpak"

for command in flatpak flatpak-builder; do
    if ! command -v "$command" >/dev/null 2>&1; then
        echo "Error: '$command' is not installed." >&2
        echo "On Debian/LMDE: sudo apt install flatpak flatpak-builder" >&2
        exit 1
    fi
done

mkdir -p "$DIST_DIR"

flatpak remote-add --user --if-not-exists flathub \
    https://flathub.org/repo/flathub.flatpakrepo

flatpak install --user -y flathub \
    org.kde.Platform//6.9 \
    org.kde.Sdk//6.9

rm -rf "$BUILD_DIR" "$REPO_DIR"

flatpak-builder \
    --user \
    --force-clean \
    --install-deps-from=flathub \
    --repo="$REPO_DIR" \
    "$BUILD_DIR" \
    "$MANIFEST"

rm -f "$BUNDLE"
flatpak build-bundle \
    "$REPO_DIR" \
    "$BUNDLE" \
    com.onpoint.OnPointPhotoEditor \
    stable

printf '\nFlatpak bundle created:\n  %s\n' "$BUNDLE"
printf '\nInstall or replace the local test build with:\n'
printf '  flatpak install --user --reinstall "%s"\n' "$BUNDLE"
printf '\nRun it with:\n'
printf '  flatpak run com.onpoint.OnPointPhotoEditor\n'
