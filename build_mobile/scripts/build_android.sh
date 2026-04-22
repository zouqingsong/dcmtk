#!/bin/bash
set -e

# Build DCMTK for Android
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
BUILD_DIR="$PROJECT_ROOT/build_mobile/android"

echo "Building DCMTK for Android..."
echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"

# Check if Android NDK is set
if [ -z "$ANDROID_NDK_ROOT" ]; then
    echo "Error: ANDROID_NDK_ROOT environment variable is not set"
    echo "Please set it to your Android NDK installation path"
    echo "Example: export ANDROID_NDK_ROOT=/path/to/android-ndk-r25b"
    exit 1
fi

echo "Using Android NDK: $ANDROID_NDK_ROOT"

# Check if Android SDK is set
if [ -z "$ANDROID_SDK_ROOT" ]; then
    ANDROID_SDK_ROOT="$(dirname "$(dirname "$ANDROID_NDK_ROOT")")"
    echo "ANDROID_SDK_ROOT not set, inferred: $ANDROID_SDK_ROOT"
fi

# Build OpenSSL for Android first (if not already built)
OPENSSL_SCRIPT="$SCRIPT_DIR/build_openssl_android.sh"
if [ -f "$OPENSSL_SCRIPT" ]; then
    echo "Building OpenSSL for Android..."
    bash "$OPENSSL_SCRIPT"
fi

OPENSSL_ROOT="$PROJECT_ROOT/build_mobile/android/openssl/install"

# Android ABIs to build for
ANDROID_ABIS=("arm64-v8a")

for ABI in "${ANDROID_ABIS[@]}"; do
    echo "Building for Android ABI: $ABI"
    
    ABI_BUILD_DIR="$BUILD_DIR/$ABI"
    rm -rf "$ABI_BUILD_DIR"
    mkdir -p "$ABI_BUILD_DIR"
    
    # Check if OpenSSL is available for this ABI
    OPENSSL_ABI_DIR="$OPENSSL_ROOT/$ABI"
    OPENSSL_FLAGS=""
    if [ -f "$OPENSSL_ABI_DIR/lib/libssl.a" ]; then
        echo "OpenSSL found for $ABI, enabling TLS support"
        OPENSSL_FLAGS="-DDCMTK_WITH_OPENSSL=ON -DOPENSSL_ROOT_DIR=$OPENSSL_ABI_DIR -DOPENSSL_INCLUDE_DIR=$OPENSSL_ABI_DIR/include -DOPENSSL_SSL_LIBRARY=$OPENSSL_ABI_DIR/lib/libssl.a -DOPENSSL_CRYPTO_LIBRARY=$OPENSSL_ABI_DIR/lib/libcrypto.a"
    else
        echo "OpenSSL not found for $ABI, building without TLS"
        OPENSSL_FLAGS="-DDCMTK_WITH_OPENSSL=OFF"
    fi
    
    cd "$ABI_BUILD_DIR"
    
    cmake -G "Unix Makefiles" \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="$ABI" \
        -DANDROID_PLATFORM=android-28 \
        -DANDROID_STL=c++_shared \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$ABI_BUILD_DIR/install" \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DCMAKE_C_FLAGS="-fPIC" \
        -DCMAKE_CXX_FLAGS="-fPIC" \
        -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
        -DANDROID_EMULATOR_PROGRAM="$ANDROID_SDK_ROOT/emulator/emulator" \
        -DANDROID_ANDROID_PROGRAM="/tmp/android_dummy.sh" \
        -DANDROID_ADB_PROGRAM="$ANDROID_SDK_ROOT/platform-tools/adb" \
        -DHAVE_GETLOGIN_R=0 \
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
        -DBUILD_TESTING=OFF \
        -DDCMTK_WITH_DOXYGEN=OFF \
        -Wno-dev \
        "$PROJECT_ROOT"
    
    # Build the libraries
    make -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)
    make install
    
    # Copy the libraries to the Flutter plugin directory
    PLUGIN_JNI_DIR="$PROJECT_ROOT/flutter_plugin/android/src/main/jniLibs/$ABI"
    mkdir -p "$PLUGIN_JNI_DIR"
    find "$ABI_BUILD_DIR/install/lib" -name "*.a" -exec cp {} "$PLUGIN_JNI_DIR/" \;
    
    # Copy OpenSSL libraries if available
    if [ -f "$OPENSSL_ABI_DIR/lib/libssl.a" ]; then
        cp "$OPENSSL_ABI_DIR/lib/libssl.a" "$PLUGIN_JNI_DIR/"
        cp "$OPENSSL_ABI_DIR/lib/libcrypto.a" "$PLUGIN_JNI_DIR/"
        echo "OpenSSL libraries copied to $PLUGIN_JNI_DIR"
    fi
    
    # Copy headers (same across ABIs)
    INCLUDE_DIR="$PROJECT_ROOT/flutter_plugin/android/src/main/cpp/include"
    mkdir -p "$INCLUDE_DIR"
    cp -R "$ABI_BUILD_DIR/install/include/dcmtk" "$INCLUDE_DIR/"
    
    # Copy OpenSSL headers if available
    if [ -d "$OPENSSL_ABI_DIR/include/openssl" ]; then
        cp -R "$OPENSSL_ABI_DIR/include/openssl" "$INCLUDE_DIR/"
        echo "OpenSSL headers copied"
    fi
    
    echo "Completed build for $ABI"
done

echo "Android build completed successfully!"
echo "Libraries are available in: $PROJECT_ROOT/flutter_plugin/android/src/main/jniLibs/"
echo "Headers are available in: $PROJECT_ROOT/flutter_plugin/android/src/main/cpp/include/"