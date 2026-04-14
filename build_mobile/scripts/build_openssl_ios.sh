#!/bin/bash
set -e

# Build OpenSSL for iOS (device arm64 + simulator arm64)
# Requires: Xcode command line tools

OPENSSL_VERSION="3.3.2"
OPENSSL_SHA256="e13f24d3272e30e3b1dfdd8ceee1dffc7e23b0a0e7c62d0a5a0e564e67fdd56b"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
BUILD_DIR="$PROJECT_ROOT/build_mobile/ios/openssl"
OPENSSL_SRC_DIR="$BUILD_DIR/openssl-${OPENSSL_VERSION}"
INSTALL_DIR="$BUILD_DIR/install"

IOS_MIN_VERSION="11.0"

echo "=== Building OpenSSL ${OPENSSL_VERSION} for iOS ==="
echo "Build directory: $BUILD_DIR"

# Skip if already built
if [ -f "$INSTALL_DIR/device/lib/libssl.a" ] && [ -f "$INSTALL_DIR/simulator/lib/libssl.a" ]; then
    echo "OpenSSL already built. To rebuild, rm -rf $BUILD_DIR"
    exit 0
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Download OpenSSL
TARBALL="openssl-${OPENSSL_VERSION}.tar.gz"
if [ ! -f "$TARBALL" ]; then
    echo "Downloading OpenSSL ${OPENSSL_VERSION}..."
    curl -fSL "https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/${TARBALL}" -o "$TARBALL"
fi

# Extract
if [ ! -d "$OPENSSL_SRC_DIR" ]; then
    echo "Extracting..."
    tar xzf "$TARBALL"
fi

build_openssl() {
    local PLATFORM=$1   # "device" or "simulator"
    local TARGET=$2     # OpenSSL target name
    local SDK=$3        # iphoneos or iphonesimulator

    local PLATFORM_INSTALL_DIR="$INSTALL_DIR/$PLATFORM"
    local PLATFORM_BUILD_DIR="$BUILD_DIR/build_${PLATFORM}"

    if [ -f "$PLATFORM_INSTALL_DIR/lib/libssl.a" ]; then
        echo "OpenSSL for $PLATFORM already built, skipping."
        return
    fi

    echo ""
    echo "=== Building OpenSSL for $PLATFORM ($TARGET) ==="

    rm -rf "$PLATFORM_BUILD_DIR"
    cp -R "$OPENSSL_SRC_DIR" "$PLATFORM_BUILD_DIR"
    cd "$PLATFORM_BUILD_DIR"

    local SDK_PATH
    SDK_PATH=$(xcrun --sdk "$SDK" --show-sdk-path)

    export CROSS_TOP="$(dirname "$(dirname "$SDK_PATH")")"
    export CROSS_SDK="$(basename "$SDK_PATH")"

    # Platform-specific flags
    local EXTRA_CFLAGS
    if [ "$PLATFORM" = "simulator" ]; then
        # Use -target to get proper LC_BUILD_VERSION with platform=IOSSIMULATOR
        EXTRA_CFLAGS="-target arm64-apple-ios14.0-simulator"
    else
        EXTRA_CFLAGS="-mios-version-min=${IOS_MIN_VERSION}"
    fi

    # Configure
    ./Configure "$TARGET" \
        no-shared \
        no-dso \
        no-engine \
        no-tests \
        no-apps \
        no-docs \
        no-ui-console \
        --prefix="$PLATFORM_INSTALL_DIR" \
        -fembed-bitcode \
        "$EXTRA_CFLAGS"

    # Build
    make -j$(sysctl -n hw.ncpu) build_libs

    # Install (just libs + headers)
    make install_dev

    cd "$BUILD_DIR"
    echo "=== Completed OpenSSL for $PLATFORM ==="
}

# Build for iOS device (arm64)
build_openssl "device" "ios64-xcrun" "iphoneos"

# Build for iOS simulator (arm64 Apple Silicon)
build_openssl "simulator" "iossimulator-xcrun" "iphonesimulator"

echo ""
echo "=== OpenSSL build complete ==="
echo "Device libs:    $INSTALL_DIR/device/lib/"
echo "Simulator libs: $INSTALL_DIR/simulator/lib/"
echo "Device headers: $INSTALL_DIR/device/include/"

# Verify
for platform in device simulator; do
    echo ""
    echo "$platform libraries:"
    ls -la "$INSTALL_DIR/$platform/lib"/libssl.a "$INSTALL_DIR/$platform/lib"/libcrypto.a 2>/dev/null || echo "  NOT FOUND"
done
