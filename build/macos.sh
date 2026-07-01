#!/usr/bin/env bash

set -euo pipefail

CONFIG="Release"
BUILD_DIR="build/output"
JUCE_ROOT="../JUCE"
OSX_ARCHITECTURES="arm64;x86_64"
OSX_DEPLOYMENT_TARGET="11.0"
ENABLE_UPDATES="ON"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
PRODUCT_NAME="Altar"
PACKAGE_ID_BASE="com.iamlamprey.Altar"
STANDALONE_ENTITLEMENTS="$REPO_ROOT/Source/macos.entitlements"

usage() {
    cat <<USAGE
Usage: $0 [--noupdate] [-c Debug|Release|MinSizeRel|RelWithDebInfo] [-b build-dir] [-j juce-root]

Pipeline:
  1. Build app, VST3, and AU artifacts.
  2. Sign artifacts when MACOS_CODESIGN_IDENTITY is set.
  3. Build and sign pkg when MACOS_INSTALLER_IDENTITY is set.
  4. Notarize and staple the pkg when notary credentials are set.

Required environment variables for release:
  MACOS_CODESIGN_IDENTITY="Developer ID Application: Name (TEAMID)"
  MACOS_INSTALLER_IDENTITY="Developer ID Installer: Name (TEAMID)"

Notarization credentials, choose one option:
  MACOS_NOTARY_PROFILE="stored-notarytool-profile"

  or:

  APPLE_ID="apple-id@example.com"
  APPLE_TEAM_ID="TEAMID"
  APPLE_APP_PASSWORD="xxxx-xxxx-xxxx-xxxx"
USAGE
}

fail_step() {
    local step="$1"
    local message="$2"

    echo
    echo "ERROR: $step failed."
    echo "$message"
    echo
    exit 1
}

