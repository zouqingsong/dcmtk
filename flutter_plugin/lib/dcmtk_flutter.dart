import 'dart:ffi';
import 'dart:io';
import 'dart:typed_data';
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

  /// Extract image from DICOM file
  /// Returns Map with 'width', 'height', and 'data' (Uint8List of grayscale pixels)
  Future<Map<String, dynamic>?> extractImage(String filePath, {int frameIndex = 0}) async {
    if (Platform.isIOS) {
      try {
        final Map<dynamic, dynamic> result = await _methodChannel.invokeMethod('extractImage', {
          'filePath': filePath,
          'frameIndex': frameIndex,
        });
        return {
          'width': result['width'] as int,
          'height': result['height'] as int,
          'data': result['data'] as Uint8List,
        };
      } on PlatformException catch (e) {
        print("Error extracting image: ${e.message}");
        return null;
      }
    } else if (Platform.isAndroid) {
      // TODO: Implement Android FFI image extraction
      return null;
    } else {
      return null;
    }
  }

  /// Test connection to DICOM server
  /// Returns true if connection successful, false otherwise
  Future<bool> testServerConnection({
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
  }) async {
    if (Platform.isIOS) {
      try {
        final bool result = await _methodChannel.invokeMethod('testServerConnection', {
          'serverHost': serverHost,
          'serverPort': serverPort,
          'aeTitle': aeTitle,
          'calledAeTitle': calledAeTitle,
        });
        return result;
      } on PlatformException catch (e) {
        print("Error testing server connection: ${e.message}");
        return false;
      }
    } else if (Platform.isAndroid) {
      // TODO: Implement Android FFI server communication
      return false;
    } else {
      return false;
    }
  }

  /// Query patients from DICOM server
  /// Returns list of patient information maps
  Future<List<Map<String, dynamic>>> queryPatients({
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
  }) async {
    if (Platform.isIOS) {
      try {
        final List<dynamic> result = await _methodChannel.invokeMethod('queryPatients', {
          'serverHost': serverHost,
          'serverPort': serverPort,
          'aeTitle': aeTitle,
          'calledAeTitle': calledAeTitle,
        });
        
        // Convert each item to Map<String, dynamic> safely
        return result.map((item) {
          if (item is Map) {
            return Map<String, dynamic>.from(item);
          }
          return <String, dynamic>{};
        }).toList();
      } on PlatformException catch (e) {
        print("Error querying patients: ${e.message}");
        return [];
      }
    } else if (Platform.isAndroid) {
      // TODO: Implement Android FFI patient querying
      return [];
    } else {
      return [];
    }
  }

  /// Query studies for a specific patient from DICOM server
  /// Returns list of study information maps
  Future<List<Map<String, dynamic>>> queryStudiesForPatient({
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
    required String patientId,
  }) async {
    if (Platform.isIOS) {
      try {
        final List<dynamic> result = await _methodChannel.invokeMethod('queryStudiesForPatient', {
          'serverHost': serverHost,
          'serverPort': serverPort,
          'aeTitle': aeTitle,
          'calledAeTitle': calledAeTitle,
          'patientId': patientId,
        });
        
        // Convert each item to Map<String, dynamic> safely
        return result.map((item) {
          if (item is Map) {
            return Map<String, dynamic>.from(item);
          }
          return <String, dynamic>{};
        }).toList();
      } on PlatformException catch (e) {
        print("Error querying studies: ${e.message}");
        return [];
      }
    } else if (Platform.isAndroid) {
      // TODO: Implement Android FFI study querying
      return [];
    } else {
      return [];
    }
  }

  /// Query series for a specific study from DICOM server
  /// Returns list of series information maps
  Future<List<Map<String, dynamic>>> querySeriesForStudy({
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
    required String studyInstanceUID,
  }) async {
    if (Platform.isIOS) {
      try {
        final List<dynamic> result = await _methodChannel.invokeMethod('querySeriesForStudy', {
          'serverHost': serverHost,
          'serverPort': serverPort,
          'aeTitle': aeTitle,
          'calledAeTitle': calledAeTitle,
          'studyInstanceUID': studyInstanceUID,
        });
        
        // Convert each item to Map<String, dynamic> safely
        return result.map((item) {
          if (item is Map) {
            return Map<String, dynamic>.from(item);
          }
          return <String, dynamic>{};
        }).toList();
      } on PlatformException catch (e) {
        print("Error querying series: ${e.message}");
        return [];
      }
    } else if (Platform.isAndroid) {
      // TODO: Implement Android FFI series querying
      return [];
    } else {
      return [];
    }
  }

  /// Download instances from a series using C-MOVE (native DICOM protocol)
  /// Returns list of downloaded instance information
  Future<List<Map<String, dynamic>>> downloadInstancesViaCMove({
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
    required String seriesInstanceUID,
    required String localStoragePath,
  }) async {
    if (Platform.isIOS) {
      try {
        final List<dynamic> result = await _methodChannel.invokeMethod('downloadInstancesViaCMove', {
          'serverHost': serverHost,
          'serverPort': serverPort,
          'aeTitle': aeTitle,
          'calledAeTitle': calledAeTitle,
          'seriesInstanceUID': seriesInstanceUID,
          'localStoragePath': localStoragePath,
        });
        
        // Convert each item to Map<String, dynamic> safely
        return result.map((item) {
          if (item is Map) {
            return Map<String, dynamic>.from(item);
          }
          return <String, dynamic>{};
        }).toList();
      } on PlatformException catch (e) {
        print("Error downloading instances via C-MOVE: ${e.message}");
        return [];
      }
    } else if (Platform.isAndroid) {
      // TODO: Implement Android FFI C-MOVE download
      return [];
    } else {
      return [];
    }
  }

  /// Create a new patient on DICOM server
  /// Returns creation result with success status and generated patient ID
  Future<Map<String, dynamic>> createPatient({
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
    required String patientId,
    required String patientName,
    String birthDate = '',
    String sex = '',
    String comments = '',
  }) async {
    print('[Dart] createPatient called with: host=$serverHost, port=$serverPort, patientId=$patientId');
    if (Platform.isIOS) {
      try {
        print('[Dart] Calling iOS method channel for createPatient');
        final dynamic result = await _methodChannel.invokeMethod('createPatient', {
          'serverHost': serverHost,
          'serverPort': serverPort,
          'aeTitle': aeTitle,
          'calledAeTitle': calledAeTitle,
          'patientId': patientId,
          'patientName': patientName,
          'birthDate': birthDate,
          'sex': sex,
          'comments': comments,
        });
        print('[Dart] iOS method channel returned: $result');
        return Map<String, dynamic>.from(result as Map);
      } on PlatformException catch (e) {
        print("[Dart] Error creating patient: ${e.message}");
        return {'success': false, 'error': e.message};
      }
    } else if (Platform.isAndroid) {
      // TODO: Implement Android FFI patient creation
      return {'success': false, 'error': 'Not implemented for Android'};
    } else {
      return {'success': false, 'error': 'Platform not supported'};
    }
  }

  /// Upload image to DICOM server for a specific patient
  /// Returns upload result with generated UIDs
  Future<Map<String, dynamic>> uploadImage({
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
    required String patientId,
    required String imagePath,
    String studyDescription = 'Uploaded Image',
    String seriesDescription = 'Uploaded Series',
    String imageComments = '',
    String modality = 'SC',
  }) async {
    print('[Dart] uploadImage called with: host=$serverHost, port=$serverPort, patientId=$patientId, imagePath=$imagePath');
    if (Platform.isIOS) {
      try {
        print('[Dart] Calling iOS method channel for uploadImage');
        final dynamic result = await _methodChannel.invokeMethod('uploadImage', {
          'serverHost': serverHost,
          'serverPort': serverPort,
          'aeTitle': aeTitle,
          'calledAeTitle': calledAeTitle,
          'patientId': patientId,
          'imagePath': imagePath,
          'studyDescription': studyDescription,
          'seriesDescription': seriesDescription,
          'imageComments': imageComments,
          'modality': modality,
        });
        print('[Dart] iOS method channel returned: $result');
        return Map<String, dynamic>.from(result as Map);
      } on PlatformException catch (e) {
        print("[Dart] Error uploading image: ${e.message}");
        return {'success': false, 'error': e.message};
      }
    } else if (Platform.isAndroid) {
      // TODO: Implement Android FFI image upload
      return {'success': false, 'error': 'Not implemented for Android'};
    } else {
      return {'success': false, 'error': 'Platform not supported'};
    }
  }

  /// Upload video to DICOM server for a specific patient
  /// Returns upload result with generated UIDs
  Future<Map<String, dynamic>> uploadVideo({
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
    required String patientId,
    required String videoPath,
    String studyDescription = 'Uploaded Video',
    String seriesDescription = 'Uploaded Video Series',
    String imageComments = '',
    String modality = 'SC',
  }) async {
    if (Platform.isIOS) {
      try {
        // For now, treat video upload similar to image upload
        // In a real implementation, this would handle video-specific DICOM objects
        final dynamic result = await _methodChannel.invokeMethod('uploadImage', {
          'serverHost': serverHost,
          'serverPort': serverPort,
          'aeTitle': aeTitle,
          'calledAeTitle': calledAeTitle,
          'patientId': patientId,
          'imagePath': videoPath,  // Will be handled as media path
          'studyDescription': studyDescription,
          'seriesDescription': seriesDescription,
          'imageComments': imageComments,
          'modality': modality,
        });
        return Map<String, dynamic>.from(result as Map);
      } on PlatformException catch (e) {
        print("Error uploading video: ${e.message}");
        return {'success': false, 'error': e.message};
      }
    } else if (Platform.isAndroid) {
      // TODO: Implement Android FFI video upload
      return {'success': false, 'error': 'Not implemented for Android'};
    } else {
      return {'success': false, 'error': 'Platform not supported'};
    }
  }
}

