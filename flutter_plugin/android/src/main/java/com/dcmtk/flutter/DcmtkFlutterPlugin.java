package com.dcmtk.flutter;

import android.content.Context;
import android.content.res.AssetManager;
import android.util.Log;

import androidx.annotation.NonNull;

import io.flutter.embedding.engine.plugins.FlutterPlugin;
import io.flutter.plugin.common.MethodCall;
import io.flutter.plugin.common.MethodChannel;
import io.flutter.plugin.common.MethodChannel.MethodCallHandler;
import io.flutter.plugin.common.MethodChannel.Result;

import java.util.ArrayList;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** DcmtkFlutterPlugin */
public class DcmtkFlutterPlugin implements FlutterPlugin, MethodCallHandler {
  private static final String TAG = "DcmtkFlutter";
  private MethodChannel channel;
  private Context applicationContext;

  static {
    System.loadLibrary("dcmtk_flutter");
  }

  // ==================== Native method declarations ====================
  private native String nativeLoadDicomFile(String filePath);
  private native String nativeGetVersion();
  private native boolean nativeValidateDicomFile(String filePath);
  private native String nativeGetDicomTag(String filePath, String tagName);
  private native HashMap<String, Object> nativeExtractImage(String filePath, int frameIndex, double windowCenter, double windowWidth);
  private native boolean nativeTestServerConnection(String serverHost, int serverPort, String aeTitle, String calledAeTitle);
  private native int nativeTestServerConnectionTls(String serverHost, int serverPort, String aeTitle, String calledAeTitle, String certFile, String keyFile, String caFile);
  private native boolean nativeInitDictionary(String dictionaryPath);
  private native HashMap<String, Object> nativeQueryPatients(String serverHost, int serverPort, String aeTitle, String calledAeTitle, String patientNameFilter);
  private native HashMap<String, Object> nativeQueryStudiesForPatient(String serverHost, int serverPort, String aeTitle, String calledAeTitle, String patientId);
  private native HashMap<String, Object> nativeQuerySeriesForStudy(String serverHost, int serverPort, String aeTitle, String calledAeTitle, String studyInstanceUID);
  private native HashMap<String, Object> nativeQueryInstancesForSeries(String serverHost, int serverPort, String aeTitle, String calledAeTitle, String seriesInstanceUID);
  private native HashMap<String, Object> nativeCreatePatient(String serverHost, int serverPort, String aeTitle, String calledAeTitle, String patientId, String patientName, String birthDate, String sex, String comments);
  private native HashMap<String, Object> nativeUploadImage(String serverHost, int serverPort, String aeTitle, String calledAeTitle, String patientId, String imagePath, String patientName, String patientBirthDate, String studyDescription, String seriesDescription, String imageComments, String modality, String studyInstanceUID, String seriesInstanceUID, int instanceNumber);
  private native HashMap<String, Object> nativeUploadMultiframe(String serverHost, int serverPort, String aeTitle, String calledAeTitle, String patientId, String[] imagePaths, String patientName, String patientBirthDate, String studyDescription, String seriesDescription, String imageComments, String modality, String studyInstanceUID, String seriesInstanceUID);
  private native HashMap<String, Object> nativeUploadVideo(String serverHost, int serverPort, String aeTitle, String calledAeTitle, String patientId, String videoPath, String patientName, String patientBirthDate, String studyDescription, String seriesDescription, String imageComments, String modality);
  private native HashMap<String, Object> nativeDownloadInstances(String serverHost, int serverPort, String aeTitle, String calledAeTitle, String seriesInstanceUID, String localStoragePath);
  private native HashMap<String, Object> nativeMoveInstances(String serverHost, int serverPort, String aeTitle, String calledAeTitle, String seriesInstanceUID, String localStoragePath, int moveSCPPort);
  private native HashMap<String, Object> nativeExtractVideo(String dicomPath, String outputPath);
  private native HashMap<String, Object> nativeStoreFiles(String serverHost, int serverPort, String aeTitle, String calledAeTitle, String[] filePaths);
  private native boolean nativeStartStoreSCP(int port, String aeTitle, String storageDir);
  private native void nativeStopStoreSCP();
  private native HashMap<String, Object> nativeGetStoreSCPStatus();
  private native void nativeSetTlsConfig(String certFile, String keyFile, String caFile);
  private native void nativeClearTlsConfig();
  private native boolean nativeIsTlsAvailable();
  private native boolean nativeIsTlsEnabled();
  private native HashMap<String, Object> nativeBuildMprVolume(String[] filePaths);
  private native HashMap<String, Object> nativeGetMprSlice(int volumeId, int plane, int sliceIndex, double windowCenter, double windowWidth);
  private native void nativeFreeMprVolume(int volumeId);
  private native HashMap<String, Object> nativeRenderMip(int volumeId, double rotationX, double rotationY, double windowCenter, double windowWidth);
  private native HashMap<String, Object> nativeRenderVolume(int volumeId, double rotationX, double rotationY, double windowCenter, double windowWidth, String preset, boolean preview);
  private native HashMap<String, Object> nativeCreateGsps(String sourceDicomPath, String annotationsJson, String outputPath);
  private native String nativeParseGsps(String filePath);
  private native HashMap<String, Object> nativeConvertImageToDicom(String imagePath, String outputPath, String patientId, String patientName, String patientBirthDate, String studyDescription, String seriesDescription, String imageComments, String modality, String studyInstanceUID, String seriesInstanceUID, int instanceNumber);

