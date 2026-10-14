# On Point Photo Editor

**Precision photo correction for Linux — version 1.0.0 RC1**

On Point is a desktop Qt photo editor for correcting and restoring photographs without the complexity of a layer-based graphics suite. It is free software licensed under **GPL-3.0-or-later** (see [LICENSE](LICENSE)).

## Features

- Brightness, contrast, gamma, saturation, hue, highlights, midtones and shadows
- White balance, red-eye correction, sharpening/focus and noise reduction
- Selective overexposure recovery and local editing tools
- Crop, resize, rotate, flip, undo and redo
- Image import/export through Qt image plugins; JPEG and PNG support
- Printing, print preview and PDF output
- Multithreaded CPU processing; optional OpenCL acceleration with automatic CPU fallback

## Download and installation

Download published installers from this repository's **Releases** page. Release artifacts are separate from the source-code archive:

- **Flatpak (`.flatpak`)**, when supplied: install with `flatpak install --user ./OnPointPhotoEditor-1.0.0-rc1-x86_64.flatpak`; run with `flatpak run com.onpoint.OnPointPhotoEditor`. This is a locally distributed Flatpak bundle, not necessarily a Flathub listing.
- **Debian (`.deb`)**, when supplied: install with `sudo apt install ./PACKAGE-NAME.deb` (substitute the downloaded filename).
- **RPM (`.rpm`)**, when supplied: install using the distribution's package manager.
- **Native `.tar.gz`**, when supplied: extract it and run `bin/OnPointPhotoEditor`. The system-linked archive requires compatible Qt 6 libraries; **it is not a self-contained AppImage or portable runtime**. The optional portable builder below generates a different archive, with bundled Qt runtime components.

Not all formats are included in every release. Check the assets that are actually attached.

## Build from source

Build prerequisites: a C++17 compiler, CMake 3.21+, Ninja, Qt 6 development libraries (Core, Gui, Widgets, PrintSupport, Concurrent and Qt build tools), and a Linux desktop environment. On Debian/LMDE:

```bash
sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-base-dev-tools
```

From the extracted source directory:

```bash
cmake --preset onpoint-release
cmake --build --preset onpoint-release --parallel
./build-release/OnPointPhotoEditor
```

The CMake preset builds into `build-release/` inside your checkout. In Qt Creator, open `CMakeLists.txt` and select the **On Point Release** preset when available. A build with system Qt libraries may depend on your installed Qt runtime. OpenCL is optional: the CPU backend remains available without it.

## Build release packages

Build Debian packaging with `./scripts/build-deb.sh`; build RPM packaging with `./scripts/build-rpm.sh` (requires `rpmbuild`, provided by `rpm` on Debian/LMDE), or both via `./scripts/build-packages.sh`. Package artifacts are copied into `dist/` by default; override with `DIST_DIR=/your/output/directory` if desired.

Build the locally distributed Flatpak with:

```bash
sudo apt install flatpak flatpak-builder
./scripts/build-flatpak.sh
```

The Flatpak builder installs the specified KDE SDK/runtime from Flathub and creates a `.flatpak` bundle in `dist/`. **The source archive contains the Flatpak build recipe, not a precompiled `.flatpak` file.** GPU acceleration in Flatpak depends on the graphics runtime exposed to the sandbox; verify separately on each vendor stack.

To build a different **Qt-bundled portable archive**, run `./packaging/build-portable-release.sh` on a machine with compatible Qt deployment tooling. The output goes to `release-output/`. This builder has platform-specific Qt dependencies and requires a clean-machine smoke test before publication.

## Release status

`1.0.0 RC1` is a public *release candidate*, not an assertion that every GPU/system combination has been tested. Before a wider announcement, use [RELEASE-VERIFICATION.md](RELEASE-VERIFICATION.md) to check build, packaging and user-facing functionality. See [CHANGELOG.md](CHANGELOG.md) and [CONTRIBUTING.md](CONTRIBUTING.md).

Copyright © 2026 Gary Scott. Licensed GPL-3.0-or-later.