class DicomPatient {
  final String patientId;
  final String patientName;
  final String patientBirthDate;
  final String patientSex;
  final int studyCount;

  DicomPatient({
    required this.patientId,
    required this.patientName,
    required this.patientBirthDate,
    required this.patientSex,
    required this.studyCount,
  });

  factory DicomPatient.fromMap(Map<String, dynamic> map) {
    return DicomPatient(
      patientId: map['patientId'] ?? '',
      patientName: map['patientName'] ?? '',
      patientBirthDate: map['patientBirthDate'] ?? '',
      patientSex: map['patientSex'] ?? '',
      studyCount: map['studyCount'] ?? 0,
    );
  }

  @override
  String toString() {
    return 'Patient: $patientName (ID: $patientId, DOB: $patientBirthDate, Sex: $patientSex)';
  }
}

class DicomStudy {
  final String studyInstanceUID;
  final String studyDate;
  final String studyTime;
  final String studyDescription;
  final String accessionNumber;
  final int seriesCount;

  DicomStudy({
    required this.studyInstanceUID,
    required this.studyDate,
    required this.studyTime,
    required this.studyDescription,
    required this.accessionNumber,
    required this.seriesCount,
  });

  factory DicomStudy.fromMap(Map<String, dynamic> map) {
    return DicomStudy(
      studyInstanceUID: map['studyInstanceUID'] ?? '',
      studyDate: map['studyDate'] ?? '',
      studyTime: map['studyTime'] ?? '',
      studyDescription: map['studyDescription'] ?? '',
      accessionNumber: map['accessionNumber'] ?? '',
      seriesCount: map['seriesCount'] ?? 0,
    );
  }

  @override
  String toString() {
    return 'Study: $studyDescription (Date: $studyDate, UID: $studyInstanceUID)';
  }
}

class DicomSeries {
  final String seriesInstanceUID;
  final String seriesNumber;
  final String seriesDescription;
  final String modality;
  final String seriesDate;
  final String seriesTime;
  final int instanceCount;

  DicomSeries({
    required this.seriesInstanceUID,
    required this.seriesNumber,
    required this.seriesDescription,
    required this.modality,
    required this.seriesDate,
    required this.seriesTime,
    required this.instanceCount,
  });

  factory DicomSeries.fromMap(Map<String, dynamic> map) {
    return DicomSeries(
      seriesInstanceUID: map['seriesInstanceUID'] ?? '',
      seriesNumber: map['seriesNumber'] ?? '',
      seriesDescription: map['seriesDescription'] ?? '',
      modality: map['modality'] ?? '',
      seriesDate: map['seriesDate'] ?? '',
      seriesTime: map['seriesTime'] ?? '',
      instanceCount: map['instanceCount'] ?? 0,
    );
  }

  @override
  String toString() {
    return 'Series: $seriesDescription ($modality, Number: $seriesNumber)';
  }
}
