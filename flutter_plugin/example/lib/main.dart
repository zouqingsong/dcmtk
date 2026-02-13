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

  // Patient management
  DicomPatient? _selectedPatient;
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
    _tabController = TabController(length: 4, vsync: this);  // Updated to 4 tabs
  }

  @override
  void dispose() {
    _tabController.dispose();
    _serverHostController.dispose();
    _serverPortController.dispose();
    _aeTitleController.dispose();
    _calledAeTitleController.dispose();
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

  Future<void> _pickAndLoadDicomFile() async {
    FilePickerResult? result = await FilePicker.platform.pickFiles(
      type: FileType.custom,
      allowedExtensions: ['dcm', 'dicom', 'DCM', 'DICOM'],
    );

    if (result != null && result.files.isNotEmpty) {
      final String filePath = result.files.first.path!;
      await _loadDicomFile(filePath);
    }
  }

  Future<void> _loadDicomFile(String filePath) async {
    setState(() {
      _loading = true;
      _currentFilePath = filePath;
      _dicomImage = null;
    });

    try {
      print('Loading DICOM file: $filePath');
      final String dicomInfo = await _dcmtk.loadDicomFile(filePath);
      print('DICOM info loaded: ${dicomInfo.substring(0, dicomInfo.length > 500 ? 500 : dicomInfo.length)}...');

      // Parse frame count from DICOM info
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
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Error querying patients: $e')),
      );
    }
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

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(widget.title),
        bottom: TabBar(
          controller: _tabController,
          tabs: const [
            Tab(icon: Icon(Icons.file_open), text: 'Files'),
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
      child: SingleChildScrollView(
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              'DICOM Server Connection',
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
                  Text('  "FLUTTER": ["FLUTTER", "localhost", 4242]', style: TextStyle(fontFamily: 'monospace', fontSize: 12)),
                  Text('}', style: TextStyle(fontFamily: 'monospace', fontSize: 12)),
                ],
              ),
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
                labelText: 'DICOM Server Port',
                hintText: '4242 (default for Orthanc DICOM)',
                helperText: 'Not the HTTP port (8042)',
                border: OutlineInputBorder(),
              ),
              keyboardType: TextInputType.number,
            ),
            const SizedBox(height: 8),
            Row(
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
                const SizedBox(width: 8),
                ElevatedButton.icon(
                  onPressed: () => _serverPortController.text = '11112',
                  icon: const Icon(Icons.settings, size: 16),
                  label: const Text('11112'),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.grey[200],
                    foregroundColor: Colors.black87,
                  ),
                ),
                const SizedBox(width: 8),
                const Text('← Common DICOM ports', style: TextStyle(color: Colors.grey)),
              ],
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
                          _tabController.animateTo(1); // Switch to Server tab
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
                        _tabController.animateTo(2); // Switch to Patients tab
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
                      onPressed: _uploading ? null : () {
                        ScaffoldMessenger.of(context).showSnackBar(
                          const SnackBar(content: Text('Video upload feature coming soon!')),
                        );
                      },
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