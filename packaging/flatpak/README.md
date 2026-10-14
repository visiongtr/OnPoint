# On Point Flatpak (1.0.0 RC1)

This local Flatpak build uses the KDE Qt 6.9 runtime (as declared in the manifest), rather than relying on the host's Qt libraries.

On Debian/LMDE, install `flatpak` and `flatpak-builder`, then run from the source directory:

```bash
sudo apt install flatpak flatpak-builder
./scripts/build-flatpak.sh
```

The builder installs the specified runtime/SDK and creates `dist/OnPointPhotoEditor-1.0.0-rc1-<architecture>.flatpak` by default. Install it using the exact generated filename:

```bash
flatpak install --user --reinstall ./dist/OnPointPhotoEditor-1.0.0-rc1-x86_64.flatpak
flatpak run com.onpoint.OnPointPhotoEditor
```

The sandbox currently requests home-folder access for photo workflows, CUPS printing, Wayland/X11 display access and DRI devices. For future Flathub submission, review sandbox permissions and add screenshots/validation evidence.

CPU processing should work without vendor GPU drivers being available inside the sandbox. OpenCL acceleration requires separate testing on NVIDIA, AMD and Intel systems.
