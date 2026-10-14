# Changelog

## 1.0.0 RC1 — 2026-07-30

- First public Linux release candidate of On Point Photo Editor.
- GPL-3.0-or-later license, Linux desktop entry, application icon and AppStream metadata.
- Native `.deb`, `.rpm` and system-Qt `.tar.gz` packaging through CMake/CPack.
- Source-side Qt-bundled portable archive builder and Flatpak/KDE Qt 6.9 build manifest.
- Optional OpenCL backends for supported NVIDIA, AMD and Intel hardware, with CPU fallback.
- Previous release-candidate corrections to OpenCL device selection, Qt/XCB plugins and ICU dependency deployment incorporated into source/portable build script.

## 1.0.0 RC1 — public source packaging cleanup (2026-10-14)

- Removed personal development paths and superseded draft/hotfix overlays from the public source archive.
- Adjusted native package scripts to use the checkout-relative build and `dist/` directories by default.
- Documented which release packages are system-linked versus Qt-bundled and added a unified release verification checklist.
- No image-processing or UI application source code changed in this packaging cleanup.
