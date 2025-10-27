#!/bin/bash
set -e

# Main build script for DCMTK Flutter plugin
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"

echo "=========================================="
echo "Building DCMTK Flutter Plugin"
echo "=========================================="
echo "Project root: $PROJECT_ROOT"

# Function to display usage
usage() {
    echo "Usage: $0 [OPTIONS]"
    echo "Options:"
    echo "  -p, --platform PLATFORM    Build for specific platform (ios, android, all)"
    echo "  -c, --clean                 Clean build directories before building"
    echo "  -h, --help                  Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 --platform ios           Build for iOS only"
    echo "  $0 --platform android       Build for Android only"
    echo "  $0 --platform all           Build for both platforms"
    echo "  $0 --clean --platform all   Clean and build for both platforms"
}

# Default values
PLATFORM="all"
CLEAN=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--platform)
            PLATFORM="$2"
            shift 2
            ;;
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# Validate platform
if [[ "$PLATFORM" != "ios" && "$PLATFORM" != "android" && "$PLATFORM" != "all" ]]; then
    echo "Error: Invalid platform '$PLATFORM'. Must be 'ios', 'android', or 'all'"
    exit 1
fi

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo "Cleaning build directories..."
    rm -rf "$PROJECT_ROOT/build_mobile/ios"
    rm -rf "$PROJECT_ROOT/build_mobile/android"
    rm -rf "$PROJECT_ROOT/flutter_plugin/ios/Frameworks"
    rm -rf "$PROJECT_ROOT/flutter_plugin/android/src/main/jniLibs"
    echo "Clean completed"
fi

# Build for requested platforms
if [[ "$PLATFORM" == "ios" || "$PLATFORM" == "all" ]]; then
    echo ""
    echo "=========================================="
    echo "Building for iOS"
    echo "=========================================="
    
    if [[ "$OSTYPE" != "darwin"* ]]; then
        echo "Warning: iOS build requires macOS. Skipping iOS build."
    else
        "$SCRIPT_DIR/build_ios.sh"
        echo "iOS build completed successfully!"
    fi
fi

if [[ "$PLATFORM" == "android" || "$PLATFORM" == "all" ]]; then
    echo ""
    echo "=========================================="
    echo "Building for Android"
    echo "=========================================="
    
    "$SCRIPT_DIR/build_android.sh"
    echo "Android build completed successfully!"
fi

echo ""
echo "=========================================="
echo "Build Summary"
echo "=========================================="

if [[ "$PLATFORM" == "ios" || "$PLATFORM" == "all" ]]; then
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo "iOS libraries:"
        if [ -d "$PROJECT_ROOT/flutter_plugin/ios/Frameworks" ]; then
            ls -la "$PROJECT_ROOT/flutter_plugin/ios/Frameworks"
        else
            echo "  No iOS libraries found"
        fi
    fi
fi

if [[ "$PLATFORM" == "android" || "$PLATFORM" == "all" ]]; then
    echo "Android libraries:"
    if [ -d "$PROJECT_ROOT/flutter_plugin/android/src/main/jniLibs" ]; then
        find "$PROJECT_ROOT/flutter_plugin/android/src/main/jniLibs" -name "*.a" -o -name "*.so"
    else
        echo "  No Android libraries found"
    fi
fi

echo ""
echo "=========================================="
echo "Next Steps"
echo "=========================================="
echo "1. Copy the flutter_plugin directory to your Flutter project"
echo "2. Add dcmtk_flutter dependency to your pubspec.yaml"
echo "3. Run 'flutter packages get'"
echo "4. Use the library in your Dart code:"
echo ""
echo "   import 'package:dcmtk_flutter/dcmtk_flutter.dart';"
echo "   final dcmtk = DcmtkFlutter();"
echo "   final result = await dcmtk.loadDicomFile('/path/to/file.dcm');"
echo ""
echo "Build completed successfully!"