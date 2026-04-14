#!/bin/bash
set -e

# Build DCMTK for iOS
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
BUILD_DIR="$PROJECT_ROOT/build_mobile/ios"
TOOLCHAIN_FILE="$PROJECT_ROOT/build_mobile/toolchains/ios.cmake"

echo "Building DCMTK for iOS..."
echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"

# Build OpenSSL first if not already built
OPENSSL_BASE_DIR="$BUILD_DIR/openssl/install"
if [ ! -f "$OPENSSL_BASE_DIR/device/lib/libssl.a" ]; then
    echo "Building OpenSSL for iOS..."
    bash "$SCRIPT_DIR/build_openssl_ios.sh"
fi

HAS_OPENSSL=0
if [ -f "$OPENSSL_BASE_DIR/device/lib/libssl.a" ] && [ -f "$OPENSSL_BASE_DIR/simulator/lib/libssl.a" ]; then
    HAS_OPENSSL=1
    echo "OpenSSL found, TLS support will be enabled"
else
    echo "OpenSSL not found, building without TLS support"
fi

# Check if we're on macOS
if [[ "$OSTYPE" != "darwin"* ]]; then
    echo "Error: iOS build can only be performed on macOS"
    exit 1
fi

# Check if Xcode is installed
if ! command -v xcodebuild &> /dev/null; then
    echo "Error: Xcode is not installed or not in PATH"
    exit 1
fi

# iOS platforms to build for
IOS_PLATFORMS=("OS" "SIMULATOR")
IOS_ARCHS=("arm64" "x86_64")

COMBINED_LIB_DIR="$BUILD_DIR/combined"
mkdir -p "$COMBINED_LIB_DIR"

DEVICE_LIBS=()
SIMULATOR_LIBS=()

for i in "${!IOS_PLATFORMS[@]}"; do
    PLATFORM="${IOS_PLATFORMS[$i]}"
    ARCH="${IOS_ARCHS[$i]}"
    
    echo "Building for iOS platform: $PLATFORM ($ARCH)"
    
    PLATFORM_BUILD_DIR="$BUILD_DIR/$PLATFORM"
    mkdir -p "$PLATFORM_BUILD_DIR"
    
    cd "$PLATFORM_BUILD_DIR"
    
    # Determine OpenSSL root for this platform
    OPENSSL_CMAKE_FLAGS=""
    if [ "$HAS_OPENSSL" -eq 1 ]; then
        if [ "$PLATFORM" = "OS" ]; then
            OPENSSL_ROOT="$OPENSSL_BASE_DIR/device"
        else
            OPENSSL_ROOT="$OPENSSL_BASE_DIR/simulator"
        fi
        OPENSSL_CMAKE_FLAGS="-DOPENSSL_ROOT_DIR=$OPENSSL_ROOT"
    fi

    cmake -G "Unix Makefiles" \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
        -DIOS_PLATFORM="$PLATFORM" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$PLATFORM_BUILD_DIR/install" \
        -DDCMTK_DEFAULT_DICT=builtin \
        $OPENSSL_CMAKE_FLAGS \
        "$PROJECT_ROOT"
    
    # Build the libraries
    make -j$(sysctl -n hw.ncpu)
    make install
    
    # Collect library paths for universal binary creation
    LIB_DIR="$PLATFORM_BUILD_DIR/install/lib"
    if [ "$PLATFORM" = "OS" ]; then
        DEVICE_LIBS+=("$LIB_DIR")
    else
        SIMULATOR_LIBS+=("$LIB_DIR")
    fi
    
    echo "Completed build for $PLATFORM"
done

# Skip universal binary creation since both device and simulator are arm64
echo "Skipping universal binary creation (both device and simulator are arm64)"

PLUGIN_FRAMEWORK_DIR="$PROJECT_ROOT/flutter_plugin/ios/Frameworks"
mkdir -p "$PLUGIN_FRAMEWORK_DIR"

# Get list of all static libraries
DEVICE_LIB_DIR="${DEVICE_LIBS[0]}"
SIMULATOR_LIB_DIR="${SIMULATOR_LIBS[0]}"

# Build the Flutter wrapper
echo "Building Flutter wrapper..."
WRAPPER_DIR="$PROJECT_ROOT/flutter_plugin/ios/native"
WRAPPER_SRC="$WRAPPER_DIR/dcmtk_flutter_wrapper.cpp"
WRAPPER_OBJ_DIR="$BUILD_DIR/wrapper_obj"
mkdir -p "$WRAPPER_OBJ_DIR"

