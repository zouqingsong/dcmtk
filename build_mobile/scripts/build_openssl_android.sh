#!/bin/bash
set -e

# Build OpenSSL for Android (arm64-v8a)
# Requires: ANDROID_NDK_ROOT environment variable

OPENSSL_VERSION="3.3.2"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
BUILD_DIR="$PROJECT_ROOT/build_mobile/android/openssl"
OPENSSL_SRC_DIR="$BUILD_DIR/openssl-${OPENSSL_VERSION}"
INSTALL_DIR="$BUILD_DIR/install"

ANDROID_API=24

echo "=== Building OpenSSL ${OPENSSL_VERSION} for Android ==="
echo "Build directory: $BUILD_DIR"

# Check NDK
if [ -z "$ANDROID_NDK_ROOT" ]; then
    # Try common locations
    if [ -d "$HOME/Library/Android/sdk/ndk" ]; then
        ANDROID_NDK_ROOT=$(ls -d "$HOME/Library/Android/sdk/ndk"/*/ 2>/dev/null | sort -V | tail -1 | sed 's/\/$//')
    fi
    if [ -z "$ANDROID_NDK_ROOT" ]; then
        echo "Error: ANDROID_NDK_ROOT not set and NDK not found"
        exit 1
    fi
fi
echo "Using NDK: $ANDROID_NDK_ROOT"

# Determine host OS
UNAME_S=$(uname -s)
case "$UNAME_S" in
    Darwin) HOST_TAG="darwin-x86_64" ;;
    Linux)  HOST_TAG="linux-x86_64" ;;
    *)      echo "Unsupported host OS: $UNAME_S"; exit 1 ;;
esac

TOOLCHAIN="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/$HOST_TAG"

# Skip if already built
if [ -f "$INSTALL_DIR/arm64-v8a/lib/libssl.a" ] && [ -f "$INSTALL_DIR/arm64-v8a/lib/libcrypto.a" ]; then
    echo "OpenSSL for Android already built. To rebuild, rm -rf $BUILD_DIR"
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

build_openssl_android() {
    local ABI=$1        # arm64-v8a
    local TARGET=$2     # android-arm64

    local ABI_INSTALL_DIR="$INSTALL_DIR/$ABI"
    local ABI_BUILD_DIR="$BUILD_DIR/build_${ABI}"

    if [ -f "$ABI_INSTALL_DIR/lib/libssl.a" ]; then
        echo "OpenSSL for $ABI already built, skipping."
        return
    fi

    echo ""
    echo "=== Building OpenSSL for Android $ABI ($TARGET) ==="

    rm -rf "$ABI_BUILD_DIR"
    cp -R "$OPENSSL_SRC_DIR" "$ABI_BUILD_DIR"
    cd "$ABI_BUILD_DIR"

    export ANDROID_NDK_HOME="$ANDROID_NDK_ROOT"
    export PATH="$TOOLCHAIN/bin:$PATH"

    # Configure for Android
    ./Configure "$TARGET" \
        -D__ANDROID_API__=$ANDROID_API \
        no-shared \
        no-dso \
        no-engine \
        no-tests \
        no-apps \
        no-docs \
        no-ui-console \
        --prefix="$ABI_INSTALL_DIR" \
        -fPIC

    # Build
    make -j$(sysctl -n hw.ncpu 2>/dev/null || nproc) build_libs

    # Install (just libs + headers)
    make install_dev

    cd "$BUILD_DIR"
    echo "=== Completed OpenSSL for Android $ABI ==="
}

# Build for arm64-v8a
build_openssl_android "arm64-v8a" "android-arm64"

echo ""
echo "=== OpenSSL Android build complete ==="
echo "arm64-v8a libs: $INSTALL_DIR/arm64-v8a/lib/"
echo "Headers:        $INSTALL_DIR/arm64-v8a/include/"

# Verify
echo ""
echo "Libraries:"
ls -la "$INSTALL_DIR/arm64-v8a/lib"/libssl.a "$INSTALL_DIR/arm64-v8a/lib"/libcrypto.a 2>/dev/null || echo "  NOT FOUND"
