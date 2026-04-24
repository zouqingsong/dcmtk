#!/bin/bash
set -e

# Build DCMTK static libraries for Linux x86_64
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
BUILD_DIR="$PROJECT_ROOT/build_mobile/linux"
INSTALL_DIR="$BUILD_DIR/install"

echo "=========================================="
echo "Building DCMTK for Linux x86_64"
echo "=========================================="
echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"

# Check for required tools
for tool in cmake make gcc g++; do
    if ! command -v $tool &>/dev/null; then
        echo "Error: $tool is not installed"
        exit 1
    fi
done

# Build OpenSSL for Linux first (if script exists and not already built)
OPENSSL_ROOT="$BUILD_DIR/openssl/install"
OPENSSL_SCRIPT="$SCRIPT_DIR/build_openssl_linux.sh"
if [ -f "$OPENSSL_SCRIPT" ] && [ ! -f "$OPENSSL_ROOT/lib/libssl.a" ]; then
    echo "Building OpenSSL for Linux..."
    bash "$OPENSSL_SCRIPT"
fi

# Check for system OpenSSL if no custom build
OPENSSL_FLAGS=""
if [ -f "$OPENSSL_ROOT/lib/libssl.a" ]; then
    echo "Using custom-built OpenSSL from $OPENSSL_ROOT"
    OPENSSL_FLAGS="-DDCMTK_WITH_OPENSSL=ON \
        -DOPENSSL_ROOT_DIR=$OPENSSL_ROOT \
        -DOPENSSL_INCLUDE_DIR=$OPENSSL_ROOT/include \
        -DOPENSSL_SSL_LIBRARY=$OPENSSL_ROOT/lib/libssl.a \
        -DOPENSSL_CRYPTO_LIBRARY=$OPENSSL_ROOT/lib/libcrypto.a"
elif pkg-config --exists openssl 2>/dev/null; then
    echo "Using system OpenSSL"
    OPENSSL_FLAGS="-DDCMTK_WITH_OPENSSL=ON"
else
    echo "OpenSSL not found, building without TLS support"
    OPENSSL_FLAGS="-DDCMTK_WITH_OPENSSL=OFF"
fi

# Clean and create build directory
rm -rf "$BUILD_DIR/build"
mkdir -p "$BUILD_DIR/build"
cd "$BUILD_DIR/build"

echo ""
echo "Configuring DCMTK with CMake..."
cmake -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_C_FLAGS="-fPIC" \
    -DCMAKE_CXX_FLAGS="-fPIC" \
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
    -DDCMTK_ENABLE_BUILTIN_DICTIONARY=ON \
    -DDCMTK_ENABLE_EXTERNAL_DICTIONARY=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DDCMTK_LINK_STATIC=ON \
    -DBUILD_APPS=OFF \
    -DBUILD_TESTING=OFF \
    -DDCMTK_WITH_DOXYGEN=OFF \
    -Wno-dev \
    "$PROJECT_ROOT"

echo ""
echo "Building DCMTK (this may take a while)..."
make -j$(nproc)
make install

echo ""
echo "Collecting libraries and headers for Flutter plugin..."

# Output directories
PLUGIN_DIR="$PROJECT_ROOT/flutter_plugin/linux"
PLUGIN_LIBS_DIR="$PLUGIN_DIR/Libs"
PLUGIN_HEADERS_DIR="$PLUGIN_DIR/Headers"

mkdir -p "$PLUGIN_LIBS_DIR"
mkdir -p "$PLUGIN_HEADERS_DIR"

# Combine all static libs into one archive using ar scripts
echo "Combining static libraries..."
COMBINED_LIB="$PLUGIN_LIBS_DIR/libdcmtk_all.a"

# Create MRI script for ar to combine all .a files
AR_SCRIPT="$BUILD_DIR/build/ar_combine.mri"
echo "CREATE $COMBINED_LIB" > "$AR_SCRIPT"
for lib in $(find "$INSTALL_DIR/lib" -name "*.a" | sort); do
    echo "ADDLIB $lib" >> "$AR_SCRIPT"
done
echo "SAVE" >> "$AR_SCRIPT"
echo "END" >> "$AR_SCRIPT"

ar -M < "$AR_SCRIPT"
ranlib "$COMBINED_LIB"

echo "Combined library: $(du -h "$COMBINED_LIB" | cut -f1)"

# Also copy individual libs for reference
find "$INSTALL_DIR/lib" -name "*.a" -exec cp {} "$PLUGIN_LIBS_DIR/" \;

# Copy headers
cp -R "$INSTALL_DIR/include/dcmtk" "$PLUGIN_HEADERS_DIR/"

# Sanitize machine-specific paths in osconfig.h
OSCONFIG="$PLUGIN_HEADERS_DIR/dcmtk/config/osconfig.h"
if [ -f "$OSCONFIG" ]; then
    sed -i \
        -e "s|\"$INSTALL_DIR[^\"]*\"|\"\"|g" \
        -e "s|\"$PROJECT_ROOT[^\"]*\"|\"\"|g" \
        -e "s|\"$BUILD_DIR[^\"]*\"|\"\"|g" \
        "$OSCONFIG"
    echo "Sanitized machine-specific paths in osconfig.h"
fi

# Copy OpenSSL libraries if available
if [ -f "$OPENSSL_ROOT/lib/libssl.a" ]; then
    cp "$OPENSSL_ROOT/lib/libssl.a" "$PLUGIN_LIBS_DIR/"
    cp "$OPENSSL_ROOT/lib/libcrypto.a" "$PLUGIN_LIBS_DIR/"
    if [ -d "$OPENSSL_ROOT/include/openssl" ]; then
        cp -R "$OPENSSL_ROOT/include/openssl" "$PLUGIN_HEADERS_DIR/"
    fi
    echo "OpenSSL libraries copied"
elif pkg-config --exists openssl 2>/dev/null; then
    echo "System OpenSSL will be used at link time"
fi

# Sync wrapper files from ios/native/ to linux native reference
echo "Verifying wrapper files..."
if [ -f "$PROJECT_ROOT/flutter_plugin/ios/native/dcmtk_flutter_wrapper.cpp" ]; then
    echo "  Wrapper source available at ios/native/"
fi

echo ""
echo "=========================================="
echo "Linux Build Summary"
echo "=========================================="
echo "Combined library: $COMBINED_LIB"
echo "Individual libs:"
ls -la "$PLUGIN_LIBS_DIR/"*.a 2>/dev/null | awk '{print "  " $NF " (" $5 " bytes)"}'
echo ""
echo "Headers: $PLUGIN_HEADERS_DIR/"
echo ""
echo "Build completed successfully!"