# Build for each architecture
for PLATFORM in "OS" "SIMULATOR64"; do
    # Map SIMULATOR64 to SIMULATOR for directory paths
    if [ "$PLATFORM" == "SIMULATOR64" ]; then
        BUILD_PLATFORM="SIMULATOR"
    else
        BUILD_PLATFORM="$PLATFORM"
    fi
    
    ARCH_BUILD_DIR="$BUILD_DIR/$BUILD_PLATFORM"
    
    if [ "$PLATFORM" == "OS" ]; then
        SDK="iphoneos"
        ARCH_FLAGS="-arch arm64"
    else
        SDK="iphonesimulator"
        ARCH_FLAGS="-arch arm64"  # Only arm64 for Apple Silicon Macs
    fi
    
    WRAPPER_OBJ="$WRAPPER_OBJ_DIR/dcmtk_flutter_wrapper_${PLATFORM}.o"
    
    # Set minimum OS version flag based on platform
    if [ "$PLATFORM" == "OS" ]; then
        MIN_OS_FLAG="-mios-version-min=11.0"
    else
        MIN_OS_FLAG="-mios-simulator-version-min=11.0"
    fi
    
    # OpenSSL include path for wrapper (WITH_OPENSSL is already defined in osconfig.h)
    OPENSSL_WRAPPER_FLAGS=""
    if [ "$HAS_OPENSSL" -eq 1 ]; then
        if [ "$PLATFORM" == "OS" ]; then
            OPENSSL_WRAPPER_FLAGS="-I$OPENSSL_BASE_DIR/device/include"
        else
            OPENSSL_WRAPPER_FLAGS="-I$OPENSSL_BASE_DIR/simulator/include"
        fi
    fi

    echo "Compiling wrapper for $PLATFORM..."
    echo "SDK: $SDK, Min OS flag: $MIN_OS_FLAG"
    
    xcrun clang++ \
        -x c++ \
        -std=c++11 \
        -stdlib=libc++ \
        $ARCH_FLAGS \
        -isysroot $(xcrun --sdk $SDK --show-sdk-path) \
        $MIN_OS_FLAG \
        -I"$ARCH_BUILD_DIR/config/include" \
        -I"$PROJECT_ROOT/ofstd/include" \
        -I"$PROJECT_ROOT/oflog/include" \
        -I"$PROJECT_ROOT/dcmdata/include" \
        -I"$PROJECT_ROOT/dcmimgle/include" \
        -I"$PROJECT_ROOT/dcmimage/include" \
        -I"$PROJECT_ROOT/dcmjpeg/include" \
        -I"$PROJECT_ROOT/dcmjpls/include" \
        -I"$PROJECT_ROOT/dcmnet/include" \
        -I"$PROJECT_ROOT/dcmtls/include" \
        -I"$WRAPPER_DIR" \
        -DHAVE_CONFIG_H \
        $OPENSSL_WRAPPER_FLAGS \
        -c "$WRAPPER_SRC" \
        -o "$WRAPPER_OBJ"
    
    # Create a static library from the object file
    WRAPPER_LIB="$WRAPPER_OBJ_DIR/libdcmtk_flutter_wrapper_${PLATFORM}.a"
    libtool -static -o "$WRAPPER_LIB" "$WRAPPER_OBJ"
done

# Create separate device and simulator libraries
echo "Creating device library..."
DEVICE_LIB_DIR="$BUILD_DIR/device_lib"
DEVICE_OBJ_DIR="$BUILD_DIR/device_obj"
rm -rf "$DEVICE_LIB_DIR" "$DEVICE_OBJ_DIR"
mkdir -p "$DEVICE_LIB_DIR" "$DEVICE_OBJ_DIR"

