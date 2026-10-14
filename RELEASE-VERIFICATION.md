# On Point 1.0.0 RC1 — Publication Verification

This is one release checklist, not a request to reopen feature development. Check each relevant installer **once** before putting its download link in front of the public.

## Repository/source (automatically inspected on 2026-10-14)

- [x] GPLv3-or-later LICENSE, README, CHANGELOG, CONTRIBUTING and .gitignore present
- [x] Qt resource file references point to files included in source
- [x] Desktop launcher and AppStream XML present and structurally parseable
- [x] All Bash scripts pass `bash -n` syntax checks
- [x] No machine-specific Qt Creator settings or original personal-directory paths in publication package
- [x] Flatpak YAML parses successfully
- [ ] **Clean compile against real Qt 6 developer libraries** (not installed in review environment)

## Native release (`.deb`, `.rpm` or system-Qt `.tar.gz`)

- [ ] Build required installer(s) using the scripts and capture any compiler/CPack errors
- [ ] Install or extract on the target Linux machine; confirm runtime libraries resolve
- [ ] Launch desktop icon and run the binary without crashing
- [ ] Open a JPEG and PNG, make a correction, save/export, reopen the result
- [ ] Check undo/redo, crop, image dimensions, print preview
- [ ] Confirm clean exit and availability of CPU-only fallback

The supplied `onpoint-photo-editor-1.0.0-rc1-x86_64.tar.gz` is **system-linked** to Qt 6, not self-contained. Do not advertise it as universally portable. Its exact correspondence to this post-cleanup source tree cannot be proven without a fresh build.

## Flatpak (manifest provided; installer requires actual build)

- [ ] `./scripts/build-flatpak.sh` completes and writes a `.flatpak` bundle
- [ ] `flatpak install --user --reinstall ./dist/ACTUAL-FILENAME.flatpak` succeeds
- [ ] `flatpak run com.onpoint.OnPointPhotoEditor` launches
- [ ] File selection, save/export, print behavior and CPU fallback work within sandbox
- [ ] Test OpenCL separately per GPU vendor if advertising GPU support for that vendor

## Publishing to GitHub

- [ ] Push complete source to a public repository
- [ ] Create tag/release `v1.0.0-rc1` and label as **pre-release / release candidate**
- [ ] Attach only compiled artifacts that have passed the matching launch tests
- [ ] Add screenshot(s) made from the real running application; avoid mocked screenshot claims
- [ ] Link the project page and release after validating public downloads

Do not change the application version or code merely to satisfy this checklist. A successful RC1 is a valid public release.
