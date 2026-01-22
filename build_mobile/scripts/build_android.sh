#!/bin/bash
set -e

# Build DCMTK for Android
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
BUILD_DIR="$PROJECT_ROOT/build_mobile/android"
TOOLCHAIN_FILE="$PROJECT_ROOT/build_mobile/toolchains/android.cmake"

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

# Android ABIs to build for
ANDROID_ABIS=("arm64-v8a" "armeabi-v7a" "x86_64")

for ABI in "${ANDROID_ABIS[@]}"; do
    echo "Building for Android ABI: $ABI"
    
    ABI_BUILD_DIR="$BUILD_DIR/$ABI"
    mkdir -p "$ABI_BUILD_DIR"
    
    cd "$ABI_BUILD_DIR"
    
    cmake -G "Unix Makefiles" \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
        -DANDROID_ABI="$ABI" \
        -DANDROID_NDK="$ANDROID_NDK_ROOT" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$ABI_BUILD_DIR/install" \
        -DANDROID_EMULATOR_PROGRAM="$ANDROID_SDK_ROOT/emulator/emulator" \
        -DANDROID_ANDROID_PROGRAM="/tmp/android_dummy.sh" \
        -DANDROID_ADB_PROGRAM="$ANDROID_SDK_ROOT/platform-tools/adb" \
        -DDCMTK_FORCE_FPIC_ON_UNIX=ON \
        -DCMAKE_CROSSCOMPILING_EMULATOR="" \
        -Wno-dev \
        "$PROJECT_ROOT"
    
    # Build the libraries
    make -j$(nproc)
    make install
    
    # Copy the libraries to the Flutter plugin directory
    PLUGIN_JNI_DIR="$PROJECT_ROOT/flutter_plugin/android/src/main/jniLibs/$ABI"
    mkdir -p "$PLUGIN_JNI_DIR"
    
    # Find all the built libraries and copy them
    find "$ABI_BUILD_DIR/install/lib" -name "*.a" -exec cp {} "$PLUGIN_JNI_DIR/" \;
    
    echo "Completed build for $ABI"
done

# Create a combined native library for JNI
echo "Creating combined native library..."

# This will be implemented in a separate C++ wrapper
JNI_SRC_DIR="$PROJECT_ROOT/flutter_plugin/android/src/main/cpp"
mkdir -p "$JNI_SRC_DIR"

cat > "$JNI_SRC_DIR/CMakeLists.txt" << 'EOF'
cmake_minimum_required(VERSION 3.10.2)
project("dcmtk_flutter")

# Find the Flutter library
find_library(log-lib log)

# Add the DCMTK libraries
set(DCMTK_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}")

# Create the shared library
add_library(dcmtk_flutter SHARED
    dcmtk_flutter_jni.cpp)

# Link the libraries
target_link_libraries(dcmtk_flutter
    ${log-lib}
    ${DCMTK_LIB_DIR}/libdcmdata.a
    ${DCMTK_LIB_DIR}/libofstd.a
    ${DCMTK_LIB_DIR}/liboflog.a
    z)
EOF

echo "Android build completed successfully!"
echo "Libraries are available in: $PROJECT_ROOT/flutter_plugin/android/src/main/jniLibs/"