class DcmtkFlutter {
  static DcmtkFlutter? _instance;

  DcmtkFlutter._internal();

  factory DcmtkFlutter() {
    _instance ??= DcmtkFlutter._internal();
    return _instance!;
  }

  Future<String> loadDicomFile(String filePath) async {
    return 'Error: dcmtk_flutter is not supported on web';
  }

  Future<Map<String, dynamic>?> extractImage(
    String filePath, {
    int frameIndex = 0,
    double windowCenter = 0,
    double windowWidth = 0,
  }) async {
    return {'error': 'dcmtk_flutter is not supported on web'};
  }

  Future<Map<String, dynamic>?> extractVideo(String dicomPath, String outputPath) async {
    return null;
  }

  Future<String> getDicomTag(String filePath, String tagName) async {
    return 'Error: dcmtk_flutter is not supported on web';
  }

  Future<bool> validateDicomFile(String filePath) async {
    return false;
  }

  Future<bool> testServerConnection({
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
  }) async {
    return false;
  }

  Future<bool> testServerConnectionTls({
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
    String certFile = '',
    String keyFile = '',
    String caFile = '',
  }) async {
    return false;
  }

  Future<List<Map<String, dynamic>>> queryPatients({
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
  }) async {
    return <Map<String, dynamic>>[];
  }

  Future<List<Map<String, dynamic>>> queryStudiesForPatient({
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
    required String patientId,
  }) async {
    return <Map<String, dynamic>>[];
  }

  Future<List<Map<String, dynamic>>> querySeriesForStudy({
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
    required String studyInstanceUID,
  }) async {
    return <Map<String, dynamic>>[];
  }

  Future<List<Map<String, dynamic>>> queryInstancesForSeries({
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
    required String studyInstanceUID,
    required String seriesInstanceUID,
  }) async {
    return <Map<String, dynamic>>[];
  }

  Future<List<Map<String, dynamic>>> downloadInstancesViaCMove({
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
    required String moveDestination,
    required String studyInstanceUID,
    required String seriesInstanceUID,
    required List<String> sopInstanceUIDs,
    int timeoutSeconds = 60,
    int pollIntervalMs = 500,
    bool includePixelData = true,
  }) async {
    return <Map<String, dynamic>>[];
  }

  Future<Map<String, dynamic>> createPatient({
    required String patientName,
    required String patientId,
    required String birthDate,
    required String sex,
    required String studyDescription,
    String modality = 'OT',
    String accessionNumber = '',
    String institutionName = '',
    String referringPhysicianName = '',
    String studyId = '',
  }) async {
    return {'error': 'dcmtk_flutter is not supported on web'};
  }

  Future<Map<String, dynamic>> createGsps({
    required String sourceDicomPath,
    required String outputGspsPath,
    required String annotationJson,
  }) async {
    return {'error': 'dcmtk_flutter is not supported on web'};
  }

  Future<String?> parseGsps(String filePath) async {
    return null;
  }

  Future<Map<String, dynamic>> convertImageToDicom({
    required String imagePath,
    required String outputPath,
    String patientName = '',
    String patientId = '',
    String studyDescription = '',
    String modality = 'OT',
  }) async {
    return {'error': 'dcmtk_flutter is not supported on web'};
  }

  Future<Map<String, dynamic>> uploadImage({
    required String imagePath,
    required String outputDir,
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
    String patientName = '',
    String patientId = '',
    String studyDescription = '',
    String modality = 'OT',
  }) async {
    return {'error': 'dcmtk_flutter is not supported on web'};
  }

  Future<Map<String, dynamic>> uploadMultiframe({
    required List<String> framePaths,
    required String outputPath,
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
    String patientName = '',
    String patientId = '',
    String studyDescription = '',
    int frameRate = 25,
    String modality = 'XC',
  }) async {
    return {'error': 'dcmtk_flutter is not supported on web'};
  }

  Future<Map<String, dynamic>> uploadVideo({
    required String videoPath,
    required String outputPath,
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
    String patientName = '',
    String patientId = '',
    String studyDescription = '',
    String modality = 'XC',
  }) async {
    return {'error': 'dcmtk_flutter is not supported on web'};
  }

  Future<Map<String, dynamic>> storeFiles({
    required List<String> filePaths,
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
  }) async {
    return {'error': 'dcmtk_flutter is not supported on web'};
  }

  Future<Map<String, dynamic>> startStoreSCP({
    required int listenPort,
    required String aeTitle,
    required String outputDirectory,
  }) async {
    return {'success': false, 'error': 'dcmtk_flutter is not supported on web'};
  }

  Future<Map<String, dynamic>> stopStoreSCP() async {
    return {'success': false, 'error': 'dcmtk_flutter is not supported on web'};
  }

  Future<Map<String, dynamic>> getStoreSCPStatus() async {
    return {'running': false, 'port': 0, 'aeTitle': '', 'outputDirectory': ''};
  }

  Future<Map<String, dynamic>> moveInstances({
    required String serverHost,
    required int serverPort,
    required String aeTitle,
    required String calledAeTitle,
    required String moveDestination,
    required String studyInstanceUID,
    String seriesInstanceUID = '',
    String sopInstanceUID = '',
  }) async {
    return {'success': false, 'error': 'dcmtk_flutter is not supported on web'};
  }

  Future<bool> setTlsConfig({
    String certFile = '',
    String keyFile = '',
    String caFile = '',
  }) async {
    return false;
  }

  Future<bool> clearTlsConfig() async {
    return false;
  }

  Future<bool> isTlsAvailable() async {
    return false;
  }

  Future<bool> isTlsEnabled() async {
    return false;
  }

  Future<Map<String, dynamic>?> buildMprVolume(List<String> filePaths) async {
    return {'error': 'dcmtk_flutter is not supported on web'};
  }

  Future<Map<String, dynamic>?> getMprSlice(
    int volumeId,
    int plane,
    int sliceIndex, {
    double windowCenter = 0,
    double windowWidth = 0,
  }) async {
    return {'error': 'dcmtk_flutter is not supported on web'};
  }

  Future<void> freeMprVolume(int volumeId) async {}

  Future<Map<String, dynamic>?> renderMip(
    int volumeId,
    int axis, {
    double angleDegrees = 0,
    int slabThickness = 0,
    double windowCenter = 0,
    double windowWidth = 0,
  }) async {
    return {'error': 'dcmtk_flutter is not supported on web'};
  }

  Future<Map<String, dynamic>?> renderVolume(
    int volumeId, {
    double rotationX = 0,
    double rotationY = 0,
    double windowCenter = 0,
    double windowWidth = 0,
    String preset = 'Muscle',
    bool preview = false,
  }) async {
    return {'error': 'dcmtk_flutter is not supported on web'};
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
}