  @Override
  public void onAttachedToEngine(@NonNull FlutterPluginBinding flutterPluginBinding) {
    applicationContext = flutterPluginBinding.getApplicationContext();
    channel = new MethodChannel(flutterPluginBinding.getBinaryMessenger(), "dcmtk_flutter");
    channel.setMethodCallHandler(this);
    initializeDictionary();
  }

  private void initializeDictionary() {
    if (applicationContext == null) {
      Log.e(TAG, "Cannot initialize dictionary: application context is null");
      return;
    }

    final File dcmtkDir = new File(applicationContext.getFilesDir(), "dcmtk");
    if (!dcmtkDir.exists() && !dcmtkDir.mkdirs()) {
      Log.e(TAG, "Failed to create DCMTK files dir: " + dcmtkDir.getAbsolutePath());
      return;
    }

    final File dictionaryFile = new File(dcmtkDir, "dicom.dic");
    try {
      copyAssetIfNeeded(applicationContext.getAssets(), "dcmtk/dicom.dic", dictionaryFile);
      if (!nativeInitDictionary(dictionaryFile.getAbsolutePath())) {
        Log.e(TAG, "Failed to initialize DCMTK dictionary from " + dictionaryFile.getAbsolutePath());
      }
    } catch (IOException e) {
      Log.e(TAG, "Failed to copy DCMTK dictionary asset", e);
    }
  }

  private static void copyAssetIfNeeded(AssetManager assetManager, String assetPath, File destination) throws IOException {
    boolean shouldCopy = !destination.exists();
    if (!shouldCopy) {
      try (InputStream in = assetManager.open(assetPath)) {
        shouldCopy = destination.length() != in.available();
      }
    }

    if (!shouldCopy) {
      return;
    }

    try (InputStream in = assetManager.open(assetPath);
         FileOutputStream out = new FileOutputStream(destination, false)) {
      byte[] buffer = new byte[8192];
      int read;
      while ((read = in.read(buffer)) != -1) {
        out.write(buffer, 0, read);
      }
      out.flush();
    }
  }

  @Override
  public void onMethodCall(@NonNull MethodCall call, @NonNull Result result) {
    switch (call.method) {
      case "loadDicomFile":
        handleLoadDicomFile(call, result);
        break;
      case "extractImage":
        handleExtractImage(call, result);
        break;
      case "testServerConnection":
        handleTestServerConnection(call, result);
        break;
      case "testServerConnectionTls":
        handleTestServerConnectionTls(call, result);
        break;
      case "queryPatients":
        handleQueryPatients(call, result);
        break;
      case "queryStudiesForPatient":
        handleQueryStudiesForPatient(call, result);
        break;
      case "querySeriesForStudy":
        handleQuerySeriesForStudy(call, result);
        break;
      case "queryInstancesForSeries":
        handleQueryInstancesForSeries(call, result);
        break;
      case "downloadInstancesViaCMove":
        handleDownloadInstances(call, result);
        break;
      case "createPatient":
        handleCreatePatient(call, result);
        break;
      case "uploadImage":
        handleUploadImage(call, result);
        break;
      case "uploadMultiframe":
        handleUploadMultiframe(call, result);
        break;
      case "uploadVideo":
        handleUploadVideo(call, result);
        break;
      case "getDicomTag":
        handleGetDicomTag(call, result);
        break;
      case "validateDicomFile":
        handleValidateDicomFile(call, result);
        break;
      case "extractVideo":
        handleExtractVideo(call, result);
        break;
      case "storeFiles":
        handleStoreFiles(call, result);
        break;
      case "startStoreSCP":
        handleStartStoreSCP(call, result);
        break;
      case "stopStoreSCP":
        handleStopStoreSCP(call, result);
        break;
      case "getStoreSCPStatus":
        handleGetStoreSCPStatus(call, result);
        break;
      case "moveInstances":
        handleMoveInstances(call, result);
        break;
      case "setTlsConfig":
        handleSetTlsConfig(call, result);
        break;
      case "clearTlsConfig":
        handleClearTlsConfig(call, result);
        break;
      case "isTlsAvailable":
        result.success(nativeIsTlsAvailable());
        break;
      case "isTlsEnabled":
        result.success(nativeIsTlsEnabled());
        break;
      case "buildMprVolume":
        handleBuildMprVolume(call, result);
        break;
      case "getMprSlice":
        handleGetMprSlice(call, result);
        break;
      case "freeMprVolume":
        handleFreeMprVolume(call, result);
        break;
      case "renderMip":
        handleRenderMip(call, result);
        break;
      case "renderVolume":
        handleRenderVolume(call, result);
        break;
      case "createGsps":
        handleCreateGsps(call, result);
        break;
      case "parseGsps":
        handleParseGsps(call, result);
        break;
      case "convertImageToDicom":
        handleConvertImageToDicom(call, result);
        break;
      default:
        result.notImplemented();
    }
  }

