#!/bin/bash
set -e

# Build DCMTK for macOS (native desktop)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
BUILD_DIR="$PROJECT_ROOT/build_mobile/macos"

echo "Building DCMTK for macOS..."
echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"

# Check if we're on macOS
if [[ "$OSTYPE" != "darwin"* ]]; then
    echo "Error: macOS build can only be performed on macOS"
    exit 1
fi

# Build OpenSSL for macOS if script exists and not already built
OPENSSL_INSTALL_DIR="$BUILD_DIR/openssl/install"
if [ ! -f "$OPENSSL_INSTALL_DIR/lib/libssl.a" ]; then
    OPENSSL_SCRIPT="$SCRIPT_DIR/build_openssl_macos.sh"
    if [ -f "$OPENSSL_SCRIPT" ]; then
        echo "Building OpenSSL for macOS..."
        bash "$OPENSSL_SCRIPT"
    fi
fi

HAS_OPENSSL=0
if [ -f "$OPENSSL_INSTALL_DIR/lib/libssl.a" ]; then
    HAS_OPENSSL=1
    echo "OpenSSL found, TLS support will be enabled"
else
    echo "OpenSSL not found, building without TLS support"
fi

PLATFORM_BUILD_DIR="$BUILD_DIR/build"
rm -rf "$PLATFORM_BUILD_DIR"
mkdir -p "$PLATFORM_BUILD_DIR"
cd "$PLATFORM_BUILD_DIR"

# Determine OpenSSL flags
OPENSSL_FLAGS=""
if [ "$HAS_OPENSSL" -eq 1 ]; then
    OPENSSL_FLAGS="-DDCMTK_WITH_OPENSSL=ON \
        -DOPENSSL_ROOT_DIR=$OPENSSL_INSTALL_DIR \
        -DOPENSSL_INCLUDE_DIR=$OPENSSL_INSTALL_DIR/include \
        -DOPENSSL_SSL_LIBRARY=$OPENSSL_INSTALL_DIR/lib/libssl.a \
        -DOPENSSL_CRYPTO_LIBRARY=$OPENSSL_INSTALL_DIR/lib/libcrypto.a"
else
    OPENSSL_FLAGS="-DDCMTK_WITH_OPENSSL=OFF"
fi

# Build for native macOS (universal: arm64 + x86_64)
cmake -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PLATFORM_BUILD_DIR/install" \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="10.15" \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DDCMTK_DEFAULT_DICT=builtin \
    -DDCMTK_WITH_TIFF=OFF \
    -DDCMTK_WITH_PNG=OFF \
    -DDCMTK_WITH_XML=OFF \
    -DDCMTK_WITH_ZLIB=ON \
    $OPENSSL_FLAGS \
    -DDCMTK_WITH_SNDFILE=OFF \
    -DDCMTK_WITH_ICONV=OFF \
    -DDCMTK_WITH_WRAP=OFF \
    -DDCMTK_WITH_OPENJPEG=OFF \
    -DDCMTK_WITH_THREADS=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DDCMTK_LINK_STATIC=ON \
    -DBUILD_APPS=OFF \
    -DDCMTK_WITH_DOXYGEN=OFF \
    -DBUILD_TESTING=OFF \
    "$PROJECT_ROOT"

make -j$(sysctl -n hw.ncpu)
make install

echo ""
echo "=== Creating combined static library ==="

INSTALL_LIB_DIR="$PLATFORM_BUILD_DIR/install/lib"
ALL_LIBS=$(find "$INSTALL_LIB_DIR" -name "*.a" | sort)
echo "Found libraries:"
echo "$ALL_LIBS"

# Combine all static libraries into one
libtool -static -o "$INSTALL_LIB_DIR/libdcmtk_all.a" $ALL_LIBS
echo "Combined library: $INSTALL_LIB_DIR/libdcmtk_all.a"

# Copy to Flutter plugin
PLUGIN_DIR="$PROJECT_ROOT/flutter_plugin/macos"
PLUGIN_LIBS_DIR="$PLUGIN_DIR/Libs"
PLUGIN_HEADERS_DIR="$PLUGIN_DIR/Headers"

mkdir -p "$PLUGIN_LIBS_DIR"
mkdir -p "$PLUGIN_HEADERS_DIR"

cp "$INSTALL_LIB_DIR/libdcmtk_all.a" "$PLUGIN_LIBS_DIR/"
echo "Library copied to: $PLUGIN_LIBS_DIR/libdcmtk_all.a"

# Copy OpenSSL libs if available
if [ "$HAS_OPENSSL" -eq 1 ]; then
    cp "$OPENSSL_INSTALL_DIR/lib/libssl.a" "$PLUGIN_LIBS_DIR/"
    cp "$OPENSSL_INSTALL_DIR/lib/libcrypto.a" "$PLUGIN_LIBS_DIR/"
    echo "OpenSSL libraries copied"
fi

# Copy headers
cp -R "$PLATFORM_BUILD_DIR/install/include/dcmtk" "$PLUGIN_HEADERS_DIR/"

# Sanitize machine-specific paths in osconfig.h
OSCONFIG="$PLUGIN_HEADERS_DIR/dcmtk/config/osconfig.h"
if [ -f "$OSCONFIG" ]; then
    sed -i.bak \
        -e 's|"'"$PLATFORM_BUILD_DIR"'/install[^"]*"|""|g' \
        -e 's|"'"$PROJECT_ROOT"'[^"]*"|""|g' \
        "$OSCONFIG"
    rm -f "$OSCONFIG.bak"
    echo "Sanitized machine-specific paths in osconfig.h"
fi

# Copy OpenSSL headers if available
if [ -d "$OPENSSL_INSTALL_DIR/include/openssl" ]; then
    cp -R "$OPENSSL_INSTALL_DIR/include/openssl" "$PLUGIN_HEADERS_DIR/"
    echo "OpenSSL headers copied"
fi

echo ""
echo "=== macOS build complete ==="
echo "Library: $PLUGIN_LIBS_DIR/libdcmtk_all.a"
echo "Headers: $PLUGIN_HEADERS_DIR/"
echo ""
echo "Library architecture info:"
lipo -info "$PLUGIN_LIBS_DIR/libdcmtk_all.a"

# Sync wrapper source files from ios/native → macos/native
# CocoaPods doesn't follow symlinks, so we copy the files
NATIVE_SRC="$PROJECT_ROOT/flutter_plugin/ios/native"
NATIVE_DST="$PLUGIN_DIR/native"
mkdir -p "$NATIVE_DST"
cp "$NATIVE_SRC/dcmtk_flutter_wrapper.h" "$NATIVE_DST/"
cp "$NATIVE_SRC/dcmtk_flutter_wrapper.cpp" "$NATIVE_DST/"
echo "Wrapper files synced to: $NATIVE_DST/"
