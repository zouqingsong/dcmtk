./build_mobile/scripts/build_all.sh --platform all#!/bin/bash
set -e

# Build DCMTK for iOS
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
BUILD_DIR="$PROJECT_ROOT/build_mobile/ios"
TOOLCHAIN_FILE="$PROJECT_ROOT/build_mobile/toolchains/ios.cmake"

echo "Building DCMTK for iOS..."
echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"

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
    
    cmake -G "Unix Makefiles" \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
        -DIOS_PLATFORM="$PLATFORM" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$PLATFORM_BUILD_DIR/install" \
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

# Create universal libraries using lipo
echo "Creating universal libraries..."

PLUGIN_FRAMEWORK_DIR="$PROJECT_ROOT/flutter_plugin/ios/Frameworks"
mkdir -p "$PLUGIN_FRAMEWORK_DIR"

# Get list of all static libraries
DEVICE_LIB_DIR="${DEVICE_LIBS[0]}"
SIMULATOR_LIB_DIR="${SIMULATOR_LIBS[0]}"

# Create universal binaries for each library
for lib_file in "$DEVICE_LIB_DIR"/*.a; do
    lib_name=$(basename "$lib_file")
    simulator_lib="$SIMULATOR_LIB_DIR/$lib_name"
    
    if [ -f "$simulator_lib" ]; then
        echo "Creating universal binary for $lib_name"
        lipo -create "$lib_file" "$simulator_lib" -output "$COMBINED_LIB_DIR/$lib_name"
    else
        echo "Warning: $lib_name not found in simulator build, copying device version only"
        cp "$lib_file" "$COMBINED_LIB_DIR/$lib_name"
    fi
done

# Create a single combined library using libtool (proper way for macOS)
echo "Creating combined DCMTK library..."
cd "$COMBINED_LIB_DIR"

# Collect all library files
LIB_FILES=()
for lib_file in *.a; do
    if [ -f "$lib_file" ]; then
        LIB_FILES+=("$lib_file")
    fi
done

# Use libtool to combine all static libraries into one
echo "Combining ${#LIB_FILES[@]} libraries using libtool..."
libtool -static -o "$PLUGIN_FRAMEWORK_DIR/libdcmtk.a" "${LIB_FILES[@]}"

echo "iOS build completed successfully!"
echo "Universal library is available at: $PLUGIN_FRAMEWORK_DIR/libdcmtk.a"

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