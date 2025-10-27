# DCMTK Flutter Plugin

This Flutter plugin provides access to the DCMTK (DICOM toolkit) library for iOS and Android platforms.

## Features

- DICOM file reading and parsing
- Image conversion and processing
- Network operations (C-STORE, C-FIND, etc.)
- Cross-platform support for iOS and Android

## Installation

Add this to your package's `pubspec.yaml` file:

```yaml
dependencies:
  dcmtk_flutter: ^1.0.0
```

## Usage

```dart
import 'package:dcmtk_flutter/dcmtk_flutter.dart';

// Load and parse a DICOM file
final dcmtk = DcmtkFlutter();
final result = await dcmtk.loadDicomFile('/path/to/file.dcm');
```

## Building from Source

This plugin includes native DCMTK libraries. To build from source:

1. Clone the repository with DCMTK source
2. Run the build scripts in `build_mobile/scripts/`
3. Libraries will be automatically placed in the correct locations

## Platform Support

- iOS: arm64 (device), x86_64 (simulator)
- Android: arm64-v8a, armeabi-v7a, x86_64

## License

See the DCMTK license in the main repository.