  // ==================== Handler methods ====================

  private void handleLoadDicomFile(MethodCall call, Result result) {
    String filePath = call.argument("filePath");
    if (filePath == null || filePath.isEmpty()) {
      result.error("INVALID_ARGUMENT", "File path is required", null);
      return;
    }
    String jsonResult = nativeLoadDicomFile(filePath);
    result.success(jsonResult);
  }

  private void handleExtractImage(MethodCall call, Result result) {
    String filePath = call.argument("filePath");
    if (filePath == null || filePath.isEmpty()) {
      result.error("INVALID_ARGUMENT", "File path is required", null);
      return;
    }
    Number frameIndexNum = call.argument("frameIndex");
    Number windowCenterNum = call.argument("windowCenter");
    Number windowWidthNum = call.argument("windowWidth");
    int frameIndex = frameIndexNum != null ? frameIndexNum.intValue() : 0;
    double windowCenter = windowCenterNum != null ? windowCenterNum.doubleValue() : 0.0;
    double windowWidth = windowWidthNum != null ? windowWidthNum.doubleValue() : 0.0;

    HashMap<String, Object> imgResult = nativeExtractImage(filePath, frameIndex, windowCenter, windowWidth);
    if (imgResult.containsKey("error")) {
      result.error("EXTRACTION_ERROR", (String) imgResult.get("error"), null);
      return;
    }
    result.success(imgResult);
  }

  private void handleTestServerConnection(MethodCall call, Result result) {
    String serverHost = call.argument("serverHost");
    Number serverPort = call.argument("serverPort");
    String aeTitle = call.argument("aeTitle");
    String calledAeTitle = call.argument("calledAeTitle");
    if (serverHost == null || serverPort == null || aeTitle == null || calledAeTitle == null) {
      result.error("INVALID_ARGUMENT", "Server host, port, AE title, and called AE title are required", null);
      return;
    }
    boolean connected = nativeTestServerConnection(serverHost, serverPort.intValue(), aeTitle, calledAeTitle);
    result.success(connected);
  }

  private void handleTestServerConnectionTls(MethodCall call, Result result) {
    String serverHost = call.argument("serverHost");
    Number serverPort = call.argument("serverPort");
    String aeTitle = call.argument("aeTitle");
    String calledAeTitle = call.argument("calledAeTitle");
    String certFile = call.argument("certFile");
    String keyFile = call.argument("keyFile");
    String caFile = call.argument("caFile");
    if (serverHost == null || serverPort == null || aeTitle == null || calledAeTitle == null) {
      result.error("INVALID_ARGUMENT", "Server host, port, AE title, and called AE title are required", null);
      return;
    }
    int tlsResult = nativeTestServerConnectionTls(serverHost, serverPort.intValue(), aeTitle, calledAeTitle,
            certFile != null ? certFile : "", keyFile != null ? keyFile : "", caFile != null ? caFile : "");
    if (tlsResult == -1) {
      result.error("TLS_NOT_AVAILABLE", "TLS support not compiled (OpenSSL not available)", null);
    } else {
      result.success(tlsResult == 1);
    }
  }

  private void handleQueryPatients(MethodCall call, Result result) {
    String serverHost = call.argument("serverHost");
    Number serverPort = call.argument("serverPort");
    String aeTitle = call.argument("aeTitle");
    String calledAeTitle = call.argument("calledAeTitle");
    String patientNameFilter = call.argument("patientNameFilter");
    if (serverHost == null || serverPort == null || aeTitle == null || calledAeTitle == null) {
      result.error("INVALID_ARGUMENT", "Server host, port, AE title, and called AE title are required", null);
      return;
    }
    HashMap<String, Object> queryResult = nativeQueryPatients(serverHost, serverPort.intValue(), aeTitle, calledAeTitle, patientNameFilter);
    if (queryResult.containsKey("error")) {
      result.error("QUERY_ERROR", (String) queryResult.get("error"), null);
      return;
    }
    result.success(queryResult.get("patients"));
  }

