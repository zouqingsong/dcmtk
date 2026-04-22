#!/bin/bash
set -e

# Build OpenSSL for macOS (native)
OPENSSL_VERSION="3.3.2"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
BUILD_DIR="$PROJECT_ROOT/build_mobile/macos/openssl"
OPENSSL_SRC_DIR="$BUILD_DIR/openssl-${OPENSSL_VERSION}"
INSTALL_DIR="$BUILD_DIR/install"

echo "=== Building OpenSSL ${OPENSSL_VERSION} for macOS ==="

# Skip if already built
if [ -f "$INSTALL_DIR/lib/libssl.a" ] && [ -f "$INSTALL_DIR/lib/libcrypto.a" ]; then
    echo "OpenSSL for macOS already built. To rebuild, rm -rf $BUILD_DIR"
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

echo "=== Building OpenSSL for macOS (universal arm64+x86_64) ==="

# Build for arm64
ARM64_BUILD="$BUILD_DIR/build_arm64"
ARM64_INSTALL="$BUILD_DIR/install_arm64"
rm -rf "$ARM64_BUILD"
cp -R "$OPENSSL_SRC_DIR" "$ARM64_BUILD"
cd "$ARM64_BUILD"

./Configure darwin64-arm64-cc \
    no-shared no-dso no-engine no-tests no-apps no-docs no-ui-console \
    --prefix="$ARM64_INSTALL" \
    -mmacosx-version-min=10.15 \
    -fPIC

make -j$(sysctl -n hw.ncpu) build_libs
make install_dev
cd "$BUILD_DIR"

# Build for x86_64
X86_BUILD="$BUILD_DIR/build_x86_64"
X86_INSTALL="$BUILD_DIR/install_x86_64"
rm -rf "$X86_BUILD"
cp -R "$OPENSSL_SRC_DIR" "$X86_BUILD"
cd "$X86_BUILD"

./Configure darwin64-x86_64-cc \
    no-shared no-dso no-engine no-tests no-apps no-docs no-ui-console \
    --prefix="$X86_INSTALL" \
    -mmacosx-version-min=10.15 \
    -fPIC

make -j$(sysctl -n hw.ncpu) build_libs
make install_dev
cd "$BUILD_DIR"

# Create universal libraries
echo "Creating universal (fat) libraries..."
mkdir -p "$INSTALL_DIR/lib"
lipo -create "$ARM64_INSTALL/lib/libssl.a" "$X86_INSTALL/lib/libssl.a" -output "$INSTALL_DIR/lib/libssl.a"
lipo -create "$ARM64_INSTALL/lib/libcrypto.a" "$X86_INSTALL/lib/libcrypto.a" -output "$INSTALL_DIR/lib/libcrypto.a"

# Copy headers (same for both architectures)
cp -R "$ARM64_INSTALL/include" "$INSTALL_DIR/"

echo ""
echo "=== OpenSSL macOS build complete ==="
echo "Libraries: $INSTALL_DIR/lib/"
echo "Headers: $INSTALL_DIR/include/"
lipo -info "$INSTALL_DIR/lib/libssl.a"
lipo -info "$INSTALL_DIR/lib/libcrypto.a"
