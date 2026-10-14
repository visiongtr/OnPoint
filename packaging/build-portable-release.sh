#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-release-portable}"
OUTPUT_DIR="${OUTPUT_DIR:-${ROOT_DIR}/release-output}"
VERSION="1.0.0"
PACKAGE_BASENAME="OnPointPhotoEditor-${VERSION}-RC1-Linux-x86_64"
INSTALL_DIR="${OUTPUT_DIR}/${PACKAGE_BASENAME}"
ARCHIVE_PATH="${OUTPUT_DIR}/${PACKAGE_BASENAME}.tar.gz"
CHECKSUM_PATH="${ARCHIVE_PATH}.sha256"

CMAKE_BIN="${CMAKE_BIN:-cmake}"
if ! command -v "${CMAKE_BIN}" >/dev/null 2>&1; then
    echo "Error: cmake was not found." >&2
    exit 1
fi

if command -v ninja >/dev/null 2>&1; then
    GENERATOR_ARGS=(-G Ninja)
else
    GENERATOR_ARGS=()
fi

CONFIGURE_ARGS=(
    -S "${ROOT_DIR}"
    -B "${BUILD_DIR}"
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}"
    -DONPOINT_PORTABLE_DEPLOY=ON
)

# Optional override for a non-system Qt installation:
#   QT_PREFIX_PATH=/path/to/Qt/6.8.2/gcc_64 ./packaging/build-portable-release.sh
if [[ -n "${QT_PREFIX_PATH:-}" ]]; then
    CONFIGURE_ARGS+=("-DCMAKE_PREFIX_PATH=${QT_PREFIX_PATH}")
fi

find_qtpaths() {
    local candidate
    local candidates=()

    if [[ -n "${QT_PREFIX_PATH:-}" ]]; then
        candidates+=(
            "${QT_PREFIX_PATH}/bin/qtpaths6"
            "${QT_PREFIX_PATH}/bin/qtpaths"
        )
    fi

    candidates+=(
        "$(command -v qtpaths6 2>/dev/null || true)"
        "$(command -v qtpaths 2>/dev/null || true)"
        "/usr/lib/qt6/bin/qtpaths6"
        "/usr/lib/qt6/bin/qtpaths"
    )

    for candidate in "${candidates[@]}"; do
        if [[ -n "${candidate}" && -x "${candidate}" ]]; then
            printf '%s\n' "${candidate}"
            return 0
        fi
    done

    return 1
}

qt_query_path() {
    local qtpaths_bin="$1"
    local key="$2"
    local result=""

    result="$("${qtpaths_bin}" --query "${key}" 2>/dev/null || true)"
    if [[ -z "${result}" && "${key}" == "QT_INSTALL_PLUGINS" ]]; then
        result="$("${qtpaths_bin}" --plugin-dir 2>/dev/null || true)"
    fi
    if [[ -z "${result}" && "${key}" == "QT_INSTALL_LIBS" ]]; then
        result="$("${qtpaths_bin}" --library-dir 2>/dev/null || true)"
    fi

    printf '%s\n' "${result}"
}