  private void handleQueryStudiesForPatient(MethodCall call, Result result) {
    String serverHost = call.argument("serverHost");
    Number serverPort = call.argument("serverPort");
    String aeTitle = call.argument("aeTitle");
    String calledAeTitle = call.argument("calledAeTitle");
    String patientId = call.argument("patientId");
    if (serverHost == null || serverPort == null || aeTitle == null || calledAeTitle == null || patientId == null) {
      result.error("INVALID_ARGUMENT", "Server host, port, AE title, called AE title, and patient ID are required", null);
      return;
    }
    HashMap<String, Object> queryResult = nativeQueryStudiesForPatient(serverHost, serverPort.intValue(), aeTitle, calledAeTitle, patientId);
    if (queryResult.containsKey("error")) {
      result.error("QUERY_ERROR", (String) queryResult.get("error"), null);
      return;
    }
    result.success(queryResult.get("studies"));
  }

  private void handleQuerySeriesForStudy(MethodCall call, Result result) {
    String serverHost = call.argument("serverHost");
    Number serverPort = call.argument("serverPort");
    String aeTitle = call.argument("aeTitle");
    String calledAeTitle = call.argument("calledAeTitle");
    String studyInstanceUID = call.argument("studyInstanceUID");
    if (serverHost == null || serverPort == null || aeTitle == null || calledAeTitle == null || studyInstanceUID == null) {
      result.error("INVALID_ARGUMENT", "Server host, port, AE title, called AE title, and study instance UID are required", null);
      return;
    }
    HashMap<String, Object> queryResult = nativeQuerySeriesForStudy(serverHost, serverPort.intValue(), aeTitle, calledAeTitle, studyInstanceUID);
    if (queryResult.containsKey("error")) {
      result.error("QUERY_ERROR", (String) queryResult.get("error"), null);
      return;
    }
    result.success(queryResult.get("series"));
  }

  private void handleQueryInstancesForSeries(MethodCall call, Result result) {
    String serverHost = call.argument("serverHost");
    Number serverPort = call.argument("serverPort");
    String aeTitle = call.argument("aeTitle");
    String calledAeTitle = call.argument("calledAeTitle");
    String seriesInstanceUID = call.argument("seriesInstanceUID");
    if (serverHost == null || serverPort == null || aeTitle == null || calledAeTitle == null || seriesInstanceUID == null) {
      result.error("INVALID_ARGUMENT", "Server host, port, AE title, called AE title, and series instance UID are required", null);
      return;
    }
    HashMap<String, Object> queryResult = nativeQueryInstancesForSeries(serverHost, serverPort.intValue(), aeTitle, calledAeTitle, seriesInstanceUID);
    if (queryResult.containsKey("error")) {
      result.error("QUERY_ERROR", (String) queryResult.get("error"), null);
      return;
    }
    result.success(queryResult.get("instances"));
  }

  private void handleDownloadInstances(MethodCall call, Result result) {
    String serverHost = call.argument("serverHost");
    Number serverPort = call.argument("serverPort");
    String aeTitle = call.argument("aeTitle");
    String calledAeTitle = call.argument("calledAeTitle");
    String seriesInstanceUID = call.argument("seriesInstanceUID");
    String localStoragePath = call.argument("localStoragePath");
    if (serverHost == null || serverPort == null || aeTitle == null || calledAeTitle == null || seriesInstanceUID == null || localStoragePath == null) {
      result.error("INVALID_ARGUMENT", "Server host, port, AE title, called AE title, series instance UID, and local storage path are required", null);
      return;
    }

    new Thread(() -> {
      HashMap<String, Object> instResult = nativeDownloadInstances(serverHost, serverPort.intValue(), aeTitle, calledAeTitle, seriesInstanceUID, localStoragePath);
      runOnMainThread(() -> {
        if (instResult.containsKey("error")) {
          result.error("DOWNLOAD_ERROR", (String) instResult.get("error"), null);
          return;
        }
        result.success(instResult.get("instances"));
      });
    }).start();
  }

