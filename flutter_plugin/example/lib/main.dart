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

class _MyHomePageState extends State<MyHomePage> {
  final DcmtkFlutter _dcmtk = DcmtkFlutter();
  String _result = 'No DICOM file loaded';
  bool _loading = false;

  Future<void> _pickAndLoadDicomFile() async {
    FilePickerResult? result = await FilePicker.platform.pickFiles(
      type: FileType.custom,
      allowedExtensions: ['dcm', 'dicom'],
    );

    if (result != null && result.files.single.path != null) {
      setState(() {
        _loading = true;
      });

      try {
        String filePath = result.files.single.path!;
        String dicomInfo = await _dcmtk.loadDicomFile(filePath);
        
        setState(() {
          _result = dicomInfo;
          _loading = false;
        });
      } catch (e) {
        setState(() {
          _result = 'Error loading DICOM file: $e';
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
      ),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: <Widget>[
            ElevatedButton(
              onPressed: _loading ? null : _pickAndLoadDicomFile,
              child: _loading
                  ? const CircularProgressIndicator()
                  : const Text('Pick DICOM File'),
            ),
            const SizedBox(height: 20),
            const Text(
              'DICOM File Information:',
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 10),
            Expanded(
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