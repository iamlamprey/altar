#!/usr/bin/env bash

set -euo pipefail

CONFIG="Release"
BUILD_DIR="build/output"
JUCE_ROOT="../JUCE"
ENABLE_UPDATES="ON"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"

APT_PACKAGES=(
    build-essential
    cmake
    g++
    gcc
    git
    pkg-config
    libasound2-dev
    libjack-jackd2-dev
    libfreetype6-dev
    libx11-dev
    libxext-dev
    libxinerama-dev
    libxrandr-dev
    libxcursor-dev
    libxcomposite-dev
    libxrender-dev
    libgtk-3-dev
)

if apt-cache show libwebkit2gtk-4.1-dev >/dev/null 2>&1; then
    APT_PACKAGES+=(libwebkit2gtk-4.1-dev)
else
    APT_PACKAGES+=(libwebkit2gtk-4.0-dev)
fi

usage() {
    echo "Usage: $0 [--noupdate] [-c Debug|Release|MinSizeRel|RelWithDebInfo] [-b build-dir] [-j juce-root]"
}

normalize_config() {
    case "${1,,}" in
        debug) echo "Debug" ;;
        release) echo "Release" ;;
        minsizerel) echo "MinSizeRel" ;;
        relwithdebinfo) echo "RelWithDebInfo" ;;
        *)
            echo "Unknown config: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
}

while (($# > 0)); do
    case "$1" in
        --noupdate) ENABLE_UPDATES="OFF"; shift ;;
        -c|-b|-j)
            if (($# < 2)); then
                echo "Option $1 requires an argument." >&2
                usage >&2
                exit 1
            fi
            option="$1"
            value="$2"
            if [[ "$option" == "-c" ]]; then CONFIG="$(normalize_config "$value")"; fi
            if [[ "$option" == "-b" ]]; then BUILD_DIR="$value"; fi
            if [[ "$option" == "-j" ]]; then JUCE_ROOT="$value"; fi
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

check_apt_packages() {
    if ! command -v apt-get >/dev/null 2>&1 || ! command -v dpkg-query >/dev/null 2>&1; then
        echo "apt-get and dpkg-query are required for dependency installation." >&2
        exit 1
    fi

    local missing=()

    for package in "${APT_PACKAGES[@]}"; do
        if ! dpkg-query -W -f='${Status}' "$package" 2>/dev/null | grep -q "install ok installed"; then
            missing+=("$package")
        fi
    done

    if ((${#missing[@]} == 0)); then
        echo "--- Required apt packages are installed ---"
        return
    fi

    echo "--- Missing apt packages: ${missing[*]} ---"
    sudo apt-get update
    sudo apt-get install -y "${missing[@]}"
}

copy_build_artifact() {
    local source="$1"
    local destination_directory="$2"

    if [[ ! -e "$source" ]]; then
        echo "Expected build artifact was not found: $source" >&2
        exit 1
    fi

    mkdir -p "$destination_directory"

    local destination="$destination_directory/$(basename "$source")"
    rm -rf "$destination"
    cp -R "$source" "$destination_directory"

    echo "Copied: $destination"
}

check_apt_packages

resolve_repo_path() {
    local path="$1"

    if [[ "$path" = /* ]]; then
        echo "$path"
    else
        echo "$REPO_ROOT/$path"
    fi
}

RESOLVED_BUILD_DIR="$(resolve_repo_path "$BUILD_DIR")"
RESOLVED_JUCE_ROOT="$(resolve_repo_path "$JUCE_ROOT")"

CMAKE_ARGS=(
    -S "$REPO_ROOT"
    -B "$RESOLVED_BUILD_DIR"
    -DCMAKE_BUILD_TYPE="$CONFIG"
    -DCMAKE_C_COMPILER=gcc
    -DCMAKE_CXX_COMPILER=g++
    -DJUCE_ROOT="$RESOLVED_JUCE_ROOT"
    -DALTAR_ENABLE_UPDATES="$ENABLE_UPDATES"
)

echo "--- Configuring CMake with GCC ---"
cmake "${CMAKE_ARGS[@]}"

echo "--- Building $CONFIG with CMake ---"
cmake --build "$RESOLVED_BUILD_DIR" --config "$CONFIG" --parallel

case "$CONFIG" in
    Debug) OUTPUT_CONFIG="debug" ;;
    Release) OUTPUT_CONFIG="release" ;;
    *) OUTPUT_CONFIG="${CONFIG,,}" ;;
esac
if [[ "$ENABLE_UPDATES" == "OFF" ]]; then OUTPUT_CONFIG="$OUTPUT_CONFIG-noupdate"; fi

ARTIFACT_ROOT="$RESOLVED_BUILD_DIR/Altar_artefacts/$CONFIG"
OUTPUT_DIRECTORY="$REPO_ROOT/binaries/$OUTPUT_CONFIG"

echo "--- Copying build artifacts to $OUTPUT_DIRECTORY ---"
copy_build_artifact "$ARTIFACT_ROOT/Standalone/Altar" "$OUTPUT_DIRECTORY"
copy_build_artifact "$ARTIFACT_ROOT/VST3/Altar.vst3" "$OUTPUT_DIRECTORY"

echo "--- CMake build successful! ---"