  private void handleCreatePatient(MethodCall call, Result result) {
    String serverHost = call.argument("serverHost");
    Number serverPort = call.argument("serverPort");
    String aeTitle = call.argument("aeTitle");
    String calledAeTitle = call.argument("calledAeTitle");
    String patientId = call.argument("patientId");
    String patientName = call.argument("patientName");
    String birthDate = call.argument("birthDate");
    String sex = call.argument("sex");
    String comments = call.argument("comments");
    if (serverHost == null || serverPort == null || aeTitle == null || calledAeTitle == null || patientId == null) {
      result.error("INVALID_ARGUMENT", "Server host, port, AE title, called AE title, and patient ID are required", null);
      return;
    }

    new Thread(() -> {
      HashMap<String, Object> creationResult = nativeCreatePatient(serverHost, serverPort.intValue(), aeTitle, calledAeTitle, patientId, patientName, birthDate, sex, comments);
      runOnMainThread(() -> {
        if (creationResult.containsKey("error")) {
          result.error("CREATION_ERROR", (String) creationResult.get("error"), null);
          return;
        }
        result.success(creationResult);
      });
    }).start();
  }

  private void handleUploadImage(MethodCall call, Result result) {
    String serverHost = call.argument("serverHost");
    Number serverPort = call.argument("serverPort");
    String aeTitle = call.argument("aeTitle");
    String calledAeTitle = call.argument("calledAeTitle");
    String patientId = call.argument("patientId");
    String imagePath = call.argument("imagePath");
    String patientName = call.argument("patientName");
    String patientBirthDate = call.argument("patientBirthDate");
    String studyDescription = call.argument("studyDescription");
    String seriesDescription = call.argument("seriesDescription");
    String imageComments = call.argument("imageComments");
    String modality = call.argument("modality");
    String studyInstanceUID = call.argument("studyInstanceUID");
    String seriesInstanceUID = call.argument("seriesInstanceUID");
    Number instanceNumber = call.argument("instanceNumber");
    if (serverHost == null || serverPort == null || aeTitle == null || calledAeTitle == null || patientId == null || imagePath == null) {
      result.error("INVALID_ARGUMENT", "Server host, port, AE title, called AE title, patient ID, and image path are required", null);
      return;
    }
    int instNum = instanceNumber != null ? instanceNumber.intValue() : 1;

    new Thread(() -> {
      HashMap<String, Object> uploadResult = nativeUploadImage(serverHost, serverPort.intValue(), aeTitle, calledAeTitle, patientId, imagePath, patientName != null ? patientName : "", patientBirthDate != null ? patientBirthDate : "", studyDescription, seriesDescription, imageComments, modality, studyInstanceUID, seriesInstanceUID, instNum);
      runOnMainThread(() -> {
        if (uploadResult.containsKey("error")) {
          result.error("UPLOAD_ERROR", (String) uploadResult.get("error"), null);
          return;
        }
        result.success(uploadResult);
      });
    }).start();
  }

  @SuppressWarnings("unchecked")
  private void handleUploadMultiframe(MethodCall call, Result result) {
    String serverHost = call.argument("serverHost");
    Number serverPort = call.argument("serverPort");
    String aeTitle = call.argument("aeTitle");
    String calledAeTitle = call.argument("calledAeTitle");
    String patientId = call.argument("patientId");
    List<String> imagePaths = call.argument("imagePaths");
    String patientName = call.argument("patientName");
    String patientBirthDate = call.argument("patientBirthDate");
    String studyDescription = call.argument("studyDescription");
    String seriesDescription = call.argument("seriesDescription");
    String imageComments = call.argument("imageComments");
    String modality = call.argument("modality");
    String studyInstanceUID = call.argument("studyInstanceUID");
    String seriesInstanceUID = call.argument("seriesInstanceUID");
    if (serverHost == null || serverPort == null || aeTitle == null || calledAeTitle == null || patientId == null || imagePaths == null || imagePaths.isEmpty()) {
      result.error("INVALID_ARGUMENT", "Required parameters missing for multi-frame upload", null);
      return;
    }
    String[] pathsArray = imagePaths.toArray(new String[0]);

    new Thread(() -> {
      HashMap<String, Object> uploadResult = nativeUploadMultiframe(serverHost, serverPort.intValue(), aeTitle, calledAeTitle, patientId, pathsArray, patientName != null ? patientName : "", patientBirthDate != null ? patientBirthDate : "", studyDescription, seriesDescription, imageComments, modality, studyInstanceUID, seriesInstanceUID);
      runOnMainThread(() -> {
        if (uploadResult.containsKey("error")) {
          result.error("UPLOAD_ERROR", (String) uploadResult.get("error"), null);
          return;
        }
        result.success(uploadResult);
      });
    }).start();
  }