# Extract all object files from device libraries, prefixing with library name to avoid collisions
cd "$DEVICE_OBJ_DIR"
for lib in "$BUILD_DIR/OS/install/lib"/*.a; do
    LIB_BASE="$(basename "$lib" .a)"
    LIB_OBJ_DIR="$DEVICE_OBJ_DIR/${LIB_BASE}_objs"
    mkdir -p "$LIB_OBJ_DIR"
    echo "Extracting $(basename $lib)..."
    cd "$LIB_OBJ_DIR"
    ar -x "$lib"
    # Prefix each .o with the library name to avoid name collisions
    for obj in *.o; do
        mv "$obj" "$DEVICE_OBJ_DIR/${LIB_BASE}_${obj}"
    done
    cd "$DEVICE_OBJ_DIR"
    rm -rf "$LIB_OBJ_DIR"
done
# Extract wrapper (it's already thin arm64, not a fat archive)
ar -x "$WRAPPER_OBJ_DIR/libdcmtk_flutter_wrapper_OS.a"

# Extract OpenSSL libraries if available
if [ "$HAS_OPENSSL" -eq 1 ]; then
    for lib in "$OPENSSL_BASE_DIR/device/lib"/*.a; do
        LIB_BASE="$(basename "$lib" .a)"
        LIB_OBJ_DIR="$DEVICE_OBJ_DIR/${LIB_BASE}_objs"
        mkdir -p "$LIB_OBJ_DIR"
        echo "Extracting $(basename $lib) (OpenSSL)..."
        cd "$LIB_OBJ_DIR"
        ar -x "$lib"
        for obj in *.o; do
            mv "$obj" "$DEVICE_OBJ_DIR/openssl_${LIB_BASE}_${obj}"
        done
        cd "$DEVICE_OBJ_DIR"
        rm -rf "$LIB_OBJ_DIR"
    done
fi

# Create combined device library
echo "Combining device objects..."
ar -rcs "$DEVICE_LIB_DIR/libdcmtk.a" *.o

echo "Creating simulator library..."
SIMULATOR_LIB_DIR="$BUILD_DIR/simulator_lib"
SIMULATOR_OBJ_DIR="$BUILD_DIR/simulator_obj"
rm -rf "$SIMULATOR_LIB_DIR" "$SIMULATOR_OBJ_DIR"
mkdir -p "$SIMULATOR_LIB_DIR" "$SIMULATOR_OBJ_DIR"

# Extract all object files from simulator libraries (arm64 only for Apple Silicon)
cd "$SIMULATOR_OBJ_DIR"
for lib in "$BUILD_DIR/SIMULATOR/install/lib"/*.a; do
    LIB_BASE="$(basename "$lib" .a)"
    LIB_OBJ_DIR="$SIMULATOR_OBJ_DIR/${LIB_BASE}_objs"
    mkdir -p "$LIB_OBJ_DIR"
    echo "Extracting $(basename $lib)..."
    cd "$LIB_OBJ_DIR"
    ar -x "$lib"
    # Prefix each .o with the library name to avoid name collisions
    for obj in *.o; do
        mv "$obj" "$SIMULATOR_OBJ_DIR/${LIB_BASE}_${obj}"
    done
    cd "$SIMULATOR_OBJ_DIR"
    rm -rf "$LIB_OBJ_DIR"
done

# Extract wrapper (already arm64-only, no need for lipo)
ar -x "$WRAPPER_OBJ_DIR/libdcmtk_flutter_wrapper_SIMULATOR64.a"

# Extract OpenSSL libraries if available
if [ "$HAS_OPENSSL" -eq 1 ]; then
    for lib in "$OPENSSL_BASE_DIR/simulator/lib"/*.a; do
        LIB_BASE="$(basename "$lib" .a)"
        LIB_OBJ_DIR="$SIMULATOR_OBJ_DIR/${LIB_BASE}_objs"
        mkdir -p "$LIB_OBJ_DIR"
        echo "Extracting $(basename $lib) (OpenSSL)..."
        cd "$LIB_OBJ_DIR"
        ar -x "$lib"
        for obj in *.o; do
            # Fix platform tag: OpenSSL simulator objects have LC_VERSION_MIN_IPHONEOS
            # which is wrong for simulator. Rewrite to iossim platform.
            vtool -set-build-version iossim 14.0 26.4 -replace -output "$SIMULATOR_OBJ_DIR/openssl_${LIB_BASE}_${obj}" "$obj" 2>/dev/null || \
                mv "$obj" "$SIMULATOR_OBJ_DIR/openssl_${LIB_BASE}_${obj}"
        done
        cd "$SIMULATOR_OBJ_DIR"
        rm -rf "$LIB_OBJ_DIR"
    done
fi

# Create combined simulator library
echo "Combining simulator objects..."
ar -rcs "$SIMULATOR_LIB_DIR/libdcmtk.a" *.o

# Create XCFramework
echo "Creating XCFramework..."
XCFRAMEWORK_DIR="$PLUGIN_FRAMEWORK_DIR"
rm -rf "$XCFRAMEWORK_DIR/dcmtk.xcframework"

xcodebuild -create-xcframework \
    -library "$DEVICE_LIB_DIR/libdcmtk.a" \
    -headers "$BUILD_DIR/OS/install/include" \
    -library "$SIMULATOR_LIB_DIR/libdcmtk.a" \
    -headers "$BUILD_DIR/SIMULATOR/install/include" \
    -output "$XCFRAMEWORK_DIR/dcmtk.xcframework"

echo "iOS build completed successfully!"
echo "XCFramework is available at: $XCFRAMEWORK_DIR/dcmtk.xcframework"
echo ""
echo "Device library size: $(du -h $DEVICE_LIB_DIR/libdcmtk.a | cut -f1)"
echo "Simulator library size: $(du -h $SIMULATOR_LIB_DIR/libdcmtk.a | cut -f1)"

# Also create a legacy universal library for backwards compatibility (simulator-only for testing)
echo ""
echo "Creating legacy simulator-only library for testing..."
cp "$SIMULATOR_LIB_DIR/libdcmtk.a" "$PLUGIN_FRAMEWORK_DIR/libdcmtk.a"
echo "Legacy library (simulator-only) is available at: $PLUGIN_FRAMEWORK_DIR/libdcmtk.a"

# Copy headers
echo "Copying headers..."
HEADERS_DIR="$PLUGIN_FRAMEWORK_DIR/Headers"
mkdir -p "$HEADERS_DIR"

# Copy headers from device build (they should be the same for both platforms)
DEVICE_HEADERS_DIR="$BUILD_DIR/OS/install/include"
if [ -d "$DEVICE_HEADERS_DIR" ]; then
    cp -R "$DEVICE_HEADERS_DIR"/* "$HEADERS_DIR/"
    echo "Headers copied to: $HEADERS_DIR"
fi