normalize_config() {
    local normalized

    normalized="$(printf '%s' "$1" | tr '[:upper:]' '[:lower:]')"

    case "$normalized" in
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

resolve_repo_path() {
    local path="$1"

    if [[ "$path" = /* ]]; then
        echo "$path"
    else
        echo "$REPO_ROOT/$path"
    fi
}

output_config_name() {
    case "$1" in
        Debug) echo "debug" ;;
        Release) echo "release" ;;
        *) printf '%s' "$1" | tr '[:upper:]' '[:lower:]' ;;
    esac
}

extract_project_version() {
    sed -n 's/.*project(Altar VERSION \([^ ]*\).*/\1/p' "$REPO_ROOT/CMakeLists.txt" | head -n 1
}

require_tool() {
    local tool="$1"
    local step="$2"

    if ! command -v "$tool" >/dev/null 2>&1; then
        fail_step "$step" "Missing required tool '$tool'. Install it or make sure it is available in PATH."
    fi
}

require_artifact() {
    local artifact="$1"
    local step="$2"

    if [[ ! -e "$artifact" ]]; then
        fail_step "$step" "Expected artifact was not found: $artifact"
    fi
}

remove_existing_path() {
    local path="$1"
    local step="$2"

    if [[ -e "$path" ]]; then
        chmod -R u+w "$path" 2>/dev/null || true
        rm -rf "$path" || fail_step "$step" "Could not remove existing path: $path. Check ownership/permissions, or remove it manually with sudo if it was installed/copied as root."
    fi
}

copy_build_artifact() {
    local source="$1"
    local destination_directory="$2"

    require_artifact "$source" "Build"
    mkdir -p "$destination_directory"
    remove_existing_path "$destination_directory/$(basename "$source")" "Build"
    cp -R "$source" "$destination_directory"
    echo "Copied: $destination_directory/$(basename "$source")"
}

sign_artifact() {
    local artifact="$1"

    require_artifact "$artifact" "Codesign"
    if [[ "$artifact" == *.app ]]; then
        require_artifact "$STANDALONE_ENTITLEMENTS" "Codesign"
        codesign --force --deep --options runtime --timestamp --entitlements "$STANDALONE_ENTITLEMENTS" --sign "$MACOS_CODESIGN_IDENTITY" "$artifact" \
            || fail_step "Codesign" "codesign failed for $artifact. Check MACOS_CODESIGN_IDENTITY and the Developer ID G2 certificate chain."
    else
        codesign --force --deep --options runtime --timestamp --sign "$MACOS_CODESIGN_IDENTITY" "$artifact" \
            || fail_step "Codesign" "codesign failed for $artifact. Check MACOS_CODESIGN_IDENTITY and the Developer ID G2 certificate chain."
    fi
    codesign --verify --deep --strict --verbose=2 "$artifact" \
        || fail_step "Codesign" "codesign verification failed for $artifact."
    echo "Developer ID signed: $artifact"
}

copy_to_payload() {
    local source="$1"
    local destination="$2"

    require_artifact "$source" "Package"
    mkdir -p "$(dirname "$destination")"
    remove_existing_path "$destination" "Package"
    cp -R "$source" "$destination"
}

assert_pkg_payload_contains() {
    local package="$1"
    local expected_path="$2"
    local description="$3"

    pkgutil --payload-files "$package" | grep -Eq "^\./?$expected_path$|^\./?$expected_path/" \
        || fail_step "Package" "pkg payload does not contain $description. Expected to find $expected_path in pkgutil --payload-files output."
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

OUTPUT_CONFIG="$(output_config_name "$CONFIG")"
if [[ "$ENABLE_UPDATES" == "OFF" ]]; then OUTPUT_CONFIG="$OUTPUT_CONFIG-noupdate"; fi
RESOLVED_BUILD_DIR="$(resolve_repo_path "$BUILD_DIR")"
RESOLVED_JUCE_ROOT="$(resolve_repo_path "$JUCE_ROOT")"
ARTIFACT_ROOT="$RESOLVED_BUILD_DIR/Altar_artefacts/$CONFIG"
OUTPUT_DIR="$REPO_ROOT/binaries/$OUTPUT_CONFIG"
PACKAGE_WORK_DIR="$REPO_ROOT/build/package/$OUTPUT_CONFIG"
PAYLOAD_ROOT="$PACKAGE_WORK_DIR/payload"
DIST_DIR="$REPO_ROOT/binaries/package"
COMPONENT_PLIST="$PACKAGE_WORK_DIR/component.plist"
VERSION="$(extract_project_version)"

if [[ -z "$VERSION" ]]; then
    VERSION="0.0.0"
fi

echo "--- Step 1/4: Building macOS artifacts ---"
require_tool cmake "Build"
require_tool codesign "Build"

CMAKE_ARGS=(
    -S "$REPO_ROOT"
    -B "$RESOLVED_BUILD_DIR"
    -DCMAKE_BUILD_TYPE="$CONFIG"
    -DCMAKE_OSX_ARCHITECTURES="$OSX_ARCHITECTURES"
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$OSX_DEPLOYMENT_TARGET"
    -DJUCE_ROOT="$RESOLVED_JUCE_ROOT"
    -DALTAR_ENABLE_UPDATES="$ENABLE_UPDATES"
)

cmake "${CMAKE_ARGS[@]}" || fail_step "Build" "CMake configure failed."
cmake --build "$RESOLVED_BUILD_DIR" --config "$CONFIG" --parallel || fail_step "Build" "CMake build failed."

mkdir -p "$OUTPUT_DIR"
copy_build_artifact "$ARTIFACT_ROOT/Standalone/$PRODUCT_NAME.app" "$OUTPUT_DIR"
copy_build_artifact "$ARTIFACT_ROOT/VST3/$PRODUCT_NAME.vst3" "$OUTPUT_DIR"
copy_build_artifact "$ARTIFACT_ROOT/AU/$PRODUCT_NAME.component" "$OUTPUT_DIR"

echo "--- Build complete: $OUTPUT_DIR ---"

echo "--- Step 2/4: Codesigning artifacts ---"
if [[ -z "${MACOS_CODESIGN_IDENTITY:-}" ]]; then
    fail_step "Codesign" "MACOS_CODESIGN_IDENTITY is not set. Build artifacts are available in $OUTPUT_DIR. Set MACOS_CODESIGN_IDENTITY to continue the release pipeline."
fi

sign_artifact "$OUTPUT_DIR/$PRODUCT_NAME.app"
sign_artifact "$OUTPUT_DIR/$PRODUCT_NAME.vst3"
sign_artifact "$OUTPUT_DIR/$PRODUCT_NAME.component"

echo "--- Step 3/4: Building signed pkg installer ---"
if [[ -z "${MACOS_INSTALLER_IDENTITY:-}" ]]; then
    fail_step "Package" "MACOS_INSTALLER_IDENTITY is not set. Codesigned artifacts are available in $OUTPUT_DIR. Set MACOS_INSTALLER_IDENTITY to build the installer."
fi

require_tool pkgbuild "Package"
require_tool pkgutil "Package"

remove_existing_path "$PACKAGE_WORK_DIR" "Package"
mkdir -p "$PAYLOAD_ROOT" "$DIST_DIR"

copy_to_payload "$OUTPUT_DIR/$PRODUCT_NAME.app" "$PAYLOAD_ROOT/Applications/$PRODUCT_NAME.app"
copy_to_payload "$OUTPUT_DIR/$PRODUCT_NAME.vst3" "$PAYLOAD_ROOT/Library/Audio/Plug-Ins/VST3/$PRODUCT_NAME.vst3"
copy_to_payload "$OUTPUT_DIR/$PRODUCT_NAME.component" "$PAYLOAD_ROOT/Library/Audio/Plug-Ins/Components/$PRODUCT_NAME.component"

PACKAGE_VARIANT=""
if [[ "$ENABLE_UPDATES" == "OFF" ]]; then PACKAGE_VARIANT="-noupdate"; fi
FINAL_PKG="$DIST_DIR/$PRODUCT_NAME-$VERSION-macOS$PACKAGE_VARIANT.pkg"

cat > "$COMPONENT_PLIST" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<array>
    <dict>
        <key>BundleHasStrictIdentifier</key>
        <false/>
        <key>BundleIsRelocatable</key>
        <false/>
        <key>BundleIsVersionChecked</key>
        <false/>
        <key>BundleOverwriteAction</key>
        <string>upgrade</string>
        <key>RootRelativeBundlePath</key>
        <string>Applications/$PRODUCT_NAME.app</string>
    </dict>
    <dict>
        <key>BundleHasStrictIdentifier</key>
        <false/>
        <key>BundleIsRelocatable</key>
        <false/>
        <key>BundleIsVersionChecked</key>
        <false/>
        <key>BundleOverwriteAction</key>
        <string>upgrade</string>
        <key>RootRelativeBundlePath</key>
        <string>Library/Audio/Plug-Ins/VST3/$PRODUCT_NAME.vst3</string>
    </dict>
    <dict>
        <key>BundleHasStrictIdentifier</key>
        <false/>
        <key>BundleIsRelocatable</key>
        <false/>
        <key>BundleIsVersionChecked</key>
        <false/>
        <key>BundleOverwriteAction</key>
        <string>upgrade</string>
        <key>RootRelativeBundlePath</key>
        <string>Library/Audio/Plug-Ins/Components/$PRODUCT_NAME.component</string>
    </dict>
</array>
</plist>
PLIST

remove_existing_path "$FINAL_PKG" "Package"
pkgbuild \
    --root "$PAYLOAD_ROOT" \
    --identifier "$PACKAGE_ID_BASE" \
    --version "$VERSION" \
    --install-location "/" \
    --component-plist "$COMPONENT_PLIST" \
    --sign "$MACOS_INSTALLER_IDENTITY" \
    "$FINAL_PKG" \
    || fail_step "Package" "pkgbuild failed. Check MACOS_INSTALLER_IDENTITY and the Developer ID Installer certificate chain."

pkgutil --check-signature "$FINAL_PKG" \
    || fail_step "Package" "pkg signature verification failed."

assert_pkg_payload_contains "$FINAL_PKG" "Applications/$PRODUCT_NAME.app/Contents/Info.plist" "/Applications/$PRODUCT_NAME.app"
assert_pkg_payload_contains "$FINAL_PKG" "Library/Audio/Plug-Ins/VST3/$PRODUCT_NAME.vst3/Contents/Info.plist" "/Library/Audio/Plug-Ins/VST3/$PRODUCT_NAME.vst3"
assert_pkg_payload_contains "$FINAL_PKG" "Library/Audio/Plug-Ins/Components/$PRODUCT_NAME.component/Contents/Info.plist" "/Library/Audio/Plug-Ins/Components/$PRODUCT_NAME.component"

echo "--- Step 4/4: Notarizing and stapling pkg ---"
require_tool xcrun "Notarize"

NOTARY_ARGS=()
if [[ -n "${MACOS_NOTARY_PROFILE:-}" ]]; then
    NOTARY_ARGS=(--keychain-profile "$MACOS_NOTARY_PROFILE")
elif [[ -n "${APPLE_ID:-}" && -n "${APPLE_TEAM_ID:-}" && -n "${APPLE_APP_PASSWORD:-}" ]]; then
    NOTARY_ARGS=(--apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" --password "$APPLE_APP_PASSWORD")
else
    fail_step "Notarize" "Set MACOS_NOTARY_PROFILE, or set APPLE_ID, APPLE_TEAM_ID, and APPLE_APP_PASSWORD."
fi

xcrun notarytool submit "$FINAL_PKG" "${NOTARY_ARGS[@]}" --wait \
    || fail_step "Notarize" "Apple notarization failed. Review the notarytool output above."

xcrun stapler staple "$FINAL_PKG" \
    || fail_step "Staple" "Stapling failed after notarization."

xcrun stapler validate "$FINAL_PKG" \
    || fail_step "Staple" "Stapler validation failed."

spctl --assess --type install --verbose "$FINAL_PKG" \
    || fail_step "Gatekeeper" "Gatekeeper assessment failed for the final pkg."

echo "--- macOS release complete: $FINAL_PKG ---"
