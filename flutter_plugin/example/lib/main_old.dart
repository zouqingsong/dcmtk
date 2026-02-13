import 'dart:async';
import 'dart:typed_data';
import 'dart:ui' as ui;
import 'package:flutter/material.dart';
import 'package:dcmtk_flutter/dcmtk_flutter.dart';
import 'package:file_picker/file_picker.dart';

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
  final DcmtkFlutter _dcmtk = DcmtkFlutter();
  String _result = 'No DICOM file loaded';
  bool _loading = false;
  ui.Image? _dicomImage;
  String? _currentFilePath;
  int _currentFrame = 0;
  int _totalFrames = 1;
  
  // Tab controller for File/Server tabs
  late TabController _tabController;
  
  // DICOM Server settings
  final TextEditingController _serverHostController = TextEditingController(text: 'localhost');
  final TextEditingController _serverPortController = TextEditingController(text: '4242');
  final TextEditingController _aeTitleController = TextEditingController(text: 'FLUTTER_SCU');
  final TextEditingController _calledAeTitleController = TextEditingController(text: 'ORTHANC');
  bool _serverConnected = false;
  List<DicomPatient> _patients = [];
  bool _queryingPatients = false;

  @override
  void initState() {
    super.initState();
    _tabController = TabController(length: 2, vsync: this);
  }

  @override
  void dispose() {
    _tabController.dispose();
    _serverHostController.dispose();
    _serverPortController.dispose();
    _aeTitleController.dispose();
    _calledAeTitleController.dispose();
    super.dispose();
  }

  Future<void> _pickAndLoadDicomFile() async {
    FilePickerResult? result = await FilePicker.platform.pickFiles(
      type: FileType.custom,
      allowedExtensions: ['dcm', 'dicom', 'DCM', 'DICOM'],
    );

    if (result != null && result.files.isNotEmpty && result.files.first.path != null) {
      setState(() {
        _loading = true;
        _dicomImage = null;
      });

      try {
        String filePath = result.files.first.path!;
        _currentFilePath = filePath;
        
        print('=== DICOM File Loading ===');
        print('File path: $filePath');
        
        // Load DICOM info
        String dicomInfo = await _dcmtk.loadDicomFile(filePath);
        
        print('=== DICOM File Information ===');
        print(dicomInfo);
        print('==============================');
        
        // Parse number of frames from DICOM info
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
  }

  Future<void> _loadFrame(String filePath, int frameIndex) async {
    try {
      print('--- Loading frame $frameIndex from: $filePath');
      
      final imageData = await _dcmtk.extractImage(filePath, frameIndex: frameIndex);
      ui.Image? image;
      
      if (imageData != null) {
        print('Image data received: ${imageData['width']}x${imageData['height']}, ${(imageData['data'] as Uint8List).length} bytes');
        
        image = await _convertGrayscaleToImage(
          imageData['data'] as Uint8List,
          imageData['width'] as int,
          imageData['height'] as int,
        );
        
        print('Successfully converted to UI image');
      } else {
        print('WARNING: extractImage returned null');
      }
      
      setState(() {
        _dicomImage = image;
        _currentFrame = frameIndex;
      });
      
      print('Frame $frameIndex loaded successfully');
    } catch (e) {
      print('ERROR loading frame $frameIndex: $e');
      
      setState(() {
        _result = 'Error extracting image: $e';
        _dicomImage = null;
      });
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

  Future<ui.Image> _convertGrayscaleToImage(Uint8List grayscaleData, int width, int height) async {
    // Convert grayscale to RGBA
    final rgbaData = Uint8List(width * height * 4);
    for (int i = 0; i < grayscaleData.length; i++) {
      final gray = grayscaleData[i];
      rgbaData[i * 4] = gray;     // R
      rgbaData[i * 4 + 1] = gray; // G
      rgbaData[i * 4 + 2] = gray; // B
      rgbaData[i * 4 + 3] = 255;  // A
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
    });

    final serverHost = _serverHostController.text.trim();
    final serverPort = int.tryParse(_serverPortController.text.trim()) ?? 4242;
    final aeTitle = _aeTitleController.text.trim();
    final calledAeTitle = _calledAeTitleController.text.trim();

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
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Error querying patients: $e')),
      );
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
            Tab(icon: Icon(Icons.file_open), text: 'File Loading'),
            Tab(icon: Icon(Icons.cloud), text: 'Server'),
          ],
        ),
      ),
      body: TabBarView(
        controller: _tabController,
        children: [
          _buildFileLoadingTab(),
          _buildServerTab(),
        ],
      ),
    );
  }

  Widget _buildFileLoadingTab() {
    return Padding(
      padding: const EdgeInsets.all(16.0),
      child: Column(
        children: [
          ElevatedButton(
            onPressed: _loading ? null : _pickAndLoadDicomFile,
            child: Text(_loading ? 'Loading...' : 'Pick DICOM File'),
          ),
          const SizedBox(height: 20),
          if (_dicomImage != null) ...[
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

  Widget _buildServerTab() {
    return Padding(
      padding: const EdgeInsets.all(16.0),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Text(
            'DICOM Server Connection',
            style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
          ),
          const SizedBox(height: 16),
          TextField(
            controller: _serverHostController,
            decoration: const InputDecoration(
              labelText: 'Server Host',
              hintText: 'localhost',
              border: OutlineInputBorder(),
            ),
          ),
          const SizedBox(height: 12),
          TextField(
            controller: _serverPortController,
            decoration: const InputDecoration(
              labelText: 'Server Port',
              hintText: '4242',
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
          Expanded(
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
    );
  }
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: <Widget>[
            ElevatedButton(
              onPressed: _loading ? null : _pickAndLoadDicomFile,
              child: _loading
                  ? const SizedBox(
                      height: 20,
                      width: 20,
                      child: CircularProgressIndicator(strokeWidth: 2),
                    )
                  : const Text('Pick DICOM File'),
            ),
            const SizedBox(height: 20),
            if (_dicomImage != null) ...[
              Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  const Text(
                    'DICOM Image:',
                    style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
                  ),
                  if (_totalFrames > 1)
                    Text(
                      'Frame ${_currentFrame + 1} / $_totalFrames',
                      style: const TextStyle(fontSize: 14, fontWeight: FontWeight.w500),
                    ),
                ],
              ),
              const SizedBox(height: 10),
              Expanded(
                flex: 2,
                child: Container(
                  decoration: BoxDecoration(
                    border: Border.all(color: Colors.grey),
                    borderRadius: BorderRadius.circular(8),
                    color: Colors.black,
                  ),
                  child: Center(
                    child: RawImage(
                      image: _dicomImage,
                      fit: BoxFit.contain,
                    ),
                  ),
                ),
              ),
              if (_totalFrames > 1) ...[
                const SizedBox(height: 10),
                Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    IconButton(
                      icon: const Icon(Icons.arrow_back),
                      onPressed: _currentFrame > 0 ? _previousFrame : null,
                      tooltip: 'Previous frame',
                    ),
                    const SizedBox(width: 20),
                    IconButton(
                      icon: const Icon(Icons.arrow_forward),
                      onPressed: _currentFrame < _totalFrames - 1 ? _nextFrame : null,
                      tooltip: 'Next frame',
                    ),
                  ],
                ),
              ],
              const SizedBox(height: 20),
            ],
            const Text(
              'DICOM File Information:',
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 10),
            Expanded(
              flex: _dicomImage != null ? 1 : 3,
              child: Container(
                decoration: BoxDecoration(
                  border: Border.all(color: Colors.grey),
                  borderRadius: BorderRadius.circular(8),
                ),
                padding: const EdgeInsets.all(12),
                child: SingleChildScrollView(
                  child: Text(
                    _result,
                    style: const TextStyle(fontFamily: 'monospace'),
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}