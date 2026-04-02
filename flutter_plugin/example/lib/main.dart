import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';
import 'dart:ui' as ui;
import 'package:flutter/material.dart';
import 'package:dcmtk_flutter/dcmtk_flutter.dart';
import 'package:file_picker/file_picker.dart';
import 'package:path_provider/path_provider.dart';
import 'package:video_player/video_player.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'DCMTK Flutter Demo',
      theme: ThemeData(
        primarySwatch: Colors.blue,
      ),
      home: const MyHomePage(title: 'DCMTK Flutter Demo'),
    );
  }
}

class MyHomePage extends StatefulWidget {
  const MyHomePage({super.key, required this.title});

  final String title;

  @override
  State<MyHomePage> createState() => _MyHomePageState();
}

class _MyHomePageState extends State<MyHomePage> with TickerProviderStateMixin {
  static const int _tabFiles = 0;
  static const int _tabExplorer = 1;
  static const int _tabServer = 2;
  static const int _tabPatients = 3;
  static const int _tabMedia = 4;

  final DcmtkFlutter _dcmtk = DcmtkFlutter();
  String _result = 'No DICOM file loaded';
  bool _loading = false;
  ui.Image? _dicomImage;
  String? _currentFilePath;
  String? _currentOrthancInstanceId;
  String? _currentSopInstanceUid;
  final Map<String, String> _orthancInstanceByFilePath = <String, String>{};
  int _currentFrame = 0;
  int _totalFrames = 1;
  
  // Series instance navigation (multiple files from C-GET download)
  List<String> _seriesFilePaths = [];
  int _currentInstanceIndex = 0;

  // Video playback
  VideoPlayerController? _videoController;
  bool _isVideoFile = false;
  String? _videoMimeType;
  
  // Tab controller for File/Server tabs
  late TabController _tabController;
  
  // DICOM Server settings
  final TextEditingController _serverHostController = TextEditingController(text: '127.0.0.1');
  final TextEditingController _serverPortController = TextEditingController(text: '4242');
  final TextEditingController _aeTitleController = TextEditingController(text: 'FLUTTER_SCU');
  final TextEditingController _calledAeTitleController = TextEditingController(text: 'ORTHANC');
  final TextEditingController _httpHostController = TextEditingController(text: '127.0.0.1');
  final TextEditingController _httpPortController = TextEditingController(text: '8042');
  bool _serverConnected = false;
  List<DicomPatient> _patients = [];
  bool _queryingPatients = false;
  bool _runningDiagnostics = false;
  bool _downloadingFromExplorer = false;
  bool _downloadingViaDicom = false;
  String _echoStatus = 'Not run';
  String _findStatus = 'Not run';
  String _diagnosticsSummary = 'Run preflight to test C-ECHO and C-FIND';
  String _diagnosticsHint = '';

  // Patient management
  DicomPatient? _selectedPatient;
  DicomStudy? _selectedStudy;
  DicomSeries? _selectedSeries;
  List<DicomStudy> _studies = [];
  List<DicomSeries> _series = [];
  bool _queryingStudies = false;
  bool _queryingSeries = false;
  final TextEditingController _newPatientIdController = TextEditingController();
  final TextEditingController _newPatientNameController = TextEditingController();
  final TextEditingController _newPatientBirthDateController = TextEditingController();
  final TextEditingController _newPatientSexController = TextEditingController();
  
  // Media upload state
  bool _uploading = false;
  List<String> _uploadedMedia = [];
  
  // Image upload metadata controllers
  final TextEditingController _studyDescriptionController = TextEditingController(text: 'Mobile App Upload');
  final TextEditingController _seriesDescriptionController = TextEditingController(text: 'Uploaded Images');
  final TextEditingController _imageCommentsController = TextEditingController();
  final TextEditingController _modalityController = TextEditingController(text: 'SC'); // Secondary Capture

  @override
  void initState() {
    super.initState();
    _tabController = TabController(length: 5, vsync: this);
  }

  @override
  void dispose() {
    _tabController.dispose();
    _videoController?.dispose();
    _serverHostController.dispose();
    _serverPortController.dispose();
    _aeTitleController.dispose();
    _calledAeTitleController.dispose();
    _httpHostController.dispose();
    _httpPortController.dispose();
    _newPatientIdController.dispose();
    _newPatientNameController.dispose();
    _newPatientBirthDateController.dispose();
    _newPatientSexController.dispose();
    _studyDescriptionController.dispose();
    _seriesDescriptionController.dispose();
    _imageCommentsController.dispose();
    _modalityController.dispose();
    super.dispose();
  }

  Future<Directory> _getDicomDownloadsDirectory() async {
    final documentsDir = await getApplicationDocumentsDirectory();
    final dicomDir = Directory('${documentsDir.path}/DICOM Downloads');
    if (!await dicomDir.exists()) {
      await dicomDir.create(recursive: true);
    }
    return dicomDir;
  }

  Future<void> _pickAndLoadDicomFile() async {
    FilePickerResult? result = await FilePicker.platform.pickFiles(
      type: FileType.custom,
      allowedExtensions: ['dcm', 'dicom', 'DCM', 'DICOM'],
    );

    if (result != null && result.files.isNotEmpty) {
      final String filePath = result.files.first.path!;
      _currentOrthancInstanceId = null;
      _currentSopInstanceUid = null;
      await _loadDicomFile(filePath);
    }
  }

