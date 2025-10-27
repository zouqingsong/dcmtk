#!/bin/bash

# Test script to validate DCMTK Flutter plugin setup
echo "=========================================="
echo "DCMTK Flutter Plugin Validation"
echo "=========================================="

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$PROJECT_ROOT"

echo "Project root: $PROJECT_ROOT"
echo ""

# Check if required files exist
echo "Checking project structure..."

REQUIRED_FILES=(
    "flutter_plugin/pubspec.yaml"
    "flutter_plugin/lib/dcmtk_flutter.dart"
    "flutter_plugin/ios/dcmtk_flutter.podspec"
    "flutter_plugin/android/build.gradle"
    "build_mobile/scripts/build_all.sh"
    "build_mobile/toolchains/ios.cmake"
    "build_mobile/toolchains/android.cmake"
    "flutter_plugin/native/dcmtk_flutter_wrapper.h"
    "flutter_plugin/native/dcmtk_flutter_wrapper.cpp"
)

MISSING_FILES=()

for file in "${REQUIRED_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo "✓ $file"
    else
        echo "✗ $file (MISSING)"
        MISSING_FILES+=("$file")
    fi
done

if [ ${#MISSING_FILES[@]} -gt 0 ]; then
    echo ""
    echo "❌ Missing required files:"
    for file in "${MISSING_FILES[@]}"; do
        echo "   $file"
    done
    echo ""
    echo "Please ensure all required files are present before building."
    exit 1
fi

echo ""
echo "✅ All required files are present!"

# Check build script permissions
echo ""
echo "Checking build script permissions..."
if [ -x "build_mobile/scripts/build_all.sh" ]; then
    echo "✓ Build script is executable"
else
    echo "✗ Build script is not executable"
    echo "  Run: chmod +x build_mobile/scripts/*.sh"
fi

# Check for required tools
echo ""
echo "Checking required tools..."

# Check cmake
if command -v cmake &> /dev/null; then
    CMAKE_VERSION=$(cmake --version | head -n 1)
    echo "✓ $CMAKE_VERSION"
else
    echo "✗ CMake not found"
    echo "  Install with: brew install cmake (macOS) or apt install cmake (Linux)"
fi

# Check git
if command -v git &> /dev/null; then
    echo "✓ Git is available"
else
    echo "✗ Git not found"
fi

# Platform-specific checks
echo ""
echo "Platform-specific checks..."

# macOS/iOS checks
if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "✓ macOS detected - iOS building supported"
    
    if command -v xcodebuild &> /dev/null; then
        XCODE_VERSION=$(xcodebuild -version | head -n 1)
        echo "✓ $XCODE_VERSION"
    else
        echo "✗ Xcode not found"
        echo "  Install Xcode from the App Store"
    fi
else
    echo "⚠ Not macOS - iOS building not supported on this platform"
fi

# Android NDK check
if [ -n "$ANDROID_NDK_ROOT" ] && [ -d "$ANDROID_NDK_ROOT" ]; then
    echo "✓ Android NDK found: $ANDROID_NDK_ROOT"
else
    echo "⚠ ANDROID_NDK_ROOT not set or directory not found"
    echo "  Set with: export ANDROID_NDK_ROOT=/path/to/android-ndk"
fi

# Flutter check
if command -v flutter &> /dev/null; then
    FLUTTER_VERSION=$(flutter --version | head -n 1)
    echo "✓ $FLUTTER_VERSION"
else
    echo "⚠ Flutter not found (required for testing)"
    echo "  Install from: https://flutter.dev/docs/get-started/install"
fi

echo ""
echo "=========================================="
echo "Next Steps"
echo "=========================================="
echo ""
echo "To build DCMTK for Flutter:"
echo ""
echo "1. For both platforms:"
echo "   ./build_mobile/scripts/build_all.sh --platform all"
echo ""
echo "2. For iOS only (macOS required):"
echo "   ./build_mobile/scripts/build_all.sh --platform ios"
echo ""
echo "3. For Android only:"
echo "   ./build_mobile/scripts/build_all.sh --platform android"
echo ""
echo "For detailed instructions, see: build_mobile/README.md"
echo ""

if [ ${#MISSING_FILES[@]} -eq 0 ]; then
    echo "✅ Ready to build!"
else
    echo "❌ Please fix missing files before building."
    exit 1
fi