  private void handleUploadVideo(MethodCall call, Result result) {
    String serverHost = call.argument("serverHost");
    Number serverPort = call.argument("serverPort");
    String aeTitle = call.argument("aeTitle");
    String calledAeTitle = call.argument("calledAeTitle");
    String patientId = call.argument("patientId");
    String videoPath = call.argument("videoPath");
    String patientName = call.argument("patientName");
    String patientBirthDate = call.argument("patientBirthDate");
    String studyDescription = call.argument("studyDescription");
    String seriesDescription = call.argument("seriesDescription");
    String imageComments = call.argument("imageComments");
    String modality = call.argument("modality");
    if (serverHost == null || serverPort == null || aeTitle == null || calledAeTitle == null || patientId == null || videoPath == null) {
      result.error("INVALID_ARGUMENT", "Server host, port, AE title, called AE title, patient ID, and video path are required", null);
      return;
    }

    new Thread(() -> {
      HashMap<String, Object> uploadResult = nativeUploadVideo(serverHost, serverPort.intValue(), aeTitle, calledAeTitle, patientId, videoPath, patientName != null ? patientName : "", patientBirthDate != null ? patientBirthDate : "", studyDescription, seriesDescription, imageComments, modality);
      runOnMainThread(() -> {
        if (uploadResult.containsKey("error")) {
          result.error("UPLOAD_ERROR", (String) uploadResult.get("error"), null);
          return;
        }
        result.success(uploadResult);
      });
    }).start();
  }

  private void handleGetDicomTag(MethodCall call, Result result) {
    String filePath = call.argument("filePath");
    String tagName = call.argument("tagName");
    if (filePath == null || tagName == null) {
      result.error("INVALID_ARGUMENT", "File path and tag name are required", null);
      return;
    }
    String tagValue = nativeGetDicomTag(filePath, tagName);
    result.success(tagValue);
  }

  private void handleValidateDicomFile(MethodCall call, Result result) {
    String filePath = call.argument("filePath");
    if (filePath == null) {
      result.error("INVALID_ARGUMENT", "File path is required", null);
      return;
    }
    boolean valid = nativeValidateDicomFile(filePath);
    result.success(valid);
  }

  private void handleExtractVideo(MethodCall call, Result result) {
    String dicomPath = call.argument("dicomPath");
    String outputPath = call.argument("outputPath");
    if (dicomPath == null || outputPath == null) {
      result.error("INVALID_ARGUMENT", "dicomPath and outputPath are required", null);
      return;
    }
    HashMap<String, Object> vidResult = nativeExtractVideo(dicomPath, outputPath);
    if (vidResult.containsKey("error")) {
      result.error("VIDEO_EXTRACTION_ERROR", (String) vidResult.get("error"), null);
      return;
    }
    result.success(vidResult);
  }

  @SuppressWarnings("unchecked")
  private void handleStoreFiles(MethodCall call, Result result) {
    String serverHost = call.argument("serverHost");
    Number serverPort = call.argument("serverPort");
    String aeTitle = call.argument("aeTitle");
    String calledAeTitle = call.argument("calledAeTitle");
    List<String> filePaths = call.argument("filePaths");
    if (serverHost == null || serverPort == null || aeTitle == null || calledAeTitle == null || filePaths == null) {
      result.error("INVALID_ARGUMENT", "Required parameters missing for store files", null);
      return;
    }
    String[] pathsArray = filePaths.toArray(new String[0]);

    new Thread(() -> {
      HashMap<String, Object> storeResult = nativeStoreFiles(serverHost, serverPort.intValue(), aeTitle, calledAeTitle, pathsArray);
      runOnMainThread(() -> result.success(storeResult));
    }).start();
  }

  private void handleStartStoreSCP(MethodCall call, Result result) {
    Number port = call.argument("port");
    String aeTitle = call.argument("aeTitle");
    String storageDir = call.argument("storageDir");
    if (port == null || aeTitle == null || storageDir == null) {
      result.error("INVALID_ARGUMENT", "Port, AE title, and storage directory are required", null);
      return;
    }

    new Thread(() -> {
      nativeStartStoreSCP(port.intValue(), aeTitle, storageDir);
    }).start();

    HashMap<String, Object> response = new HashMap<>();
    response.put("started", true);
    response.put("port", port.intValue());
    result.success(response);
  }

  private void handleStopStoreSCP(MethodCall call, Result result) {
    nativeStopStoreSCP();
    HashMap<String, Object> response = new HashMap<>();
    response.put("stopped", true);
    result.success(response);
  }

  private void handleGetStoreSCPStatus(MethodCall call, Result result) {
    HashMap<String, Object> status = nativeGetStoreSCPStatus();
    result.success(status);
  }

