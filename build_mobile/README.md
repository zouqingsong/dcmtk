# Building DCMTK for Flutter

This guide will help you build DCMTK as a Flutter plugin for iOS and Android platforms.

## Prerequisites

### For iOS Building (macOS only):
- macOS 10.15 or later
- Xcode 12.0 or later
- Xcode Command Line Tools
- CMake 3.7 or later

### For Android Building:
- Android NDK r21 or later
- CMake 3.7 or later
- Android SDK

### General:
- Git
- Flutter SDK (2.5.0 or later)

## Setup

1. **Clone the DCMTK repository:**
   ```bash
   git clone https://github.com/zouqingsong/dcmtk.git
   cd dcmtk
   ```

2. **Set environment variables for Android (if building for Android):**
   ```bash
   export ANDROID_NDK_ROOT=/path/to/your/android-ndk-r25b
   ```

## Building

### Option 1: Build Everything (Recommended)

```bash
./build_mobile/scripts/build_all.sh --platform all
```

### Option 2: Build for Specific Platform

**iOS only (macOS required):**
```bash
./build_mobile/scripts/build_all.sh --platform ios
```

**Android only:**
```bash
./build_mobile/scripts/build_all.sh --platform android
```

### Option 3: Clean Build

```bash
./build_mobile/scripts/build_all.sh --clean --platform all
```

## Build Output

After successful build, you'll find:

### iOS:
- Universal static library: `flutter_plugin/ios/Frameworks/libdcmtk.a`
- Headers: `flutter_plugin/ios/Frameworks/Headers/`

### Android:
- Static libraries for each ABI:
  - `flutter_plugin/android/src/main/jniLibs/arm64-v8a/`
  - `flutter_plugin/android/src/main/jniLibs/armeabi-v7a/`
  - `flutter_plugin/android/src/main/jniLibs/x86_64/`

## Using in Flutter Project

1. **Copy the plugin to your Flutter project:**
   ```bash
   cp -r flutter_plugin /path/to/your/flutter/project/packages/dcmtk_flutter
   ```

2. **Add dependency to your `pubspec.yaml`:**
   ```yaml
   dependencies:
     dcmtk_flutter:
       path: packages/dcmtk_flutter
   ```

3. **Use in your Dart code:**
   ```dart
   import 'package:dcmtk_flutter/dcmtk_flutter.dart';
   
   final dcmtk = DcmtkFlutter();
   final result = await dcmtk.loadDicomFile('/path/to/file.dcm');
   ```

## Configuration Options

The build scripts automatically configure DCMTK for mobile use with these settings:

- **Disabled for mobile compatibility:**
  - TIFF support
  - PNG support  
  - XML support
  - OpenSSL support
  - ICONV support
  - OpenJPEG support
  - Applications and tools

- **Enabled:**
  - ZLIB support
  - Multi-threading
  - Static linking

## Troubleshooting

### iOS Build Issues:

1. **"iOS SDK not found":**
   ```bash
   sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
   ```

2. **"cmake command not found":**
   ```bash
   brew install cmake
   ```

### Android Build Issues:

1. **"ANDROID_NDK_ROOT not set":**
   ```bash
   export ANDROID_NDK_ROOT=/path/to/android-ndk
   ```

2. **CMake version issues:**
   - Ensure you have CMake 3.7 or later
   - Update Android NDK to r21 or later

### General Issues:

1. **Permission denied:**
   ```bash
   chmod +x build_mobile/scripts/*.sh
   ```

2. **Out of disk space:**
   - The build requires several GB of disk space
   - Clean previous builds with `--clean` flag

## Advanced Configuration

### Custom DCMTK Options

Edit the toolchain files to customize DCMTK options:
- `build_mobile/toolchains/ios.cmake`
- `build_mobile/toolchains/android.cmake`

### Adding Dependencies

To add additional dependencies, modify:
1. The wrapper functions in `flutter_plugin/native/`
2. The CMake configuration in toolchain files
3. The platform-specific build configurations

## Example Project

See `flutter_plugin/example/` for a complete Flutter app demonstrating DCMTK usage.

To run the example:
```bash
cd flutter_plugin/example
flutter pub get
flutter run
```

## API Reference

### DcmtkFlutter Class

```dart
class DcmtkFlutter {
  /// Load and parse a DICOM file
  Future<String> loadDicomFile(String filePath);
  
  /// Get specific DICOM tag value (not yet implemented)
  Future<String?> getDicomTag(String filePath, String tagName);
  
  /// Convert DICOM to image format (not yet implemented)
  Future<List<int>?> convertToImage(String filePath, String format);
}
```

## Contributing

1. Fork the repository
2. Create your feature branch
3. Test your changes on both iOS and Android
4. Submit a pull request

## License

This project follows the DCMTK license. See the main repository for details.