copy_qt_dependency_tree() {
    local seed="$1"
    local current dep base
    local -a queue=("${seed}")
    local -A seen=()

    mkdir -p "${INSTALL_DIR}/lib"

    while ((${#queue[@]})); do
        current="${queue[0]}"
        queue=("${queue[@]:1}")

        [[ -f "${current}" ]] || continue
        [[ -n "${seen[${current}]:-}" ]] && continue
        seen["${current}"]=1

        while IFS= read -r dep; do
            [[ -f "${dep}" ]] || continue
            base="$(basename -- "${dep}")"
            case "${base}" in
                # Qt on Debian/LMDE may link to a versioned ICU runtime that is
                # newer than the one installed on the test computer. Bundle the
                # exact ICU ABI used by this build along with the Qt libraries.
                libQt6*.so*|libicu*.so*)
                    if [[ ! -f "${INSTALL_DIR}/lib/${base}" ]]; then
                        cp -L -- "${dep}" "${INSTALL_DIR}/lib/${base}"
                    fi
                    queue+=("${dep}")
                    ;;
            esac
        done < <(
            ldd "${current}" 2>/dev/null \
                | awk '
                    /=> \/[^ ]+/ { print $3 }
                    /^\/[[:graph:]]+[[:space:]]+\(/ { print $1 }
                ' \
                | sort -u
        )
    done
}

copy_plugin_directory() {
    local qt_plugin_root="$1"
    local plugin_type="$2"
    local source_dir="${qt_plugin_root}/${plugin_type}"
    local target_dir="${INSTALL_DIR}/plugins/${plugin_type}"
    local plugin

    [[ -d "${source_dir}" ]] || return 0

    mkdir -p "${target_dir}"
    while IFS= read -r -d '' plugin; do
        cp -L -- "${plugin}" "${target_dir}/$(basename -- "${plugin}")"
        copy_qt_dependency_tree "${plugin}"
    done < <(find "${source_dir}" -maxdepth 1 -type f -name '*.so' -print0)
}

ensure_linux_qt_plugins() {
    local qtpaths_bin qt_plugin_root qt_lib_root qxcb

    if qtpaths_bin="$(find_qtpaths)"; then
        qt_plugin_root="$(qt_query_path "${qtpaths_bin}" QT_INSTALL_PLUGINS)"
        qt_lib_root="$(qt_query_path "${qtpaths_bin}" QT_INSTALL_LIBS)"
    else
        qt_plugin_root=""
        qt_lib_root=""
    fi

    if [[ -z "${qt_plugin_root}" || ! -d "${qt_plugin_root}" ]]; then
        qt_plugin_root="$(dirname -- "$(find /usr/lib /usr/local/lib -type f -path '*/qt6/plugins/platforms/libqxcb.so' -print -quit 2>/dev/null || true)")"
        if [[ -n "${qt_plugin_root}" ]]; then
            qt_plugin_root="$(dirname -- "${qt_plugin_root}")"
        fi
    fi

    if [[ -z "${qt_plugin_root}" || ! -d "${qt_plugin_root}" ]]; then
        echo "Error: Could not locate the Qt 6 plugin directory." >&2
        echo "Install the Qt 6 platform plugins, or set QT_PREFIX_PATH to the Qt installation used for this build." >&2
        exit 1
    fi

    qxcb="${qt_plugin_root}/platforms/libqxcb.so"
    if [[ ! -f "${qxcb}" ]]; then
        echo "Error: Qt's XCB platform plugin was not found at:" >&2
        echo "  ${qxcb}" >&2
        echo "On Debian/LMDE, verify the package that supplies the Qt 6 XCB plugin is installed." >&2
        exit 1
    fi

    printf 'Qt plugin source: %s\n' "${qt_plugin_root}"
    [[ -n "${qt_lib_root}" ]] && printf 'Qt library source: %s\n' "${qt_lib_root}"

    # Debian's Qt 6.8 generic deploy helper can omit dynamically packaged
    # plugins. Copy the runtime plugin families On Point actually needs.
    copy_plugin_directory "${qt_plugin_root}" platforms
    copy_plugin_directory "${qt_plugin_root}" xcbglintegrations
    copy_plugin_directory "${qt_plugin_root}" imageformats
    copy_plugin_directory "${qt_plugin_root}" platforminputcontexts
    copy_plugin_directory "${qt_plugin_root}" printsupport

    # libqxcb requires Qt6XcbQpa. Copy its Qt dependency chain explicitly.
    copy_qt_dependency_tree "${qxcb}"

    if [[ -n "${qt_lib_root}" && -d "${qt_lib_root}" ]]; then
        local xcb_qpa
        xcb_qpa="$(find "${qt_lib_root}" -maxdepth 1 -type f -name 'libQt6XcbQpa.so.6*' -print -quit 2>/dev/null || true)"
        if [[ -n "${xcb_qpa}" ]]; then
            cp -L -- "${xcb_qpa}" "${INSTALL_DIR}/lib/libQt6XcbQpa.so.6"
            copy_qt_dependency_tree "${xcb_qpa}"
        fi
    fi
}

printf '\n== On Point Photo Editor 1.0 RC1 portable build ==\n'
printf 'Source:  %s\n' "${ROOT_DIR}"
printf 'Build:   %s\n' "${BUILD_DIR}"
printf 'Package: %s\n\n' "${INSTALL_DIR}"

"${CMAKE_BIN}" "${GENERATOR_ARGS[@]}" "${CONFIGURE_ARGS[@]}"
"${CMAKE_BIN}" --build "${BUILD_DIR}" --parallel "$(nproc)"

rm -rf "${INSTALL_DIR}"
mkdir -p "${OUTPUT_DIR}"
"${CMAKE_BIN}" --install "${BUILD_DIR}"

# Start from the application itself so the bundle receives both its Qt
# libraries and any version-sensitive Qt runtime dependencies such as ICU.
copy_qt_dependency_tree "${INSTALL_DIR}/bin/OnPointPhotoEditor"

# Qt's generic Linux deployment step should install plugins. Some distro Qt
# builds omit them from CMake plugin metadata, so verify and repair the bundle.
ensure_linux_qt_plugins

cat > "${INSTALL_DIR}/OnPointPhotoEditor" <<'LAUNCHER'
#!/usr/bin/env sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export LD_LIBRARY_PATH="$HERE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$HERE/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
export QT_QPA_PLATFORM_PLUGIN_PATH="$HERE/plugins/platforms"
exec "$HERE/bin/OnPointPhotoEditor" "$@"
LAUNCHER
chmod +x "${INSTALL_DIR}/OnPointPhotoEditor"

cp "${ROOT_DIR}/packaging/PORTABLE-RELEASE-README.txt" "${INSTALL_DIR}/README.txt"
cp "${ROOT_DIR}/packaging/RC1-TEST-CHECKLIST.txt" "${INSTALL_DIR}/RC1-TEST-CHECKLIST.txt"
cp "${ROOT_DIR}/packaging/GPU-ACCELERATION-SETUP.txt" "${INSTALL_DIR}/GPU-ACCELERATION-SETUP.txt"

if [[ ! -x "${INSTALL_DIR}/bin/OnPointPhotoEditor" ]]; then
    echo "Error: deployed executable is missing." >&2
    exit 1
fi

if [[ ! -f "${INSTALL_DIR}/bin/qt.conf" ]]; then
    echo "Error: Qt deployment did not create bin/qt.conf." >&2
    exit 1
fi

XCB_PLUGIN="${INSTALL_DIR}/plugins/platforms/libqxcb.so"
if [[ ! -f "${XCB_PLUGIN}" ]]; then
    echo "Error: Qt XCB platform plugin was not deployed." >&2
    exit 1
fi

MISSING_DEPS=$(ldd "${INSTALL_DIR}/bin/OnPointPhotoEditor" 2>/dev/null | awk '/not found/{print}' || true)
if [[ -n "${MISSING_DEPS}" ]]; then
    printf 'Error: unresolved executable runtime dependencies:\n%s\n' "${MISSING_DEPS}" >&2
    exit 1
fi

PLUGIN_MISSING=""
while IFS= read -r -d '' plugin; do
    missing=$(LD_LIBRARY_PATH="${INSTALL_DIR}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
        ldd "${plugin}" 2>/dev/null | awk '/not found/{print}' || true)
    if [[ -n "${missing}" ]]; then
        PLUGIN_MISSING+=$'\n'"${plugin}:"$'\n'"${missing}"$'\n'
    fi
done < <(find "${INSTALL_DIR}/plugins" -type f -name '*.so' -print0)

if [[ -n "${PLUGIN_MISSING}" ]]; then
    printf 'Error: unresolved Qt plugin runtime dependencies:%s\n' "${PLUGIN_MISSING}" >&2
    exit 1
fi

# A normal ldd check on the build computer can silently resolve ICU from the
# host system even when the portable folder forgot to include it. Explicitly
# verify every ICU SONAME required by the bundled application/Qt runtime.
ICU_REQUIRED=""
while IFS= read -r -d '' runtime_file; do
    while IFS= read -r soname; do
        [[ -n "${soname}" ]] || continue
        ICU_REQUIRED+="${soname}"$'\n'
    done < <(
        LD_LIBRARY_PATH="${INSTALL_DIR}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
            ldd "${runtime_file}" 2>/dev/null \
            | awk '/libicu[^[:space:]]*\.so/{print $1}'
    )
done < <(
    find "${INSTALL_DIR}/bin" "${INSTALL_DIR}/lib" "${INSTALL_DIR}/plugins" \
        -type f \
        \( -name 'OnPointPhotoEditor' -o -name 'libQt6*.so*' -o -name '*.so' \) \
        -print0
)

while IFS= read -r soname; do
    [[ -n "${soname}" ]] || continue
    if [[ ! -f "${INSTALL_DIR}/lib/${soname}" ]]; then
        echo "Error: portable package is missing required ICU runtime ${soname}." >&2
        exit 1
    fi
done < <(printf '%s' "${ICU_REQUIRED}" | sort -u)

rm -f "${ARCHIVE_PATH}" "${CHECKSUM_PATH}"
tar -C "${OUTPUT_DIR}" -czf "${ARCHIVE_PATH}" "${PACKAGE_BASENAME}"
(
    cd "${OUTPUT_DIR}"
    sha256sum "$(basename "${ARCHIVE_PATH}")" > "$(basename "${CHECKSUM_PATH}")"
)

printf '\nPortable Release Candidate created successfully.\n'
printf 'Folder:   %s\n' "${INSTALL_DIR}"
printf 'Archive:  %s\n' "${ARCHIVE_PATH}"
printf 'Checksum: %s\n\n' "${CHECKSUM_PATH}"
printf 'Copy the archive to the Beelink or laptop, extract it, and run:\n'
printf '  ./%s/OnPointPhotoEditor\n\n' "${PACKAGE_BASENAME}"