  private void handleMoveInstances(MethodCall call, Result result) {
    String serverHost = call.argument("serverHost");
    Number serverPort = call.argument("serverPort");
    String aeTitle = call.argument("aeTitle");
    String calledAeTitle = call.argument("calledAeTitle");
    String seriesInstanceUID = call.argument("seriesInstanceUID");
    String localStoragePath = call.argument("localStoragePath");
    Number moveSCPPort = call.argument("moveSCPPort");
    if (serverHost == null || serverPort == null || aeTitle == null || calledAeTitle == null || seriesInstanceUID == null || localStoragePath == null || moveSCPPort == null) {
      result.error("INVALID_ARGUMENT", "Required parameters missing for C-MOVE", null);
      return;
    }

    new Thread(() -> {
      HashMap<String, Object> moveResult = nativeMoveInstances(serverHost, serverPort.intValue(), aeTitle, calledAeTitle, seriesInstanceUID, localStoragePath, moveSCPPort.intValue());
      runOnMainThread(() -> {
        if (moveResult.containsKey("error")) {
          result.error("MOVE_ERROR", (String) moveResult.get("error"), null);
          return;
        }
        result.success(moveResult);
      });
    }).start();
  }

  private void handleSetTlsConfig(MethodCall call, Result result) {
    String certFile = call.argument("certFile");
    String keyFile = call.argument("keyFile");
    String caFile = call.argument("caFile");
    nativeSetTlsConfig(certFile, keyFile, caFile);
    result.success(true);
  }

  private void handleClearTlsConfig(MethodCall call, Result result) {
    nativeClearTlsConfig();
    result.success(true);
  }

  // ==================== MPR / MIP ====================

  @SuppressWarnings("unchecked")
  private void handleBuildMprVolume(MethodCall call, Result result) {
    List<String> filePaths = call.argument("filePaths");
    if (filePaths == null || filePaths.size() < 1) {
      result.error("INVALID_ARGUMENT", "Need at least 1 file path for MPR", null);
      return;
    }
    String[] pathsArray = filePaths.toArray(new String[0]);

    new Thread(() -> {
      HashMap<String, Object> volResult = nativeBuildMprVolume(pathsArray);
      runOnMainThread(() -> {
        if (volResult.containsKey("error")) {
          result.error("MPR_ERROR", (String) volResult.get("error"), null);
          return;
        }
        result.success(volResult);
      });
    }).start();
  }

  private void handleGetMprSlice(MethodCall call, Result result) {
    Number volumeId = call.argument("volumeId");
    Number plane = call.argument("plane");
    Number sliceIndex = call.argument("sliceIndex");
    Number windowCenter = call.argument("windowCenter");
    Number windowWidth = call.argument("windowWidth");
    if (volumeId == null || plane == null || sliceIndex == null) {
      result.error("INVALID_ARGUMENT", "volumeId, plane, and sliceIndex are required", null);
      return;
    }
    int vid = volumeId.intValue();
    int pl = plane.intValue();
    int si = sliceIndex.intValue();
    double wc = windowCenter != null ? windowCenter.doubleValue() : 0;
    double ww = windowWidth != null ? windowWidth.doubleValue() : 0;

    new Thread(() -> {
      HashMap<String, Object> sliceResult = nativeGetMprSlice(vid, pl, si, wc, ww);
      runOnMainThread(() -> {
        if (sliceResult.containsKey("error")) {
          result.error("MPR_ERROR", (String) sliceResult.get("error"), null);
          return;
        }
        result.success(sliceResult);
      });
    }).start();
  }

  private void handleFreeMprVolume(MethodCall call, Result result) {
    Number volumeId = call.argument("volumeId");
    if (volumeId != null) {
      nativeFreeMprVolume(volumeId.intValue());
    }
    result.success(null);
  }

  private void handleRenderMip(MethodCall call, Result result) {
    Number volumeId = call.argument("volumeId");
    Number rotationX = call.argument("rotationX");
    Number rotationY = call.argument("rotationY");
    Number windowCenter = call.argument("windowCenter");
    Number windowWidth = call.argument("windowWidth");
    if (volumeId == null) {
      result.error("INVALID_ARGUMENT", "volumeId is required", null);
      return;
    }
    int vid = volumeId.intValue();
    double rx = rotationX != null ? rotationX.doubleValue() : 0;
    double ry = rotationY != null ? rotationY.doubleValue() : 0;
    double wc = windowCenter != null ? windowCenter.doubleValue() : 0;
    double ww = windowWidth != null ? windowWidth.doubleValue() : 0;

    new Thread(() -> {
      HashMap<String, Object> mipResult = nativeRenderMip(vid, rx, ry, wc, ww);
      runOnMainThread(() -> {
        if (mipResult.containsKey("error")) {
          result.error("MIP_ERROR", (String) mipResult.get("error"), null);
          return;
        }
        result.success(mipResult);
      });
    }).start();
  }

