ON POINT PHOTO EDITOR 1.0 RC1 — PORTABLE LINUX TEST BUILD
=========================================================

This folder is a self-contained Release build for LMDE/Debian-family testing.
Qt does not need to be installed on the test computer.

Run it from a terminal with:

    ./OnPointPhotoEditor

Or double-click the OnPointPhotoEditor launcher and choose Run if your file
manager asks what to do.

FIRST LAUNCH
------------
The System Optimizer should inspect this computer and create a hardware-specific
performance profile. Each computer should receive its own profile ID and saved
settings. A missing or unsupported OpenCL provider is not fatal; On Point should
continue with the optimized CPU engine.

CONFIGURATION LOCATION
----------------------
The performance profile and preferences are stored under the current user's
normal configuration directory, typically:

    ~/.config/OnPoint/OnPointPhotoEditor/

This portable folder does not overwrite or bundle a profile from the build
computer.

PORTABLE RUNTIME NOTE
---------------------
This portable release bundles the exact versioned ICU libraries used by its Qt runtime. This
prevents startup failures such as missing libicui18n.so.76 on otherwise
compatible Debian/LMDE computers with an older system ICU release.

OPENCL / GPU NOTES
------------------
GPU acceleration is discovered at runtime. The portable package includes On
Point and its Qt runtime, but it does not bundle proprietary GPU drivers or an
OpenCL ICD. The test computer uses its own installed graphics driver.

If no compatible OpenCL device is available, CPU fallback should remain active.

DEBIAN / LMDE GPU ACCELERATION PREREQUISITES
---------------------------------------------
The Qt runtime is bundled, but OpenCL is supplied by the operating system.
Install the provider that matches the graphics hardware before expecting GPU
acceleration:

NVIDIA:

    sudo apt install nvidia-opencl-icd clinfo

AMD Radeon / Mesa (Rusticl):

    sudo apt install mesa-opencl-icd clinfo

On Point automatically enables the AMD radeonsi Rusticl device when an AMD GPU
and rusticl.icd are detected. It also prefers Rusticl over the older Mesa/Clover
OpenCL provider because the exact startup self-test rejected incorrect pixel and
alpha output from that older provider during RC testing.

Verify the system provider with:

    clinfo -l

OpenCL is optional. If the provider is missing or fails On Point's exact pixel
self-test, the optimized multicore CPU engine remains active.

UNINSTALL
---------
Delete this extracted folder. To also remove test preferences and the generated
performance profile, delete:

    ~/.config/OnPoint/OnPointPhotoEditor/
