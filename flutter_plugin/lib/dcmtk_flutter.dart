import 'dart:ffi';
import 'dart:io';
import 'package:ffi/ffi.dart';
import 'package:flutter/services.dart';

// Native function signatures (for Android FFI)
typedef _LoadDicomFileNative = Pointer<Utf8> Function(Pointer<Utf8> filePath);
typedef _LoadDicomFile = Pointer<Utf8> Function(Pointer<Utf8> filePath);

typedef _FreeStringNative = Void Function(Pointer<Utf8> str);
typedef _FreeString = void Function(Pointer<Utf8> str);

class DcmtkFlutter {
  static DcmtkFlutter? _instance;
  late DynamicLibrary? _dylib;
  
  // Function pointers (for Android FFI)
  late _LoadDicomFile? _loadDicomFile;
  late _FreeString? _freeString;
  
  // Method channel (for iOS)
  static const MethodChannel _methodChannel = MethodChannel('dcmtk_flutter');

  DcmtkFlutter._internal() {
    if (Platform.isAndroid) {
      _dylib = _loadLibrary();
      _loadDicomFile = _dylib!
          .lookup<NativeFunction<_LoadDicomFileNative>>('dcmtk_load_dicom_file')
          .asFunction();
      _freeString = _dylib!
          .lookup<NativeFunction<_FreeStringNative>>('dcmtk_free_string')
          .asFunction();
    } else if (Platform.isIOS) {
      // iOS uses MethodChannel instead of FFI
      _dylib = null;
      _loadDicomFile = null;
      _freeString = null;
    }
  }

  factory DcmtkFlutter() {
    _instance ??= DcmtkFlutter._internal();
    return _instance!;
  }

  DynamicLibrary? _loadLibrary() {
    if (Platform.isAndroid) {
      return DynamicLibrary.open('libdcmtk_flutter.so');
    }
    return null;
  }

  /// Load and parse a DICOM file
  /// Returns string with DICOM file information
  Future<String> loadDicomFile(String filePath) async {
    if (Platform.isIOS) {
      // Use MethodChannel for iOS
      try {
        final String result = await _methodChannel.invokeMethod('loadDicomFile', {
          'filePath': filePath,
        });
        return result;
      } on PlatformException catch (e) {
        return "Error: ${e.message}";
      }
    } else if (Platform.isAndroid) {
      // Use FFI for Android
      final filePathPtr = filePath.toNativeUtf8();
      
      try {
        final resultPtr = _loadDicomFile!(filePathPtr);
        final result = resultPtr.toDartString();
        _freeString!(resultPtr);
        return result;
      } finally {
        malloc.free(filePathPtr);
      }
    } else {
      return "Error: Unsupported platform";
    }
  }
}