  private void handleRenderVolume(MethodCall call, Result result) {
    Number volumeId = call.argument("volumeId");
    Number rotationX = call.argument("rotationX");
    Number rotationY = call.argument("rotationY");
    Number windowCenter = call.argument("windowCenter");
    Number windowWidth = call.argument("windowWidth");
    String preset = call.argument("preset");
    Boolean preview = call.argument("preview");
    if (volumeId == null) {
      result.error("INVALID_ARGUMENT", "volumeId is required", null);
      return;
    }
    int vid = volumeId.intValue();
    double rx = rotationX != null ? rotationX.doubleValue() : 0;
    double ry = rotationY != null ? rotationY.doubleValue() : 0;
    double wc = windowCenter != null ? windowCenter.doubleValue() : 0;
    double ww = windowWidth != null ? windowWidth.doubleValue() : 0;
    String presetName = preset != null ? preset : "Muscle";
    boolean previewMode = preview != null && preview;

    new Thread(() -> {
      HashMap<String, Object> volumeResult = nativeRenderVolume(vid, rx, ry, wc, ww, presetName, previewMode);
      runOnMainThread(() -> {
        if (volumeResult.containsKey("error")) {
          result.error("VOLUME_RENDER_ERROR", (String) volumeResult.get("error"), null);
          return;
        }
        result.success(volumeResult);
      });
    }).start();
  }

  // ==================== GSPS ====================

  private void handleCreateGsps(MethodCall call, Result result) {
    String sourceDicomPath = call.argument("sourceDicomPath");
    String annotationsJson = call.argument("annotationsJson");
    String outputPath = call.argument("outputPath");
    if (sourceDicomPath == null || annotationsJson == null || outputPath == null) {
      result.error("INVALID_ARGUMENT", "sourceDicomPath, annotationsJson, and outputPath are required", null);
      return;
    }

    new Thread(() -> {
      HashMap<String, Object> gspsResult = nativeCreateGsps(sourceDicomPath, annotationsJson, outputPath);
      runOnMainThread(() -> {
        if (gspsResult.containsKey("error")) {
          result.error("GSPS_ERROR", (String) gspsResult.get("error"), null);
          return;
        }
        result.success(gspsResult);
      });
    }).start();
  }

  private void handleParseGsps(MethodCall call, Result result) {
    String filePath = call.argument("filePath");
    if (filePath == null) {
      result.error("INVALID_ARGUMENT", "filePath is required", null);
      return;
    }

    new Thread(() -> {
      String jsonStr = nativeParseGsps(filePath);
      runOnMainThread(() -> result.success(jsonStr));
    }).start();
  }

  // ==================== Convert Image to DICOM ====================

  private void handleConvertImageToDicom(MethodCall call, Result result) {
    String imagePath = call.argument("imagePath");
    String outputPath = call.argument("outputPath");
    String patientId = call.argument("patientId");
    String patientName = call.argument("patientName");
    String patientBirthDate = call.argument("patientBirthDate");
    String studyDescription = call.argument("studyDescription");
    String seriesDescription = call.argument("seriesDescription");
    String imageComments = call.argument("imageComments");
    String modality = call.argument("modality");
    String studyInstanceUID = call.argument("studyInstanceUID");
    String seriesInstanceUID = call.argument("seriesInstanceUID");
    Number instanceNumber = call.argument("instanceNumber");
    if (imagePath == null || outputPath == null) {
      result.error("INVALID_ARGUMENT", "imagePath and outputPath are required", null);
      return;
    }
    int instNum = instanceNumber != null ? instanceNumber.intValue() : 1;

    new Thread(() -> {
      HashMap<String, Object> convResult = nativeConvertImageToDicom(imagePath, outputPath,
              patientId != null ? patientId : "",
              patientName != null ? patientName : "",
              patientBirthDate != null ? patientBirthDate : "",
              studyDescription != null ? studyDescription : "Exported Image",
              seriesDescription != null ? seriesDescription : "Exported Series",
              imageComments != null ? imageComments : "",
              modality != null ? modality : "SC",
              studyInstanceUID != null ? studyInstanceUID : "",
              seriesInstanceUID != null ? seriesInstanceUID : "",
              instNum);
      runOnMainThread(() -> {
        if (convResult.containsKey("error")) {
          result.error("CONVERSION_ERROR", (String) convResult.get("error"), null);
          return;
        }
        result.success(convResult);
      });
    }).start();
  }

  private void runOnMainThread(Runnable runnable) {
    new android.os.Handler(android.os.Looper.getMainLooper()).post(runnable);
  }

  @Override
  public void onDetachedFromEngine(@NonNull FlutterPluginBinding binding) {
    channel.setMethodCallHandler(null);
  }
}