  Future<void> _openDownloadsFolder() async {
    final dicomDir = await _getDicomDownloadsDirectory();
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text('Downloads folder: ${dicomDir.path}')),
    );
    // User can now pick files from this location using "Pick DICOM File"
  }

  Future<void> _loadDicomFile(String filePath) async {
    setState(() {
      _loading = true;
      _currentFilePath = filePath;
      _dicomImage = null;
      _isVideoFile = false;
      _videoController?.dispose();
      _videoController = null;
      _videoMimeType = null;
    });

    try {
      await _hydrateOrthancContextFromFile(filePath);
      print('Loading DICOM file: $filePath');
      final String dicomInfo = await _dcmtk.loadDicomFile(filePath);
      print('DICOM info loaded: ${dicomInfo.substring(0, dicomInfo.length > 500 ? 500 : dicomInfo.length)}...');

      final sopMatch = RegExp(r'SOPInstanceUID:\s*([^\n\r]+)').firstMatch(dicomInfo);
      _currentSopInstanceUid = sopMatch?.group(1)?.trim();
      if (_currentSopInstanceUid != null && _currentSopInstanceUid!.isNotEmpty) {
        print('Detected SOPInstanceUID: $_currentSopInstanceUid');
      }

      // Check if this is a video DICOM file by SOP class or MPEG transfer syntax
      final sopClassMatch = RegExp(r'SOPClassUID:\s*([^\n\r]+)').firstMatch(dicomInfo);
      final sopClass = sopClassMatch?.group(1)?.trim() ?? '';
      final tsMatch = RegExp(r'TransferSyntaxUID:\s*([^\n\r]+)').firstMatch(dicomInfo);
      final transferSyntax = tsMatch?.group(1)?.trim() ?? '';
      const videoSopClasses = [
        '1.2.840.10008.5.1.4.1.1.77.1.1.1', // Video Endoscopic
        '1.2.840.10008.5.1.4.1.1.77.1.2.1', // Video Microscopic
        '1.2.840.10008.5.1.4.1.1.77.1.4.1', // Video Photographic
      ];
      const mpegTransferSyntaxes = [
        '1.2.840.10008.1.2.4.100',  // MPEG2 Main Profile
        '1.2.840.10008.1.2.4.101',  // MPEG2 Main Profile High Level
        '1.2.840.10008.1.2.4.102',  // MPEG4 AVC/H.264 High Profile Level 4.1
        '1.2.840.10008.1.2.4.103',  // MPEG4 AVC/H.264 BD-compat High Profile Level 4.1
        '1.2.840.10008.1.2.4.104',  // MPEG4 AVC/H.264 High Profile Level 4.2 2D
        '1.2.840.10008.1.2.4.105',  // MPEG4 AVC/H.264 High Profile Level 4.2 3D
        '1.2.840.10008.1.2.4.106',  // MPEG4 AVC/H.264 Stereo High Profile Level 4.2
      ];
      final isVideo = videoSopClasses.contains(sopClass) ||
                      mpegTransferSyntaxes.contains(transferSyntax);

      if (isVideo) {
        print('Video DICOM detected, extracting video payload...');
        await _extractAndPlayVideo(filePath);
        setState(() {
          _result = dicomInfo;
          _loading = false;
        });
        return;
      }

      // Parse frame count from DICOM info (fallback; native extractImage returns totalFrames too)
      final framesMatch = RegExp(r'NumberOfFrames.*?(\d+)').firstMatch(dicomInfo);
      final totalFrames = framesMatch != null ? int.parse(framesMatch.group(1)!) : 1;
      
      print('Total frames detected: $totalFrames');
      
      // Reset to first frame
      _currentFrame = 0;
      _totalFrames = totalFrames;
      
      // Extract first frame
      await _loadFrame(filePath, 0);
      
      setState(() {
        _result = dicomInfo;
        _loading = false;
      });
    } catch (e) {
      setState(() {
        _result = 'Error loading DICOM file: $e';
        _dicomImage = null;
        _loading = false;
      });
    }
  }

  Future<void> _hydrateOrthancContextFromFile(String filePath) async {
    if (_currentOrthancInstanceId != null && _currentOrthancInstanceId!.isNotEmpty) {
      return;
    }

    final mapped = _orthancInstanceByFilePath[filePath];
    if (mapped != null && mapped.isNotEmpty) {
      _currentOrthancInstanceId = mapped;
      return;
    }

    final fileName = filePath.split('/').last;
    final nameMatch = RegExp(r'^OrthancInst_([^_]+)_').firstMatch(fileName);
    if (nameMatch != null && (nameMatch.group(1)?.isNotEmpty ?? false)) {
      _currentOrthancInstanceId = nameMatch.group(1)!;
      _orthancInstanceByFilePath[filePath] = _currentOrthancInstanceId!;
      return;
    }

    final sidecar = File('$filePath.meta.json');
    if (await sidecar.exists()) {
      try {
        final text = await sidecar.readAsString();
        final dynamic meta = jsonDecode(text);
        if (meta is Map<String, dynamic>) {
          final id = meta['orthancInstanceId']?.toString() ?? '';
          if (id.isNotEmpty) {
            _currentOrthancInstanceId = id;
            _orthancInstanceByFilePath[filePath] = id;
          }
          final sop = meta['sopInstanceUid']?.toString() ?? '';
          if (sop.isNotEmpty) {
            _currentSopInstanceUid = sop;
          }
        }
      } catch (e) {
        print('Failed to read sidecar metadata: $e');
      }
    }
  }

  Future<void> _loadFrame(String filePath, int frameIndex) async {
    try {
      print('--- Loading frame $frameIndex from: $filePath');
      
      final imageData = await _dcmtk.extractImage(filePath, frameIndex: frameIndex);
      ui.Image? image;
      
      if (imageData != null) {
        final bytes = imageData['data'] as Uint8List;
        final width = imageData['width'] as int;
        final height = imageData['height'] as int;
        final nativeTotalFrames = imageData['totalFrames'] as int?;
        print('Image data received: ${width}x${height}, ${bytes.length} bytes, totalFrames=${nativeTotalFrames ?? '-'}');

        if (nativeTotalFrames != null && nativeTotalFrames > 0) {
          _totalFrames = nativeTotalFrames;
        }

        image = await _convertRgbaToImage(
          bytes,
          width,
          height,
        );
        
        print('Successfully converted to UI image');
      } else {
        print('WARNING: extractImage returned null');
        image = await _loadOrthancPreviewFrame(frameIndex);
      }

      if (image == null) {
        print('WARNING: No renderable image for frame $frameIndex');
      }
      
      setState(() {
        _dicomImage = image;
        _currentFrame = frameIndex;
      });
      
      print('Frame $frameIndex loaded successfully');
    } catch (e) {
      print('ERROR loading frame $frameIndex: $e');

      final fallback = await _loadOrthancPreviewFrame(frameIndex);
      if (fallback != null) {
        setState(() {
          _dicomImage = fallback;
          _currentFrame = frameIndex;
          _result = 'Rendered via Orthanc preview fallback (native decode unavailable for this transfer syntax).';
        });
      } else {
        setState(() {
          _result = 'Error extracting image: $e';
          _dicomImage = null;
        });
      }
    }
  }

  Future<void> _extractAndPlayVideo(String dicomPath) async {
    final tempDir = await getTemporaryDirectory();
    final fileName = dicomPath.split('/').last;
    final outputPath = '${tempDir.path}/video_$fileName.mp4';

    final videoResult = await _dcmtk.extractVideo(dicomPath, outputPath);
    if (videoResult == null) {
      print('Video extraction failed');
      setState(() {
        _isVideoFile = true;
        _result = 'Video DICOM detected but extraction failed.';
      });
      return;
    }

    final extractedPath = videoResult['outputPath'] as String;
    final mimeType = videoResult['mimeType'] as String;
    final fileSize = videoResult['fileSize'] as int;
    print('Video extracted: $extractedPath ($mimeType, $fileSize bytes)');

    _videoController?.dispose();
    final controller = VideoPlayerController.file(File(extractedPath));
    try {
      await controller.initialize();
      controller.setLooping(true);
      await controller.play();
      setState(() {
        _videoController = controller;
        _isVideoFile = true;
        _videoMimeType = mimeType;
      });
    } catch (e) {
      print('Video player initialization failed: $e');
      controller.dispose();
      setState(() {
        _isVideoFile = true;
        _result = 'Video extracted ($fileSize bytes) but playback failed: $e';
      });
    }
  }

  Future<ui.Image?> _loadOrthancPreviewFrame(int frameIndex) async {
    final httpHost = _httpHostController.text.trim();
    final httpPort = int.tryParse(_httpPortController.text.trim()) ?? 8042;
    if (httpHost.isEmpty) {
      return null;
    }

    if (_currentOrthancInstanceId == null || _currentOrthancInstanceId!.isEmpty) {
      await _resolveOrthancInstanceIdFromSopUid(httpHost, httpPort);
      if (_currentOrthancInstanceId == null || _currentOrthancInstanceId!.isEmpty) {
        print('Orthanc fallback unavailable: no instance id could be resolved from SOP UID');
        return null;
      }
    }

    final client = HttpClient();
    try {
      final base = Uri.parse('http://$httpHost:$httpPort');
      final req = await client.getUrl(
        base.resolve('/instances/${_currentOrthancInstanceId!}/frames/$frameIndex/preview'),
      );
      final resp = await req.close();
      if (resp.statusCode < 200 || resp.statusCode >= 300) {
        return null;
      }

      final bytesBuilder = BytesBuilder(copy: false);
      await for (final chunk in resp) {
        bytesBuilder.add(chunk);
      }
      final bytes = bytesBuilder.takeBytes();
      if (bytes.isEmpty) {
        return null;
      }

      final codec = await ui.instantiateImageCodec(bytes);
      final frame = await codec.getNextFrame();
      print('Loaded frame $frameIndex via Orthanc preview fallback');
      return frame.image;
    } catch (e) {
      print('Orthanc preview fallback failed for frame $frameIndex: $e');
      return null;
    } finally {
      client.close(force: true);
    }
  }

  Future<void> _resolveOrthancInstanceIdFromSopUid(String httpHost, int httpPort) async {
    if (_currentSopInstanceUid == null || _currentSopInstanceUid!.isEmpty) {
      // Last resort: if current explorer context exists, find any instance in selected series.
      if (_selectedSeries == null) return;
    }

    final client = HttpClient();
    try {
      final base = Uri.parse('http://$httpHost:$httpPort');
      final findReq = await client.postUrl(base.resolve('/tools/find'));
      findReq.headers.contentType = ContentType.json;
      final body = jsonEncode({
        'Level': 'Instance',
        'Query': _currentSopInstanceUid != null && _currentSopInstanceUid!.isNotEmpty
            ? {
                'SOPInstanceUID': _currentSopInstanceUid,
              }
            : {
                'SeriesInstanceUID': _selectedSeries!.seriesInstanceUID,
              },
      });
      findReq.add(utf8.encode(body));
      final resp = await findReq.close();
      final text = await utf8.decodeStream(resp);
      if (resp.statusCode < 200 || resp.statusCode >= 300) {
        print('Orthanc instance resolve failed (${resp.statusCode}): $text');
        return;
      }

      final dynamic json = jsonDecode(text);
      if (json is List && json.isNotEmpty) {
        _currentOrthancInstanceId = json.first.toString();
        print('Resolved Orthanc instance id from SOP UID: $_currentOrthancInstanceId');
        if (_currentFilePath != null && _currentFilePath!.isNotEmpty) {
          _orthancInstanceByFilePath[_currentFilePath!] = _currentOrthancInstanceId!;
        }
      }
    } catch (e) {
      print('Orthanc instance resolve error: $e');
    } finally {
      client.close(force: true);
    }
  }

  void _previousFrame() {
    if (_currentFilePath != null && _currentFrame > 0) {
      _loadFrame(_currentFilePath!, _currentFrame - 1);
    }
  }

  void _nextFrame() {
    if (_currentFilePath != null && _currentFrame < _totalFrames - 1) {
      _loadFrame(_currentFilePath!, _currentFrame + 1);
    }
  }

  void _previousInstance() {
    if (_seriesFilePaths.isNotEmpty && _currentInstanceIndex > 0) {
      _currentInstanceIndex--;
      _loadDicomFile(_seriesFilePaths[_currentInstanceIndex]);
    }
  }

  void _nextInstance() {
    if (_seriesFilePaths.isNotEmpty && _currentInstanceIndex < _seriesFilePaths.length - 1) {
      _currentInstanceIndex++;
      _loadDicomFile(_seriesFilePaths[_currentInstanceIndex]);
    }
  }

  Future<ui.Image> _convertRgbaToImage(Uint8List pixelData, int width, int height) async {
    final expectedRgbaBytes = width * height * 4;

    // New native extractor returns RGBA. Keep grayscale compatibility as fallback.
    final Uint8List rgbaData;
    if (pixelData.length == expectedRgbaBytes) {
      rgbaData = pixelData;
    } else if (pixelData.length == width * height) {
      final converted = Uint8List(expectedRgbaBytes);
      for (int i = 0; i < pixelData.length; i++) {
        final gray = pixelData[i];
        converted[i * 4] = gray;
        converted[i * 4 + 1] = gray;
        converted[i * 4 + 2] = gray;
        converted[i * 4 + 3] = 255;
      }
      rgbaData = converted;
    } else {
      throw Exception(
        'Unexpected pixel buffer size: ${pixelData.length}, expected $expectedRgbaBytes (RGBA) or ${width * height} (grayscale)',
      );
    }

    final completer = Completer<ui.Image>();
    ui.decodeImageFromPixels(
      rgbaData,
      width,
      height,
      ui.PixelFormat.rgba8888,
      (ui.Image image) {
        completer.complete(image);
      },
    );
    return completer.future;
  }

  // DICOM Server Communication Methods
  Future<void> _testServerConnection() async {
    setState(() {
      _serverConnected = false;
    });

    final serverHost = _serverHostController.text.trim();
    final serverPort = int.tryParse(_serverPortController.text.trim()) ?? 4242;
    final aeTitle = _aeTitleController.text.trim();
    final calledAeTitle = _calledAeTitleController.text.trim();

    if (serverHost.isEmpty || aeTitle.isEmpty || calledAeTitle.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Please fill in all server connection fields')),
      );
      return;
    }

    print('Testing connection to $serverHost:$serverPort (AET: $aeTitle -> $calledAeTitle)');

    try {
      final bool connected = await _dcmtk.testServerConnection(
        serverHost: serverHost,
        serverPort: serverPort,
        aeTitle: aeTitle,
        calledAeTitle: calledAeTitle,
      );

      setState(() {
        _serverConnected = connected;
      });

      if (connected) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('✅ Server connection successful!')),
        );
      } else {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('❌ Failed to connect to server')),
        );
      }
    } catch (e) {
      print('Error testing server connection: $e');
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Connection error: $e')),
      );
    }
  }

  Future<void> _queryPatients() async {
    if (!_serverConnected) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Please test server connection first')),
      );
      return;
    }

    setState(() {
      _queryingPatients = true;
      _patients.clear();
      _selectedPatient = null;
      _selectedStudy = null;
      _selectedSeries = null;
      _studies = [];
      _series = [];
    });

    final serverHost = _serverHostController.text.trim();
    final serverPort = int.tryParse(_serverPortController.text.trim()) ?? 4242;
    final aeTitle = _aeTitleController.text.trim();
    final calledAeTitle = _calledAeTitleController.text.trim();

    // Small delay to ensure previous association is fully closed
    await Future.delayed(const Duration(milliseconds: 500));

    try {
      final List<Map<String, dynamic>> patientMaps = await _dcmtk.queryPatients(
        serverHost: serverHost,
        serverPort: serverPort,
        aeTitle: aeTitle,
        calledAeTitle: calledAeTitle,
      );

      final List<DicomPatient> patients = patientMaps
          .map((patientMap) => DicomPatient.fromMap(patientMap))
          .toList();

      setState(() {
        _patients = patients;
        _queryingPatients = false;
      });

      print('Retrieved ${patients.length} patients');
    } catch (e) {
      print('Error querying patients: $e');
      setState(() {
        _queryingPatients = false;
      });
      final message = e.toString();
      final isAssociationAbort =
          message.toLowerCase().contains('peer aborted association') ||
          message.toLowerCase().contains('never connected');
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(
            isAssociationAbort
                ? 'C-FIND failed: peer aborted association. Check Orthanc QueryRetrieveEnabled and AE title/modality config.'
                : 'Error querying patients: $e',
          ),
          duration: const Duration(seconds: 6),
        ),
      );
    }
  }

  Future<void> _loadStudiesForPatient(String patientId) async {
    if (!_serverConnected) return;

    setState(() {
      _queryingStudies = true;
      _selectedStudy = null;
      _selectedSeries = null;
      _studies = [];
      _series = [];
    });

    try {
      final studyMaps = await _dcmtk.queryStudiesForPatient(
        serverHost: _serverHostController.text.trim(),
        serverPort: int.tryParse(_serverPortController.text.trim()) ?? 4242,
        aeTitle: _aeTitleController.text.trim(),
        calledAeTitle: _calledAeTitleController.text.trim(),
        patientId: patientId,
      );

      setState(() {
        _studies = studyMaps.map((map) => DicomStudy.fromMap(map)).toList();
        _queryingStudies = false;
      });
    } catch (e) {
      setState(() {
        _queryingStudies = false;
      });
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Error querying studies: $e')),
      );
    }
  }

  Future<void> _loadSeriesForStudy(String studyInstanceUID) async {
    if (!_serverConnected) return;

    setState(() {
      _queryingSeries = true;
      _selectedSeries = null;
      _series = [];
    });

    try {
      final seriesMaps = await _dcmtk.querySeriesForStudy(
        serverHost: _serverHostController.text.trim(),
        serverPort: int.tryParse(_serverPortController.text.trim()) ?? 4242,
        aeTitle: _aeTitleController.text.trim(),
        calledAeTitle: _calledAeTitleController.text.trim(),
        studyInstanceUID: studyInstanceUID,
      );

      setState(() {
        _series = seriesMaps.map((map) => DicomSeries.fromMap(map)).toList();
        _queryingSeries = false;
      });
    } catch (e) {
      setState(() {
        _queryingSeries = false;
      });
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Error querying series: $e')),
      );
    }
  }

  Future<void> _runConnectionDiagnostics() async {
    final serverHost = _serverHostController.text.trim();
    final serverPort = int.tryParse(_serverPortController.text.trim()) ?? 4242;
    final aeTitle = _aeTitleController.text.trim();
    final calledAeTitle = _calledAeTitleController.text.trim();

    if (serverHost.isEmpty || aeTitle.isEmpty || calledAeTitle.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Please fill in Host, AE Title, and Called AE Title first')),
      );
      return;
    }

    setState(() {
      _runningDiagnostics = true;
      _echoStatus = 'Running...';
      _findStatus = 'Waiting for C-ECHO';
      _diagnosticsSummary = 'Running preflight checks';
      _diagnosticsHint = '';
    });

    final List<String> hints = [];
    bool echoOk = false;
    bool findOk = false;

    try {
      echoOk = await _dcmtk.testServerConnection(
        serverHost: serverHost,
        serverPort: serverPort,
        aeTitle: aeTitle,
        calledAeTitle: calledAeTitle,
      );

      setState(() {
        _echoStatus = echoOk ? 'OK' : 'FAILED';
        _serverConnected = echoOk;
        _findStatus = 'Running...';
      });

      if (!echoOk) {
        hints.add('C-ECHO failed: check host/port and Called AE Title.');
      }
    } catch (e) {
      setState(() {
        _echoStatus = 'ERROR: $e';
        _findStatus = 'Skipped due to C-ECHO error';
      });
      hints.add('C-ECHO threw an error: verify DICOM service is reachable from simulator.');
    }

    if (echoOk) {
      await Future.delayed(const Duration(milliseconds: 300));
      try {
        final patients = await _dcmtk.queryPatients(
          serverHost: serverHost,
          serverPort: serverPort,
          aeTitle: aeTitle,
          calledAeTitle: calledAeTitle,
        );

        findOk = true;
        setState(() {
          _patients = patients.map((patientMap) => DicomPatient.fromMap(patientMap)).toList();
          _findStatus = 'OK (${patients.length} patients)';
        });
      } catch (e) {
        final msg = e.toString();
        final normalized = msg.toLowerCase();
        final isAssociationAbort = normalized.contains('peer aborted association') ||
            normalized.contains('never connected');

        setState(() {
          _findStatus = 'FAILED: $e';
        });

        if (isAssociationAbort) {
          hints.add('C-FIND association aborted: enable QueryRetrieveEnabled in Orthanc.');
          hints.add('Ensure "$aeTitle" is permitted as a modality/peer in Orthanc config.');
          hints.add('If Orthanc runs in Docker, use host.docker.internal instead of localhost.');
        } else {
          hints.add('C-FIND failed: check Orthanc logs and presentation context negotiation.');
        }
      }
    }

    final summary = echoOk && findOk
        ? 'Diagnostics passed: C-ECHO and C-FIND are working.'
        : echoOk
            ? 'Partial success: C-ECHO works, C-FIND needs configuration fixes.'
            : 'Diagnostics failed: could not establish DICOM association.';

    setState(() {
      _runningDiagnostics = false;
      _diagnosticsSummary = summary;
      _diagnosticsHint = hints.join('\n');
    });
  }

  Future<void> _createNewPatient() async {
    if (_newPatientIdController.text.isEmpty || _newPatientNameController.text.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Patient ID and Name are required')),
      );
      return;
    }

    setState(() {
      _uploading = true;
    });

    try {
      final result = await _dcmtk.createPatient(
        serverHost: _serverHostController.text,
        serverPort: int.parse(_serverPortController.text),
        aeTitle: _aeTitleController.text,
        calledAeTitle: _calledAeTitleController.text,
        patientId: _newPatientIdController.text,
        patientName: _newPatientNameController.text,
        birthDate: _newPatientBirthDateController.text,
        sex: _newPatientSexController.text,
      );

      setState(() {
        _uploading = false;
      });

      if (result['success'] == true || result['success'] == 1) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Patient created successfully: ${result['patientId']}')),
        );
        
        // Clear form and refresh patient list
        _newPatientIdController.clear();
        _newPatientNameController.clear();
        _newPatientBirthDateController.clear();
        _newPatientSexController.clear();
        await _queryPatients();
      } else {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Failed to create patient: ${result['error']}')),
        );
      }
    } catch (e) {
      setState(() {
        _uploading = false;
      });
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Error creating patient: $e')),
      );
    }
  }

  Future<void> _uploadImageForPatient() async {
    if (_selectedPatient == null) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Please select a patient first')),
      );
      return;
    }

    FilePickerResult? result = await FilePicker.platform.pickFiles(
      type: FileType.image,
    );

    if (result != null && result.files.isNotEmpty) {
      final String imagePath = result.files.first.path!;
      await _uploadImage(imagePath);
    }
  }

  Future<void> _uploadImage(String imagePath) async {
    if (_selectedPatient == null) return;

    setState(() {
      _uploading = true;
    });

    try {
      final result = await _dcmtk.uploadImage(
        serverHost: _serverHostController.text,
        serverPort: int.parse(_serverPortController.text),
        aeTitle: _aeTitleController.text,
        calledAeTitle: _calledAeTitleController.text,
        patientId: _selectedPatient!.patientId,
        imagePath: imagePath,
        studyDescription: _studyDescriptionController.text.isEmpty 
            ? 'Mobile App Upload' 
            : _studyDescriptionController.text,
        seriesDescription: _seriesDescriptionController.text.isEmpty
            ? 'Uploaded Images'
            : _seriesDescriptionController.text,
        imageComments: _imageCommentsController.text,
        modality: _modalityController.text.isEmpty
            ? 'SC'
            : _modalityController.text,
      );

      setState(() {
        _uploading = false;
      });

      if (result['success'] == true || result['success'] == 1) {
        setState(() {
          _uploadedMedia.add('${result['sopInstanceUID']} - Image uploaded');
        });

        await _refreshExplorerAfterUpload();
        
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Image uploaded successfully!')),
        );
      } else {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Failed to upload image: ${result['error']}')),
        );
      }
    } catch (e) {
      setState(() {
        _uploading = false;
      });
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Error uploading image: $e')),
      );
    }
  }

  Future<void> _uploadVideoForPatient() async {
    if (_selectedPatient == null) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Please select a patient first')),
      );
      return;
    }

    FilePickerResult? result = await FilePicker.platform.pickFiles(
      type: FileType.custom,
      allowedExtensions: ['mp4', 'mov', 'm4v', 'mpeg', 'mpg'],
    );

    if (result != null && result.files.isNotEmpty) {
      final String videoPath = result.files.first.path!;
      await _uploadVideo(videoPath);
    }
  }

  Future<void> _uploadVideo(String videoPath) async {
    if (_selectedPatient == null) return;

    setState(() {
      _uploading = true;
    });

    try {
      final result = await _dcmtk.uploadVideo(
        serverHost: _serverHostController.text,
        serverPort: int.parse(_serverPortController.text),
        aeTitle: _aeTitleController.text,
        calledAeTitle: _calledAeTitleController.text,
        patientId: _selectedPatient!.patientId,
        videoPath: videoPath,
        studyDescription: _studyDescriptionController.text.isEmpty
            ? 'Mobile App Upload Video'
            : _studyDescriptionController.text,
        seriesDescription: _seriesDescriptionController.text.isEmpty
            ? 'Uploaded Videos'
            : _seriesDescriptionController.text,
        imageComments: _imageCommentsController.text,
        modality: _modalityController.text.isEmpty
            ? 'XC'
            : _modalityController.text,
      );

      setState(() {
        _uploading = false;
      });

      if (result['success'] == true || result['success'] == 1) {
        setState(() {
          _uploadedMedia.add('${result['sopInstanceUID']} - Video uploaded');
        });

        await _refreshExplorerAfterUpload();

        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Video uploaded successfully!')),
        );
      } else {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Failed to upload video: ${result['error']}')),
        );
      }
    } catch (e) {
      setState(() {
        _uploading = false;
      });
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Error uploading video: $e')),
      );
    }
  }

  Future<void> _refreshExplorerAfterUpload() async {
    if (_selectedPatient == null) return;
    await _loadStudiesForPatient(_selectedPatient!.patientId);
    if (_selectedStudy != null) {
      await _loadSeriesForStudy(_selectedStudy!.studyInstanceUID);
    }
  }

  void _openFilesFromExplorerSelection() {
    if (_selectedSeries == null) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Select a series first in Explorer.')),
      );
      return;
    }

    final patientName = _selectedPatient?.patientName.isNotEmpty == true
        ? _selectedPatient!.patientName
        : 'Unknown';
    final patientId = _selectedPatient?.patientId ?? '-';
    final studyDesc = _selectedStudy?.studyDescription.isNotEmpty == true
        ? _selectedStudy!.studyDescription
        : 'N/A';
    final seriesDesc = _selectedSeries!.seriesDescription.isNotEmpty
        ? _selectedSeries!.seriesDescription
        : 'N/A';
    final seriesUid = _selectedSeries!.seriesInstanceUID;

    setState(() {
      _dicomImage = null;
      _result = 'Explorer Selection\n\n'
          'Patient: $patientName ($patientId)\n'
          'Study: $studyDesc\n'
          'Series: $seriesDesc\n'
          'Series UID: $seriesUid\n\n'
          'No local DICOM file is loaded yet.\n'
          'Click "Download First Instance" to fetch one DICOM from Orthanc and open it in this tab.';
    });

    _tabController.animateTo(_tabFiles);
  }

  Future<void> _downloadSelectedSeriesFirstInstance() async {
    if (_selectedSeries == null) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Select a series first.')),
      );
      return;
    }

    final httpHost = _httpHostController.text.trim();
    final httpPort = int.tryParse(_httpPortController.text.trim()) ?? 8042;
    if (httpHost.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('HTTP host is required in Server tab.')),
      );
      return;
    }

    setState(() {
      _downloadingFromExplorer = true;
      _loading = true;
      _dicomImage = null;
      _result = 'Downloading first instance for selected series...';
    });

    final client = HttpClient();
    try {
      final base = Uri.parse('http://$httpHost:$httpPort');

      final findReq = await client.postUrl(base.resolve('/tools/find'));
      findReq.headers.contentType = ContentType.json;
      final findBody = jsonEncode({
        'Level': 'Series',
        'Query': {
          'SeriesInstanceUID': _selectedSeries!.seriesInstanceUID,
        },
      });
      findReq.add(utf8.encode(findBody));
      final findResp = await findReq.close();
      final findText = await utf8.decodeStream(findResp);
      if (findResp.statusCode < 200 || findResp.statusCode >= 300) {
        throw Exception('Orthanc /tools/find failed (${findResp.statusCode}): $findText');
      }

      final dynamic findJson = jsonDecode(findText);
      if (findJson is! List || findJson.isEmpty) {
        throw Exception('No Orthanc series found for selected SeriesInstanceUID.');
      }
      final orthancSeriesId = findJson.first.toString();

      final seriesReq = await client.getUrl(base.resolve('/series/$orthancSeriesId'));
      final seriesResp = await seriesReq.close();
      final seriesText = await utf8.decodeStream(seriesResp);
      if (seriesResp.statusCode < 200 || seriesResp.statusCode >= 300) {
        throw Exception('Orthanc /series/$orthancSeriesId failed (${seriesResp.statusCode}): $seriesText');
      }

      final dynamic seriesJson = jsonDecode(seriesText);
      final instances = (seriesJson as Map<String, dynamic>)['Instances'];
      if (instances is! List || instances.isEmpty) {
        throw Exception('Selected series has no instances in Orthanc.');
      }
      final firstInstanceId = instances.first.toString();
      _currentOrthancInstanceId = firstInstanceId;

      final fileReq = await client.getUrl(base.resolve('/instances/$firstInstanceId/file'));
      final fileResp = await fileReq.close();
      if (fileResp.statusCode < 200 || fileResp.statusCode >= 300) {
        final errText = await utf8.decodeStream(fileResp);
        throw Exception('Orthanc /instances/$firstInstanceId/file failed (${fileResp.statusCode}): $errText');
      }
      final bytesBuilder = BytesBuilder(copy: false);
      await for (final chunk in fileResp) {
        bytesBuilder.add(chunk);
      }
      final bytes = bytesBuilder.takeBytes();

      // Save to persistent downloads directory instead of temp
      final downloadsDir = await _getDicomDownloadsDirectory();
      final timestamp = DateTime.now().millisecondsSinceEpoch;
      final seriesDesc = _selectedSeries!.seriesDescription.isEmpty 
          ? 'Series' 
          : _selectedSeries!.seriesDescription.replaceAll(RegExp(r'[^\w\s]'), '');
      final filename = 'OrthancInst_${firstInstanceId}_${seriesDesc}_$timestamp.dcm';
      final outFile = File('${downloadsDir.path}/$filename');
      await outFile.writeAsBytes(bytes, flush: true);

      _orthancInstanceByFilePath[outFile.path] = firstInstanceId;
      final metaFile = File('${outFile.path}.meta.json');
      await metaFile.writeAsString(
        jsonEncode({
          'orthancInstanceId': firstInstanceId,
          'seriesInstanceUID': _selectedSeries!.seriesInstanceUID,
          'savedAt': DateTime.now().toIso8601String(),
        }),
        flush: true,
      );

      await _loadDicomFile(outFile.path);
      _tabController.animateTo(_tabFiles);

      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Downloaded to: ${downloadsDir.path}')),
        );
      }
    } catch (e) {
      setState(() {
        _loading = false;
        _result = 'Download failed: $e';
      });
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Download failed: $e')),
        );
      }
    } finally {
      client.close(force: true);
      if (mounted) {
        setState(() {
          _downloadingFromExplorer = false;
        });
      }
    }
  }

  Future<void> _downloadSeriesViaCMove() async {
    if (_selectedSeries == null) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Select a series first.')),
      );
      return;
    }

    final serverHost = _serverHostController.text.trim();
    final serverPort = int.tryParse(_serverPortController.text.trim()) ?? 4242;
    final aeTitle = _aeTitleController.text.trim();
    final calledAeTitle = _calledAeTitleController.text.trim();

    if (serverHost.isEmpty || aeTitle.isEmpty || calledAeTitle.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('DICOM server settings required in Server tab.')),
      );
      return;
    }

    setState(() {
      _downloadingViaDicom = true;
      _loading = true;
      _dicomImage = null;
      _result = 'Downloading series via C-GET...';
    });

    try {
      final downloadsDir = await _getDicomDownloadsDirectory();
      _currentOrthancInstanceId = null;
      _currentSopInstanceUid = null;
      
      final instances = await _dcmtk
          .downloadInstancesViaCMove(
            serverHost: serverHost,
            serverPort: serverPort,
            aeTitle: aeTitle,
            calledAeTitle: calledAeTitle,
            seriesInstanceUID: _selectedSeries!.seriesInstanceUID,
            localStoragePath: downloadsDir.path,
          )
          .timeout(
            const Duration(seconds: 60),
            onTimeout: () => throw Exception('C-GET timed out after 60s.'),
          );

      if (instances.isEmpty) {
        throw Exception('No instances found for this series.');
      }

      // Collect all downloaded files
      List<String> downloadedPaths = [];
      for (final instance in instances) {
        final filePath = instance['filePath'] as String?;
        if (filePath != null && filePath.isNotEmpty) {
          final file = File(filePath);
          if (await file.exists()) {
            downloadedPaths.add(filePath);
          }
        }
      }

      if (downloadedPaths.isEmpty) {
        throw Exception(
          'Server does not support C-GET. Found ${instances.length} instance(s) '
          'via C-FIND but could not download. Enable C-GET on the PACS server.');
      }

      // Sort for consistent ordering
      downloadedPaths.sort();
      
      // Set up instance navigation
      setState(() {
        _seriesFilePaths = downloadedPaths;
        _currentInstanceIndex = 0;
      });
      
      await _loadDicomFile(downloadedPaths.first);
      _tabController.animateTo(_tabFiles);

      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Downloaded ${downloadedPaths.length} instance(s) via C-GET')),
        );
      }
    } catch (e) {
      setState(() {
        _loading = false;
        _result = 'Download failed: $e';
      });
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Download failed: $e')),
        );
      }
    } finally {
      if (mounted) {
        setState(() {
          _downloadingViaDicom = false;
          _loading = false;
        });
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(widget.title),
        bottom: TabBar(
          controller: _tabController,
          tabs: const [
            Tab(icon: Icon(Icons.file_open), text: 'Files'),
            Tab(icon: Icon(Icons.travel_explore), text: 'Explorer'),
            Tab(icon: Icon(Icons.cloud), text: 'Server'),
            Tab(icon: Icon(Icons.person_add), text: 'Patients'),
            Tab(icon: Icon(Icons.upload), text: 'Media'),
          ],
        ),
      ),
      body: TabBarView(
        controller: _tabController,
        children: [
          _buildFileLoadingTab(),
          _buildExplorerTab(),
          _buildServerTab(),
          _buildPatientManagementTab(),
          _buildMediaUploadTab(),
        ],
      ),
    );
  }

  Widget _buildFileLoadingTab() {
    return Padding(
      padding: const EdgeInsets.all(16.0),
      child: Column(
        children: [
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: [
              ElevatedButton(
                onPressed: _loading ? null : _pickAndLoadDicomFile,
                child: Text(_loading ? 'Loading...' : 'Pick DICOM File'),
              ),
              OutlinedButton.icon(
                onPressed: () => _tabController.animateTo(_tabExplorer),
                icon: const Icon(Icons.travel_explore),
                label: const Text('Browse DICOM Server'),
              ),
              OutlinedButton.icon(
                onPressed: _openDownloadsFolder,
                icon: const Icon(Icons.folder_open),
                label: const Text('View Downloads'),
              ),
            ],
          ),
          const SizedBox(height: 20),
          if (_isVideoFile && _videoController != null && _videoController!.value.isInitialized) ...[
            // Instance navigation (when multiple files downloaded via C-GET)
            if (_seriesFilePaths.length > 1) ...[
              Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  IconButton(
                    onPressed: _currentInstanceIndex > 0 ? _previousInstance : null,
                    icon: const Icon(Icons.skip_previous),
                    tooltip: 'Previous Instance',
                  ),
                  Text(
                    'Instance ${_currentInstanceIndex + 1} of ${_seriesFilePaths.length}',
                    style: const TextStyle(fontWeight: FontWeight.bold),
                  ),
                  IconButton(
                    onPressed: _currentInstanceIndex < _seriesFilePaths.length - 1 ? _nextInstance : null,
                    icon: const Icon(Icons.skip_next),
                    tooltip: 'Next Instance',
                  ),
                ],
              ),
              const Divider(height: 4),
            ],
            Text('Video (${_videoMimeType ?? "unknown"})'),
            const SizedBox(height: 10),
            Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                IconButton(
                  onPressed: () {
                    setState(() {
                      _videoController!.value.isPlaying
                          ? _videoController!.pause()
                          : _videoController!.play();
                    });
                  },
                  icon: Icon(
                    _videoController!.value.isPlaying ? Icons.pause : Icons.play_arrow,
                    size: 32,
                  ),
                ),
                IconButton(
                  onPressed: () {
                    _videoController!.seekTo(Duration.zero);
                  },
                  icon: const Icon(Icons.replay, size: 32),
                ),
              ],
            ),
            const SizedBox(height: 10),
            Expanded(
              child: Center(
                child: AspectRatio(
                  aspectRatio: _videoController!.value.aspectRatio,
                  child: VideoPlayer(_videoController!),
                ),
              ),
            ),
            VideoProgressIndicator(_videoController!, allowScrubbing: true),
          ] else if (_dicomImage != null) ...[
            // Instance navigation (when multiple files downloaded via C-GET)
            if (_seriesFilePaths.length > 1) ...[
              Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  IconButton(
                    onPressed: _currentInstanceIndex > 0 ? _previousInstance : null,
                    icon: const Icon(Icons.skip_previous),
                    tooltip: 'Previous Instance',
                  ),
                  Text(
                    'Instance ${_currentInstanceIndex + 1} of ${_seriesFilePaths.length}',
                    style: const TextStyle(fontWeight: FontWeight.bold),
                  ),
                  IconButton(
                    onPressed: _currentInstanceIndex < _seriesFilePaths.length - 1 ? _nextInstance : null,
                    icon: const Icon(Icons.skip_next),
                    tooltip: 'Next Instance',
                  ),
                ],
              ),
              const Divider(height: 4),
            ],
            Text('Frame ${_currentFrame + 1} of $_totalFrames'),
            const SizedBox(height: 10),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceEvenly,
              children: [
                ElevatedButton(
                  onPressed: _currentFrame > 0 ? _previousFrame : null,
                  child: const Text('Previous Frame'),
                ),
                ElevatedButton(
                  onPressed: _currentFrame < _totalFrames - 1 ? _nextFrame : null,
                  child: const Text('Next Frame'),
                ),
              ],
            ),
            const SizedBox(height: 20),
            Expanded(
              child: CustomPaint(
                painter: ImagePainter(_dicomImage!),
                size: Size.infinite,
              ),
            ),
          ] else ...[
            Expanded(
              child: SingleChildScrollView(
                child: Container(
                  width: double.infinity,
                  padding: const EdgeInsets.all(16),
                  child: Text(
                    _result,
                    style: const TextStyle(fontFamily: 'monospace'),
                  ),
                ),
              ),
            ),
          ],
        ],
      ),
    );
  }

  Widget _buildExplorerTab() {
    return SingleChildScrollView(
      padding: const EdgeInsets.all(16.0),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Text(
            'DICOM Explorer',
            style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
          ),
          const SizedBox(height: 8),
          const Text(
            'Browse remote patient records and drill down to studies/series.',
            style: TextStyle(color: Colors.grey),
          ),
          const SizedBox(height: 12),
          if (!_serverConnected)
            Card(
              color: Colors.orange.shade50,
              child: Padding(
                padding: const EdgeInsets.all(12.0),
                child: Row(
                  children: [
                    const Icon(Icons.warning_amber, color: Colors.orange),
                    const SizedBox(width: 8),
                    const Expanded(
                      child: Text('Server not connected. Configure and test connection first.'),
                    ),
                    TextButton(
                      onPressed: () => _tabController.animateTo(_tabServer),
                      child: const Text('Open Server'),
                    ),
                  ],
                ),
              ),
            ),
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: [
              ElevatedButton.icon(
                onPressed: _serverConnected && !_queryingPatients ? _queryPatients : null,
                icon: const Icon(Icons.refresh),
                label: Text(_queryingPatients ? 'Refreshing...' : 'Refresh Patients'),
              ),
              ElevatedButton.icon(
                onPressed: _selectedPatient == null
                    ? null
                    : () => _tabController.animateTo(_tabMedia),
                icon: const Icon(Icons.upload),
                label: const Text('Use In Media Tab'),
              ),
            ],
          ),
          const SizedBox(height: 12),
          // -- Patients list --
          const Text('Patients', style: TextStyle(fontWeight: FontWeight.bold)),
          const SizedBox(height: 6),
          Container(
            height: 180,
            decoration: BoxDecoration(
              border: Border.all(color: Colors.grey.shade300),
              borderRadius: BorderRadius.circular(8),
            ),
            child: _patients.isEmpty
                ? Center(
                    child: Text(
                      _queryingPatients
                          ? 'Loading patients...'
                          : 'No patients loaded',
                      style: const TextStyle(color: Colors.grey),
                    ),
                  )
                : ListView.builder(
                    itemCount: _patients.length,
                    itemBuilder: (context, index) {
                      final patient = _patients[index];
                      final selected = _selectedPatient?.patientId == patient.patientId;
                      return ListTile(
                        selected: selected,
                        leading: const Icon(Icons.person),
                        title: Text(patient.patientName.isEmpty ? 'Unknown' : patient.patientName),
                        subtitle: Text('ID: ${patient.patientId}'),
                        trailing: selected ? const Icon(Icons.check_circle, color: Colors.blue) : null,
                        onTap: () {
                          setState(() {
                            _selectedPatient = patient;
                          });
                          _loadStudiesForPatient(patient.patientId);
                        },
                      );
                    },
                  ),
          ),
          const SizedBox(height: 12),
          // -- Patient details + Studies + Series --
          if (_selectedPatient != null) ...[
            Container(
              width: double.infinity,
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: Colors.blue.shade50,
                borderRadius: BorderRadius.circular(8),
                border: Border.all(color: Colors.blue.shade200),
              ),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const Text(
                    'Patient Details',
                    style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
                  ),
                  const SizedBox(height: 8),
                  Text('Name: ${_selectedPatient!.patientName.isEmpty ? 'Unknown' : _selectedPatient!.patientName}'),
                  const SizedBox(height: 4),
                  Text('ID: ${_selectedPatient!.patientId}'),
                  const SizedBox(height: 4),
                  Text('Birth Date: ${_selectedPatient!.patientBirthDate.isEmpty ? '-' : _selectedPatient!.patientBirthDate}'),
                  const SizedBox(height: 4),
                  Text('Sex: ${_selectedPatient!.patientSex.isEmpty ? '-' : _selectedPatient!.patientSex}'),
                  const SizedBox(height: 12),
                  Row(
                    children: [
                      Expanded(
                        child: ElevatedButton.icon(
                          onPressed: _queryingStudies
                              ? null
                              : () => _loadStudiesForPatient(_selectedPatient!.patientId),
                          icon: const Icon(Icons.folder_open, size: 16),
                          label: Text(_queryingStudies ? 'Loading...' : 'Load Studies'),
                        ),
                      ),
                    ],
                  ),
                ],
              ),
            ),
            const SizedBox(height: 12),
            // -- Studies --
            Text('Studies (${_studies.length})', style: const TextStyle(fontWeight: FontWeight.bold)),
            const SizedBox(height: 6),
            Container(
              height: 160,
              decoration: BoxDecoration(
                border: Border.all(color: Colors.grey.shade300),
                borderRadius: BorderRadius.circular(8),
              ),
              child: _studies.isEmpty
                  ? const Center(child: Text('No studies loaded', style: TextStyle(color: Colors.grey)))
                  : ListView.builder(
                      itemCount: _studies.length,
                      itemBuilder: (context, index) {
                        final study = _studies[index];
                        final isSelected = _selectedStudy?.studyInstanceUID == study.studyInstanceUID;
                        return ListTile(
                          dense: true,
                          selected: isSelected,
                          title: Text(study.studyDescription.isEmpty ? 'Study ${index + 1}' : study.studyDescription),
                          subtitle: Text(study.studyDate.isEmpty ? study.studyInstanceUID : study.studyDate),
                          trailing: isSelected ? const Icon(Icons.check_circle, color: Colors.blue, size: 18) : null,
                          onTap: () {
                            setState(() {
                              _selectedStudy = study;
                            });
                            _loadSeriesForStudy(study.studyInstanceUID);
                          },
                        );
                      },
                    ),
            ),
            const SizedBox(height: 12),
            // -- Series --
            Text('Series (${_series.length})', style: const TextStyle(fontWeight: FontWeight.bold)),
            const SizedBox(height: 6),
            Container(
              height: 160,
              decoration: BoxDecoration(
                border: Border.all(color: Colors.grey.shade300),
                borderRadius: BorderRadius.circular(8),
              ),
              child: _series.isEmpty
                  ? Center(
                      child: Text(
                        _queryingSeries ? 'Loading series...' : 'Select a study to load series',
                        style: const TextStyle(color: Colors.grey),
                        textAlign: TextAlign.center,
                      ),
                    )
                  : ListView.builder(
                      itemCount: _series.length,
                      itemBuilder: (context, index) {
                        final series = _series[index];
                        final isSelected = _selectedSeries?.seriesInstanceUID == series.seriesInstanceUID;
                        return ListTile(
                          dense: true,
                          selected: isSelected,
                          title: Text(series.seriesDescription.isEmpty ? 'Series ${index + 1}' : series.seriesDescription),
                          subtitle: Text('${series.modality}  #${series.seriesNumber}'),
                          trailing: isSelected ? const Icon(Icons.check_circle, color: Colors.blue, size: 18) : null,
                          onTap: () {
                            setState(() {
                              _selectedSeries = series;
                            });
                          },
                        );
                      },
                    ),
            ),
            const SizedBox(height: 16),
            // -- Action buttons --
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                OutlinedButton.icon(
                  onPressed: _openFilesFromExplorerSelection,
                  icon: const Icon(Icons.file_open),
                  label: const Text('Open Files Tab'),
                ),
                ElevatedButton.icon(
                  onPressed: (_selectedSeries == null || _downloadingFromExplorer)
                      ? null
                      : _downloadSelectedSeriesFirstInstance,
                  icon: const Icon(Icons.download),
                  label: Text(_downloadingFromExplorer
                      ? 'Downloading...'
                      : 'Download First Instance'),
                ),
                ElevatedButton.icon(
                  onPressed: (_selectedSeries == null || _downloadingViaDicom)
                      ? null
                      : _downloadSeriesViaCMove,
                  icon: const Icon(Icons.cloud_download),
                  label: Text(_downloadingViaDicom
                      ? 'Downloading...'
                      : 'Download via C-GET'),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.orange,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 8),
            const Text(
              'C-GET downloads files directly via native DICOM protocol. '
              '"Download First Instance" uses Orthanc HTTP API.',
              style: TextStyle(fontSize: 12, color: Colors.grey),
            ),
          ],
        ],
      ),
    );
  }

  Widget _buildServerTab() {
    return Padding(
      padding: const EdgeInsets.all(16.0),
      child: SingleChildScrollView(
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              'DICOM Server Configuration',
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 8),
            Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: Colors.blue[50],
                borderRadius: BorderRadius.circular(8),
                border: Border.all(color: Colors.blue[200]!),
              ),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: const [
                  Text('💡 Configuration Tips:', style: TextStyle(fontWeight: FontWeight.bold)),
                  SizedBox(height: 4),
                  Text('• Orthanc HTTP: port 8042 (web interface)'),
                  Text('• Orthanc DICOM: port 4242 (C-FIND/C-ECHO)'),
                  Text('• Check Orthanc.json for DicomPort setting'),
                  Text('• Ensure "DicomServerEnabled": true'),
                  SizedBox(height: 8),
                  Text('🔧 For C-FIND to work, add to Orthanc.json:', style: TextStyle(fontWeight: FontWeight.bold)),
                  Text('"QueryRetrieveEnabled": true,', style: TextStyle(fontFamily: 'monospace', fontSize: 12)),
                  Text('"DicomModalities": {', style: TextStyle(fontFamily: 'monospace', fontSize: 12)),
                  Text('  "FLUTTER_SCU": ["FLUTTER_SCU", "host.docker.internal", 4242]', style: TextStyle(fontFamily: 'monospace', fontSize: 12)),
                  Text('}', style: TextStyle(fontFamily: 'monospace', fontSize: 12)),
                ],
              ),
            ),
            const SizedBox(height: 16),
            TextField(
              controller: _serverHostController,
              decoration: const InputDecoration(
                labelText: 'Server Host',
                hintText: '127.0.0.1',
                border: OutlineInputBorder(),
              ),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: _serverPortController,
              decoration: const InputDecoration(
                labelText: 'DICOM Server Port',
                hintText: '4242 (default for Orthanc DICOM)',
                helperText: 'Not the HTTP port (8042)',
                border: OutlineInputBorder(),
              ),
              keyboardType: TextInputType.number,
            ),
            const SizedBox(height: 8),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              crossAxisAlignment: WrapCrossAlignment.center,
              children: [
                ElevatedButton.icon(
                  onPressed: () => _serverPortController.text = '4242',
                  icon: const Icon(Icons.settings, size: 16),
                  label: const Text('4242'),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.grey[200],
                    foregroundColor: Colors.black87,
                  ),
                ),
                ElevatedButton.icon(
                  onPressed: () => _serverPortController.text = '11112',
                  icon: const Icon(Icons.settings, size: 16),
                  label: const Text('11112'),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.grey[200],
                    foregroundColor: Colors.black87,
                  ),
                ),
                const Text('Common DICOM ports', style: TextStyle(color: Colors.grey)),
              ],
            ),
            const SizedBox(height: 12),
            TextField(
              controller: _httpHostController,
              decoration: const InputDecoration(
                labelText: 'Orthanc HTTP Host',
                hintText: '127.0.0.1',
                border: OutlineInputBorder(),
              ),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: _httpPortController,
              decoration: const InputDecoration(
                labelText: 'Orthanc HTTP Port',
                hintText: '8042',
                border: OutlineInputBorder(),
              ),
              keyboardType: TextInputType.number,
            ),
            const SizedBox(height: 12),
            TextField(
              controller: _aeTitleController,
              decoration: const InputDecoration(
                labelText: 'AE Title (Our)',
                hintText: 'FLUTTER_SCU',
                border: OutlineInputBorder(),
              ),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: _calledAeTitleController,
              decoration: const InputDecoration(
                labelText: 'Called AE Title (Server)',
                hintText: 'ORTHANC',
                border: OutlineInputBorder(),
              ),
            ),
            const SizedBox(height: 16),
            Container(
              width: double.infinity,
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: Colors.grey.shade50,
                borderRadius: BorderRadius.circular(8),
                border: Border.all(color: Colors.grey.shade300),
              ),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const Text(
                    'Connection Diagnostics',
                    style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
                  ),
                  const SizedBox(height: 8),
                  Wrap(
                    spacing: 12,
                    runSpacing: 8,
                    crossAxisAlignment: WrapCrossAlignment.center,
                    children: [
                      ElevatedButton.icon(
                        onPressed: _runningDiagnostics ? null : _runConnectionDiagnostics,
                        icon: const Icon(Icons.medical_services),
                        label: Text(_runningDiagnostics ? 'Running...' : 'Run Preflight'),
                      ),
                      Text(
                        _diagnosticsSummary,
                        style: const TextStyle(fontWeight: FontWeight.w500),
                      ),
                    ],
                  ),
                  if (_runningDiagnostics) ...[
                    const SizedBox(height: 8),
                    const LinearProgressIndicator(),
                  ],
                  const SizedBox(height: 8),
                  Text('C-ECHO: $_echoStatus'),
                  const SizedBox(height: 4),
                  Text('C-FIND: $_findStatus'),
                  if (_diagnosticsHint.isNotEmpty) ...[
                    const SizedBox(height: 8),
                    const Text(
                      'Hints:',
                      style: TextStyle(fontWeight: FontWeight.bold),
                    ),
                    const SizedBox(height: 4),
                    Text(_diagnosticsHint),
                  ],
                ],
              ),
            ),
            const SizedBox(height: 16),
            Row(
              children: [
                ElevatedButton(
                  onPressed: _testServerConnection,
                  child: const Text('Test Connection'),
                ),
                const SizedBox(width: 12),
                Icon(
                  _serverConnected ? Icons.check_circle : Icons.cancel,
                  color: _serverConnected ? Colors.green : Colors.red,
                ),
                const SizedBox(width: 8),
                Text(_serverConnected ? 'Connected' : 'Not Connected'),
              ],
            ),
            const SizedBox(height: 16),
            ElevatedButton(
              onPressed: _serverConnected && !_queryingPatients ? _queryPatients : null,
              child: Text(_queryingPatients ? 'Querying...' : 'Query Patients'),
            ),
            const SizedBox(height: 16),
            const Text(
              'Patients:',
              style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 8),
            SizedBox(
              height: 300, // Fixed height for patient list
              child: _patients.isEmpty
                  ? Center(
                      child: Text(
                        _queryingPatients ? 'Loading patients...' : 'No patients found',
                        style: const TextStyle(color: Colors.grey),
                      ),
                    )
                  : ListView.builder(
                      itemCount: _patients.length,
                      itemBuilder: (context, index) {
                        final patient = _patients[index];
                        return Card(
                          child: ListTile(
                            title: Text(patient.patientName.isEmpty ? 'Unknown' : patient.patientName),
                            subtitle: Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Text('ID: ${patient.patientId}'),
                                Text('DOB: ${patient.patientBirthDate}'),
                                Text('Sex: ${patient.patientSex}'),
                              ],
                            ),
                            isThreeLine: true,
                          ),
                        );
                      },
                    ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildPatientManagementTab() {
    return Padding(
      padding: const EdgeInsets.all(16.0),
      child: SingleChildScrollView(
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              'Patient Management',
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 16),
            
            // Create New Patient Section
            Container(
              padding: const EdgeInsets.all(16),
              decoration: BoxDecoration(
                border: Border.all(color: Colors.grey.shade300),
                borderRadius: BorderRadius.circular(8),
              ),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const Text(
                    'Create New Patient',
                    style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
                  ),
                  const SizedBox(height: 12),
                  TextField(
                    controller: _newPatientIdController,
                    decoration: const InputDecoration(
                      labelText: 'Patient ID *',
                      border: OutlineInputBorder(),
                    ),
                  ),
                  const SizedBox(height: 12),
                  TextField(
                    controller: _newPatientNameController,
                    decoration: const InputDecoration(
                      labelText: 'Patient Name *',
                      border: OutlineInputBorder(),
                    ),
                  ),
                  const SizedBox(height: 12),
                  TextField(
                    controller: _newPatientBirthDateController,
                    decoration: const InputDecoration(
                      labelText: 'Birth Date (YYYYMMDD)',
                      hintText: '19901225',
                      border: OutlineInputBorder(),
                    ),
                  ),
                  const SizedBox(height: 12),
                  TextField(
                    controller: _newPatientSexController,
                    decoration: const InputDecoration(
                      labelText: 'Sex (M/F/O)',
                      hintText: 'M',
                      border: OutlineInputBorder(),
                    ),
                  ),
                  const SizedBox(height: 16),
                  ElevatedButton(
                    onPressed: _uploading ? null : _createNewPatient,
                    child: Text(_uploading ? 'Creating...' : 'Create Patient'),
                  ),
                ],
              ),
            ),
            
            const SizedBox(height: 24),
            
            // Select Existing Patient Section
            const Text(
              'Select Patient for Media Upload',
              style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 12),
            
            if (_patients.isEmpty)
              Card(
                child: Padding(
                  padding: const EdgeInsets.all(16.0),
                  child: Column(
                    children: [
                      const Text('No patients available. Query patients first from the Server tab.'),
                      const SizedBox(height: 8),
                      ElevatedButton(
                        onPressed: () {
                          _tabController.animateTo(_tabServer); // Switch to Server tab
                        },
                        child: const Text('Go to Server Tab'),
                      ),
                    ],
                  ),
                ),
              )
            else
              Container(
                height: 200,
                decoration: BoxDecoration(
                  border: Border.all(color: Colors.grey.shade300),
                  borderRadius: BorderRadius.circular(8),
                ),
                child: ListView.builder(
                  itemCount: _patients.length,
                  itemBuilder: (context, index) {
                    final patient = _patients[index];
                    final isSelected = _selectedPatient?.patientId == patient.patientId;
                    
                    return Card(
                      color: isSelected ? Colors.blue.shade50 : null,
                      child: ListTile(
                        title: Text(patient.patientName.isEmpty ? 'Unknown' : patient.patientName),
                        subtitle: Text('ID: ${patient.patientId}'),
                        trailing: isSelected ? const Icon(Icons.check_circle, color: Colors.blue) : null,
                        onTap: () {
                          setState(() {
                            _selectedPatient = patient;
                          });
                        },
                      ),
                    );
                  },
                ),
              ),
              
            if (_selectedPatient != null) ...[
              const SizedBox(height: 16),
              Container(
                padding: const EdgeInsets.all(12),
                decoration: BoxDecoration(
                  color: Colors.green.shade50,
                  border: Border.all(color: Colors.green.shade200),
                  borderRadius: BorderRadius.circular(8),
                ),
                child: Row(
                  children: [
                    const Icon(Icons.check_circle, color: Colors.green),
                    const SizedBox(width: 8),
                    Expanded(
                      child: Text(
                        'Selected: ${_selectedPatient!.patientName} (${_selectedPatient!.patientId})',
                        style: const TextStyle(fontWeight: FontWeight.bold),
                      ),
                    ),
                  ],
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }

  Widget _buildMediaUploadTab() {
    return Padding(
      padding: const EdgeInsets.all(16.0),
      child: SingleChildScrollView(
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              'Media Upload',
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 16),
            
            if (_selectedPatient == null)
              Container(
                padding: const EdgeInsets.all(16),
                decoration: BoxDecoration(
                  color: Colors.orange.shade50,
                  border: Border.all(color: Colors.orange.shade200),
                  borderRadius: BorderRadius.circular(8),
                ),
                child: Column(
                  children: [
                    const Icon(Icons.info_outline, color: Colors.orange, size: 32),
                    const SizedBox(height: 8),
                    const Text(
                      'Please select a patient first',
                      style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
                    ),
                    const SizedBox(height: 8),
                    const Text(
                      'Go to the Patients tab to select an existing patient or create a new one.',
                      textAlign: TextAlign.center,
                    ),
                    const SizedBox(height: 12),
                    ElevatedButton(
                      onPressed: () {
                        _tabController.animateTo(_tabPatients); // Switch to Patients tab
                      },
                      child: const Text('Go to Patients Tab'),
                    ),
                  ],
                ),
              )
            else ...[
              Container(
                padding: const EdgeInsets.all(12),
                decoration: BoxDecoration(
                  color: Colors.blue.shade50,
                  border: Border.all(color: Colors.blue.shade200),
                  borderRadius: BorderRadius.circular(8),
                ),
                child: Row(
                  children: [
                    const Icon(Icons.person, color: Colors.blue),
                    const SizedBox(width: 8),
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          const Text('Selected Patient:', style: TextStyle(fontWeight: FontWeight.bold)),
                          Text('${_selectedPatient!.patientName} (${_selectedPatient!.patientId})'),
                        ],
                      ),
                    ),
                  ],
                ),
              ),
              
              const SizedBox(height: 24),
              
              // Upload Metadata Section
              const Text(
                'Upload Metadata',
                style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
              ),
              const SizedBox(height: 12),
              
              // Study Description
              TextField(
                controller: _studyDescriptionController,
                decoration: const InputDecoration(
                  labelText: 'Study Description',
                  hintText: 'e.g., Mobile App Upload',
                  border: OutlineInputBorder(),
                ),
              ),
              const SizedBox(height: 12),
              
              // Series Description
              TextField(
                controller: _seriesDescriptionController,
                decoration: const InputDecoration(
                  labelText: 'Series Description',
                  hintText: 'e.g., Uploaded Images',
                  border: OutlineInputBorder(),
                ),
              ),
              const SizedBox(height: 12),
              
              // Image Comments
              TextField(
                controller: _imageCommentsController,
                maxLines: 3,
                decoration: const InputDecoration(
                  labelText: 'Image Comments (Optional)',
                  hintText: 'Additional notes about this image...',
                  border: OutlineInputBorder(),
                ),
              ),
              const SizedBox(height: 12),
              
              // Modality
              TextField(
                controller: _modalityController,
                decoration: const InputDecoration(
                  labelText: 'Modality',
                  hintText: 'e.g., SC (Secondary Capture)',
                  border: OutlineInputBorder(),
                ),
              ),
              
              const SizedBox(height: 24),
              
              // Upload Buttons
              Row(
                children: [
                  Expanded(
                    child: ElevatedButton.icon(
                      onPressed: _uploading ? null : _uploadImageForPatient,
                      icon: const Icon(Icons.image),
                      label: Text(_uploading ? 'Uploading...' : 'Upload Image'),
                    ),
                  ),
                  const SizedBox(width: 16),
                  Expanded(
                    child: ElevatedButton.icon(
                      onPressed: _uploading ? null : _uploadVideoForPatient,
                      icon: const Icon(Icons.video_library),
                      label: Text(_uploading ? 'Uploading...' : 'Upload Video'),
                    ),
                  ),
                ],
              ),
              
              const SizedBox(height: 24),
              
              // Upload History
              const Text(
                'Upload History',
                style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
              ),
              const SizedBox(height: 8),
              
              if (_uploadedMedia.isEmpty)
                const Card(
                  child: Padding(
                    padding: EdgeInsets.all(16.0),
                    child: Text(
                      'No media uploaded yet',
                      style: TextStyle(color: Colors.grey),
                    ),
                  ),
                )
              else
                Container(
                  height: 200,
                  decoration: BoxDecoration(
                    border: Border.all(color: Colors.grey.shade300),
                    borderRadius: BorderRadius.circular(8),
                  ),
                  child: ListView.builder(
                    itemCount: _uploadedMedia.length,
                    itemBuilder: (context, index) {
                      return ListTile(
                        leading: const Icon(Icons.check_circle, color: Colors.green),
                        title: Text(_uploadedMedia[index]),
                        subtitle: Text('Uploaded to: ${_selectedPatient!.patientName}'),
                      );
                    },
                  ),
                ),
            ],
          ],
        ),
      ),
    );
  }
}

class ImagePainter extends CustomPainter {
  final ui.Image image;

  ImagePainter(this.image);

  @override
  void paint(Canvas canvas, Size size) {
    // Calculate how to fit the image within the available space
    final double imageAspectRatio = image.width / image.height;
    final double canvasAspectRatio = size.width / size.height;

    double drawWidth, drawHeight;
    if (imageAspectRatio > canvasAspectRatio) {
      // Image is wider than canvas
      drawWidth = size.width;
      drawHeight = size.width / imageAspectRatio;
    } else {
      // Image is taller than canvas
      drawHeight = size.height;
      drawWidth = size.height * imageAspectRatio;
    }

    final double dx = (size.width - drawWidth) / 2;
    final double dy = (size.height - drawHeight) / 2;

    final srcRect = Rect.fromLTWH(0, 0, image.width.toDouble(), image.height.toDouble());
    final dstRect = Rect.fromLTWH(dx, dy, drawWidth, drawHeight);

    canvas.drawImageRect(image, srcRect, dstRect, Paint());
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) {
    return oldDelegate is! ImagePainter || oldDelegate.image != image;
  }
}