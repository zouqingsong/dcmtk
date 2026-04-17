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
  /// Returns Map with 'width', 'height', 'data' (Uint8List of RGBA pixels),
  /// 'samplesPerPixel', 'bitsStored', 'totalFrames'
  /// On error, returns Map with 'error' key containing the error message.
  /// Optional [windowCenter] and [windowWidth] for custom window/level (grayscale only).
  /// If windowWidth is 0 or not provided, the default window from the DICOM file is used.
  Future<Map<String, dynamic>?> extractImage(String filePath, {int frameIndex = 0, double windowCenter = 0, double windowWidth = 0}) async {
    if (Platform.isIOS) {
      try {
        final Map<dynamic, dynamic> result = await _methodChannel.invokeMethod('extractImage', {
          'filePath': filePath,
          'frameIndex': frameIndex,
          'windowCenter': windowCenter,
          'windowWidth': windowWidth,
        });
        return {
          'width': result['width'] as int,
          'height': result['height'] as int,
          'data': result['data'] as Uint8List,
          'samplesPerPixel': result['samplesPerPixel'] as int? ?? 1,
          'bitsStored': result['bitsStored'] as int? ?? 8,
          'totalFrames': result['totalFrames'] as int? ?? 1,
        };
      } on PlatformException catch (e) {
        print("Error extracting image: ${e.message}");
        return {'error': e.message ?? 'Unknown extraction error'};
      }
    } else if (Platform.isAndroid) {
      // TODO: Implement Android FFI image extraction
      return null;
    } else {
      return null;
    }
  }

  /// Extract video payload from a DICOM video file to an output file
  /// Returns Map with 'outputPath', 'mimeType', 'fileSize', or null on error
  Future<Map<String, dynamic>?> extractVideo(String dicomPath, String outputPath) async {
    if (Platform.isIOS) {
      try {
        final Map<dynamic, dynamic> result = await _methodChannel.invokeMethod('extractVideo', {
          'dicomPath': dicomPath,
          'outputPath': outputPath,
        });
        return {
          'outputPath': result['outputPath'] as String,
          'mimeType': result['mimeType'] as String,
          'fileSize': result['fileSize'] as int,
        };
      } on PlatformException catch (e) {
        print("Error extracting video: ${e.message}");
        return null;
      }
    } else if (Platform.isAndroid) {
      // TODO: Implement Android video extraction
      return null;
    } else {
      return null;
    }
  }

  /// Get a specific DICOM tag value from a file
  /// tagName can be a group,element pair like "0010,0010" or a name like "PatientName"
  Future<String> getDicomTag(String filePath, String tagName) async {
    if (Platform.isIOS) {
      try {
        final String result = await _methodChannel.invokeMethod('getDicomTag', {
          'filePath': filePath,
          'tagName': tagName,
        });
        return result;
      } on PlatformException catch (e) {
        return 'Error: ${e.message}';
      }
    }
    return 'Error: Platform not supported';
  }

  /// Validate if a file is a valid DICOM file
  Future<bool> validateDicomFile(String filePath) async {
    if (Platform.isIOS) {
      try {
        final bool result = await _methodChannel.invokeMethod('validateDicomFile', {
          'filePath': filePath,
        });
        return result;
      } on PlatformException {
        return false;
      }
    }
    return false;
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

  /// Test TLS connection to DICOM server
  /// Returns true if connection successful, false otherwise
  /// Returns false and prints error if TLS is not available (OpenSSL not compiled in)
  Future<bool> testServerConnectionTls({
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
    String certFile = '',
    String keyFile = '',
    String caFile = '',
  }) async {
    if (Platform.isIOS) {
      try {
        final bool result = await _methodChannel.invokeMethod('testServerConnectionTls', {
          'serverHost': serverHost,
          'serverPort': serverPort,
          'aeTitle': aeTitle,
          'calledAeTitle': calledAeTitle,
          'certFile': certFile,
          'keyFile': keyFile,
          'caFile': caFile,
        });
        return result;
      } on PlatformException catch (e) {
        print("Error testing TLS connection: ${e.message}");
        return false;
      }
    }
    return false;
  }

  /// Query patients from DICOM server
  /// Returns list of patient information maps
  Future<List<Map<String, dynamic>>> queryPatients({
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
    String patientNameFilter = '',
  }) async {
    if (Platform.isIOS) {
      try {
        final List<dynamic> result = await _methodChannel.invokeMethod('queryPatients', {
          'serverHost': serverHost,
          'serverPort': serverPort,
          'aeTitle': aeTitle,
          'calledAeTitle': calledAeTitle,
          if (patientNameFilter.isNotEmpty) 'patientNameFilter': patientNameFilter,
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

  /// Query instances for a specific series from DICOM server
  Future<List<Map<String, dynamic>>> queryInstancesForSeries({
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
    required String seriesInstanceUID,
  }) async {
    if (Platform.isIOS) {
      try {
        final List<dynamic> result = await _methodChannel.invokeMethod('queryInstancesForSeries', {
          'serverHost': serverHost,
          'serverPort': serverPort,
          'aeTitle': aeTitle,
          'calledAeTitle': calledAeTitle,
          'seriesInstanceUID': seriesInstanceUID,
        });
        return result.map((item) {
          if (item is Map) return Map<String, dynamic>.from(item);
          return <String, dynamic>{};
        }).toList();
      } on PlatformException catch (e) {
        print("Error querying instances: ${e.message}");
        return [];
      }
    }
    return [];
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
        print("Error downloading instances via C-GET: ${e.message}");
        rethrow;
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
        final resultMap = Map<String, dynamic>.from(result as Map);
        if (resultMap['warning'] != null && (resultMap['warning'] as String).isNotEmpty) {
          print('[Dart] CREATE PATIENT WARNING: ${resultMap['warning']}');
        }
        if (resultMap['rspStatusCode'] != null) {
          print('[Dart] C-STORE response status: 0x${(resultMap['rspStatusCode'] as int).toRadixString(16).padLeft(4, '0').toUpperCase()}');
        }
        return resultMap;
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
    String patientName = '',
    String patientBirthDate = '',
    String studyDescription = 'Uploaded Image',
    String seriesDescription = 'Uploaded Series',
    String imageComments = '',
    String modality = 'SC',
    String studyInstanceUID = '',
    String seriesInstanceUID = '',
    int instanceNumber = 1,
  }) async {
    print('[Dart] uploadImage called with: host=$serverHost, port=$serverPort, patientId=$patientId, imagePath=$imagePath, instance#=$instanceNumber');
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
          'patientName': patientName,
          'patientBirthDate': patientBirthDate,
          'studyDescription': studyDescription,
          'seriesDescription': seriesDescription,
          'imageComments': imageComments,
          'modality': modality,
          'studyInstanceUID': studyInstanceUID,
          'seriesInstanceUID': seriesInstanceUID,
          'instanceNumber': instanceNumber,
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

  /// Upload multiple images as a single multi-frame DICOM instance
  /// Returns upload result with generated UIDs
  Future<Map<String, dynamic>> uploadMultiframe({
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
    required String patientId,
    required List<String> imagePaths,
    String patientName = '',
    String patientBirthDate = '',
    String studyDescription = 'Uploaded Image',
    String seriesDescription = 'Uploaded Series',
    String imageComments = '',
    String modality = 'SC',
    String studyInstanceUID = '',
    String seriesInstanceUID = '',
  }) async {
    print('[Dart] uploadMultiframe called with: host=$serverHost, port=$serverPort, patientId=$patientId, ${imagePaths.length} images');
    if (Platform.isIOS) {
      try {
        final dynamic result = await _methodChannel.invokeMethod('uploadMultiframe', {
          'serverHost': serverHost,
          'serverPort': serverPort,
          'aeTitle': aeTitle,
          'calledAeTitle': calledAeTitle,
          'patientId': patientId,
          'imagePaths': imagePaths,
          'patientName': patientName,
          'patientBirthDate': patientBirthDate,
          'studyDescription': studyDescription,
          'seriesDescription': seriesDescription,
          'imageComments': imageComments,
          'modality': modality,
          'studyInstanceUID': studyInstanceUID,
          'seriesInstanceUID': seriesInstanceUID,
        });
        print('[Dart] iOS method channel returned: $result');
        return Map<String, dynamic>.from(result as Map);
      } on PlatformException catch (e) {
        print("[Dart] Error uploading multiframe: ${e.message}");
        return {'success': false, 'error': e.message};
      }
    } else if (Platform.isAndroid) {
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
    String patientName = '',
    String patientBirthDate = '',
    String studyDescription = 'Uploaded Video',
    String seriesDescription = 'Uploaded Video Series',
    String imageComments = '',
    String modality = 'SC',
  }) async {
    if (Platform.isIOS) {
      try {
        final dynamic result = await _methodChannel.invokeMethod('uploadVideo', {
          'serverHost': serverHost,
          'serverPort': serverPort,
          'aeTitle': aeTitle,
          'calledAeTitle': calledAeTitle,
          'patientId': patientId,
          'videoPath': videoPath,
          'patientName': patientName,
          'patientBirthDate': patientBirthDate,
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

  // ============================================================
  // C-STORE SCU: Send existing DICOM files to a remote PACS
  // ============================================================
  Future<Map<String, dynamic>> storeFiles({
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
    required List<String> filePaths,
  }) async {
    if (Platform.isIOS) {
      try {
        final dynamic result = await _methodChannel.invokeMethod('storeFiles', {
          'serverHost': serverHost,
          'serverPort': serverPort,
          'aeTitle': aeTitle,
          'calledAeTitle': calledAeTitle,
          'filePaths': filePaths,
        });
        return Map<String, dynamic>.from(result as Map);
      } on PlatformException catch (e) {
        return {'error': 1, 'errorMessage': e.message};
      }
    }
    return {'error': 1, 'errorMessage': 'Platform not supported'};
  }

  // ============================================================
  // C-STORE SCP: Receive DICOM files from remote peers
  // ============================================================
  Future<Map<String, dynamic>> startStoreSCP({
    required int port,
    required String aeTitle,
    required String storageDir,
  }) async {
    if (Platform.isIOS) {
      try {
        final dynamic result = await _methodChannel.invokeMethod('startStoreSCP', {
          'port': port,
          'aeTitle': aeTitle,
          'storageDir': storageDir,
        });
        return Map<String, dynamic>.from(result as Map);
      } on PlatformException catch (e) {
        return {'started': false, 'error': e.message};
      }
    }
    return {'started': false, 'error': 'Platform not supported'};
  }

  Future<Map<String, dynamic>> stopStoreSCP() async {
    if (Platform.isIOS) {
      try {
        final dynamic result = await _methodChannel.invokeMethod('stopStoreSCP');
        return Map<String, dynamic>.from(result as Map);
      } on PlatformException catch (e) {
        return {'stopped': false, 'error': e.message};
      }
    }
    return {'stopped': false, 'error': 'Platform not supported'};
  }

  Future<Map<String, dynamic>> getStoreSCPStatus() async {
    if (Platform.isIOS) {
      try {
        final dynamic result = await _methodChannel.invokeMethod('getStoreSCPStatus');
        return Map<String, dynamic>.from(result as Map);
      } on PlatformException catch (e) {
        return {'running': 0, 'error': e.message};
      }
    }
    return {'running': 0, 'error': 'Platform not supported'};
  }

  // ============================================================
  // C-MOVE: Retrieve instances via C-MOVE
  // ============================================================
  Future<Map<String, dynamic>> moveInstances({
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
    required String seriesInstanceUID,
    required String localStoragePath,
    int moveSCPPort = 11113,
  }) async {
    if (Platform.isIOS) {
      try {
        final dynamic result = await _methodChannel.invokeMethod('moveInstances', {
          'serverHost': serverHost,
          'serverPort': serverPort,
          'aeTitle': aeTitle,
          'calledAeTitle': calledAeTitle,
          'seriesInstanceUID': seriesInstanceUID,
          'localStoragePath': localStoragePath,
          'moveSCPPort': moveSCPPort,
        });
        return Map<String, dynamic>.from(result as Map);
      } on PlatformException catch (e) {
        return {'error': 1, 'errorMessage': e.message};
      }
    }
    return {'error': 1, 'errorMessage': 'Platform not supported'};
  }

  // ==================== TLS Configuration ====================

  /// Set TLS configuration. Once set, all subsequent SCU operations use TLS.
  /// Pass null or empty for files you don't have (e.g. no client cert).
  Future<bool> setTlsConfig({
    String? certFile,
    String? keyFile,
    String? caFile,
  }) async {
    final result = await _methodChannel.invokeMethod('setTlsConfig', {
      'certFile': certFile ?? '',
      'keyFile': keyFile ?? '',
      'caFile': caFile ?? '',
    });
    return result == true || result == 1;
  }

  /// Clear TLS configuration, revert to plaintext connections.
  Future<bool> clearTlsConfig() async {
    final result = await _methodChannel.invokeMethod('clearTlsConfig');
    return result == true || result == 1;
  }

  /// Check if OpenSSL is compiled into the native library.
  Future<bool> isTlsAvailable() async {
    final result = await _methodChannel.invokeMethod('isTlsAvailable');
    return result == true || result == 1;
  }

  /// Check if TLS is currently enabled.
  Future<bool> isTlsEnabled() async {
    final result = await _methodChannel.invokeMethod('isTlsEnabled');
    return result == true || result == 1;
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
