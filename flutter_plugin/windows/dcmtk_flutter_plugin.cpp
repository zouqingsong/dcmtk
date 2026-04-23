#include "dcmtk_flutter_plugin.h"

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <thread>

#include "dcmtk_flutter_wrapper.h"

namespace dcmtk_flutter {

using flutter::EncodableList;
using flutter::EncodableMap;
using flutter::EncodableValue;

// Helper: get a string from EncodableMap, returns empty string if missing or null
static std::string GetString(const EncodableMap& map, const std::string& key) {
  auto it = map.find(EncodableValue(key));
  if (it != map.end() && std::holds_alternative<std::string>(it->second)) {
    return std::get<std::string>(it->second);
  }
  return "";
}

// Helper: get an int from EncodableMap, returns default_val if missing
static int GetInt(const EncodableMap& map, const std::string& key, int default_val = 0) {
  auto it = map.find(EncodableValue(key));
  if (it != map.end()) {
    if (std::holds_alternative<int32_t>(it->second)) {
      return std::get<int32_t>(it->second);
    }
    if (std::holds_alternative<int64_t>(it->second)) {
      return static_cast<int>(std::get<int64_t>(it->second));
    }
  }
  return default_val;
}

// Helper: get a double from EncodableMap, returns default_val if missing
static double GetDouble(const EncodableMap& map, const std::string& key, double default_val = 0.0) {
  auto it = map.find(EncodableValue(key));
  if (it != map.end()) {
    if (std::holds_alternative<double>(it->second)) {
      return std::get<double>(it->second);
    }
    if (std::holds_alternative<int32_t>(it->second)) {
      return static_cast<double>(std::get<int32_t>(it->second));
    }
    if (std::holds_alternative<int64_t>(it->second)) {
      return static_cast<double>(std::get<int64_t>(it->second));
    }
  }
  return default_val;
}

// Helper: get a string list from EncodableMap
static std::vector<std::string> GetStringList(const EncodableMap& map, const std::string& key) {
  std::vector<std::string> result;
  auto it = map.find(EncodableValue(key));
  if (it != map.end() && std::holds_alternative<EncodableList>(it->second)) {
    const auto& list = std::get<EncodableList>(it->second);
    for (const auto& item : list) {
      if (std::holds_alternative<std::string>(item)) {
        result.push_back(std::get<std::string>(item));
      }
    }
  }
  return result;
}

// Helper: safe string from char* (handles nullptr)
static std::string SafeStr(const char* s) {
  return s ? std::string(s) : std::string();
}

void DcmtkFlutterPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows *registrar) {
  auto channel =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "dcmtk_flutter",
          &flutter::StandardMethodCodec::GetInstance());

  auto plugin = std::make_unique<DcmtkFlutterPlugin>();

  channel->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto &call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });

  registrar->AddPlugin(std::move(plugin));
}

DcmtkFlutterPlugin::DcmtkFlutterPlugin() {}

DcmtkFlutterPlugin::~DcmtkFlutterPlugin() {}

void DcmtkFlutterPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue> &method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {

  const auto& method = method_call.method_name();
  const auto* args = std::get_if<EncodableMap>(method_call.arguments());

  if (!args && method != "getPlatformVersion") {
    result->Error("INVALID_ARGUMENT", "Arguments must be a map");
    return;
  }

  // ===== loadDicomFile =====
  if (method == "loadDicomFile") {
    std::string filePath = GetString(*args, "filePath");
    if (filePath.empty()) {
      result->Error("INVALID_ARGUMENT", "File path is required");
      return;
    }
    char* resultStr = dcmtk_load_dicom_file(filePath.c_str());
    std::string resultString(resultStr);
    dcmtk_free_string(resultStr);
    result->Success(EncodableValue(resultString));

  // ===== extractImage =====
  } else if (method == "extractImage") {
    std::string filePath = GetString(*args, "filePath");
    if (filePath.empty()) {
      result->Error("INVALID_ARGUMENT", "File path is required");
      return;
    }
    int frameIndex = GetInt(*args, "frameIndex", 0);
    double wc = GetDouble(*args, "windowCenter", 0.0);
    double ww = GetDouble(*args, "windowWidth", 0.0);

    DicomImageData* imgData = dcmtk_extract_image(filePath.c_str(), frameIndex, wc, ww);
    if (imgData->error) {
      std::string errorMsg = imgData->error_message ? imgData->error_message : "Unknown error";
      dcmtk_free_image_data(imgData);
      result->Error("EXTRACTION_ERROR", errorMsg);
      return;
    }

    int dataLen = imgData->width * imgData->height * 4;
    std::vector<uint8_t> pixelData(imgData->data, imgData->data + dataLen);

    EncodableMap imageResult;
    imageResult[EncodableValue("width")] = EncodableValue(imgData->width);
    imageResult[EncodableValue("height")] = EncodableValue(imgData->height);
    imageResult[EncodableValue("data")] = EncodableValue(pixelData);
    imageResult[EncodableValue("samplesPerPixel")] = EncodableValue(imgData->samples_per_pixel);
    imageResult[EncodableValue("bitsStored")] = EncodableValue(imgData->bits_stored);
    imageResult[EncodableValue("totalFrames")] = EncodableValue(imgData->total_frames);

    dcmtk_free_image_data(imgData);
    result->Success(EncodableValue(imageResult));

  // ===== testServerConnection =====
  } else if (method == "testServerConnection") {
    std::string host = GetString(*args, "serverHost");
    int port = GetInt(*args, "serverPort");
    std::string aeTitle = GetString(*args, "aeTitle");
    std::string calledAeTitle = GetString(*args, "calledAeTitle");

    if (host.empty() || aeTitle.empty() || calledAeTitle.empty()) {
      result->Error("INVALID_ARGUMENT", "Server host, port, AE title, and called AE title are required");
      return;
    }

    int connResult = dcmtk_test_server_connection(host.c_str(), port, aeTitle.c_str(), calledAeTitle.c_str());
    result->Success(EncodableValue(connResult == 1));

  // ===== testServerConnectionTls =====
  } else if (method == "testServerConnectionTls") {
    std::string host = GetString(*args, "serverHost");
    int port = GetInt(*args, "serverPort");
    std::string aeTitle = GetString(*args, "aeTitle");
    std::string calledAeTitle = GetString(*args, "calledAeTitle");
    std::string certFile = GetString(*args, "certFile");
    std::string keyFile = GetString(*args, "keyFile");
    std::string caFile = GetString(*args, "caFile");

    int tlsResult = dcmtk_test_server_connection_tls(
        host.c_str(), port, aeTitle.c_str(), calledAeTitle.c_str(),
        certFile.c_str(), keyFile.c_str(), caFile.c_str());

    if (tlsResult == -1) {
      result->Error("TLS_NOT_AVAILABLE", "TLS support not compiled (OpenSSL not available)");
    } else {
      result->Success(EncodableValue(tlsResult == 1));
    }

  // ===== queryPatients =====
  } else if (method == "queryPatients") {
    std::string host = GetString(*args, "serverHost");
    int port = GetInt(*args, "serverPort");
    std::string aeTitle = GetString(*args, "aeTitle");
    std::string calledAeTitle = GetString(*args, "calledAeTitle");
    std::string filter = GetString(*args, "patientNameFilter");

    DicomQueryResult* queryResult = dcmtk_query_patients(
        host.c_str(), port, aeTitle.c_str(), calledAeTitle.c_str(),
        filter.empty() ? nullptr : filter.c_str());

    if (queryResult->error) {
      std::string errorMsg = SafeStr(queryResult->error_message);
      dcmtk_free_query_result(queryResult);
      result->Error("QUERY_ERROR", errorMsg.empty() ? "Unknown query error" : errorMsg);
      return;
    }

    EncodableList patients;
    for (int i = 0; i < queryResult->patient_count; i++) {
      DicomPatient& p = queryResult->patients[i];
      EncodableMap patient;
      patient[EncodableValue("patientId")] = EncodableValue(SafeStr(p.patient_id));
      patient[EncodableValue("patientName")] = EncodableValue(SafeStr(p.patient_name));
      patient[EncodableValue("patientBirthDate")] = EncodableValue(SafeStr(p.patient_birth_date));
      patient[EncodableValue("patientSex")] = EncodableValue(SafeStr(p.patient_sex));
      patient[EncodableValue("studyCount")] = EncodableValue(p.study_count);
      patient[EncodableValue("numberOfPatientRelatedStudies")] = EncodableValue(SafeStr(p.number_of_patient_related_studies));
      patients.push_back(EncodableValue(patient));
    }
    dcmtk_free_query_result(queryResult);
    result->Success(EncodableValue(patients));

  // ===== queryStudiesForPatient =====
  } else if (method == "queryStudiesForPatient") {
    std::string host = GetString(*args, "serverHost");
    int port = GetInt(*args, "serverPort");
    std::string aeTitle = GetString(*args, "aeTitle");
    std::string calledAeTitle = GetString(*args, "calledAeTitle");
    std::string patientId = GetString(*args, "patientId");

    DicomStudyQueryResult* queryResult = dcmtk_query_studies_for_patient(
        host.c_str(), port, aeTitle.c_str(), calledAeTitle.c_str(), patientId.c_str());

    if (queryResult->error) {
      std::string errorMsg = SafeStr(queryResult->error_message);
      dcmtk_free_study_query_result(queryResult);
      result->Error("QUERY_ERROR", errorMsg.empty() ? "Unknown query error" : errorMsg);
      return;
    }

    EncodableList studies;
    for (int i = 0; i < queryResult->study_count; i++) {
      DicomStudy& s = queryResult->studies[i];
      EncodableMap study;
      study[EncodableValue("studyInstanceUID")] = EncodableValue(SafeStr(s.study_instance_uid));
      study[EncodableValue("studyDate")] = EncodableValue(SafeStr(s.study_date));
      study[EncodableValue("studyTime")] = EncodableValue(SafeStr(s.study_time));
      study[EncodableValue("studyDescription")] = EncodableValue(SafeStr(s.study_description));
      study[EncodableValue("accessionNumber")] = EncodableValue(SafeStr(s.accession_number));
      study[EncodableValue("seriesCount")] = EncodableValue(s.series_count);
      study[EncodableValue("modalitiesInStudy")] = EncodableValue(SafeStr(s.modalities_in_study));
      study[EncodableValue("numberOfStudyRelatedSeries")] = EncodableValue(SafeStr(s.number_of_study_related_series));
      study[EncodableValue("numberOfStudyRelatedInstances")] = EncodableValue(SafeStr(s.number_of_study_related_instances));
      study[EncodableValue("referringPhysicianName")] = EncodableValue(SafeStr(s.referring_physician_name));
      studies.push_back(EncodableValue(study));
    }
    dcmtk_free_study_query_result(queryResult);
    result->Success(EncodableValue(studies));

  // ===== querySeriesForStudy =====
  } else if (method == "querySeriesForStudy") {
    std::string host = GetString(*args, "serverHost");
    int port = GetInt(*args, "serverPort");
    std::string aeTitle = GetString(*args, "aeTitle");
    std::string calledAeTitle = GetString(*args, "calledAeTitle");
    std::string studyInstanceUID = GetString(*args, "studyInstanceUID");

    DicomSeriesQueryResult* seriesResult = dcmtk_query_series_for_study(
        host.c_str(), port, aeTitle.c_str(), calledAeTitle.c_str(), studyInstanceUID.c_str());

    if (seriesResult->error) {
      std::string errorMsg = SafeStr(seriesResult->error_message);
      dcmtk_free_series_query_result(seriesResult);
      result->Error("QUERY_ERROR", errorMsg.empty() ? "Unknown series query error" : errorMsg);
      return;
    }

    EncodableList seriesList;
    for (int i = 0; i < seriesResult->series_count; i++) {
      DicomSeries& s = seriesResult->series[i];
      EncodableMap series;
      series[EncodableValue("seriesInstanceUID")] = EncodableValue(SafeStr(s.series_instance_uid));
      series[EncodableValue("seriesNumber")] = EncodableValue(SafeStr(s.series_number));
      series[EncodableValue("seriesDescription")] = EncodableValue(SafeStr(s.series_description));
      series[EncodableValue("modality")] = EncodableValue(SafeStr(s.modality));
      series[EncodableValue("seriesDate")] = EncodableValue(SafeStr(s.series_date));
      series[EncodableValue("seriesTime")] = EncodableValue(SafeStr(s.series_time));
      series[EncodableValue("instanceCount")] = EncodableValue(s.instance_count);
      series[EncodableValue("numberOfSeriesRelatedInstances")] = EncodableValue(SafeStr(s.number_of_series_related_instances));
      series[EncodableValue("bodyPartExamined")] = EncodableValue(SafeStr(s.body_part_examined));
      seriesList.push_back(EncodableValue(series));
    }
    dcmtk_free_series_query_result(seriesResult);
    result->Success(EncodableValue(seriesList));

  // ===== queryInstancesForSeries =====
  } else if (method == "queryInstancesForSeries") {
    std::string host = GetString(*args, "serverHost");
    int port = GetInt(*args, "serverPort");
    std::string aeTitle = GetString(*args, "aeTitle");
    std::string calledAeTitle = GetString(*args, "calledAeTitle");
    std::string seriesInstanceUID = GetString(*args, "seriesInstanceUID");

    DicomInstanceQueryResult* instResult = dcmtk_query_instances_for_series(
        host.c_str(), port, aeTitle.c_str(), calledAeTitle.c_str(), seriesInstanceUID.c_str());

    if (instResult->error) {
      std::string errorMsg = SafeStr(instResult->error_message);
      dcmtk_free_instance_query_result(instResult);
      result->Error("QUERY_ERROR", errorMsg.empty() ? "Unknown query error" : errorMsg);
      return;
    }

    EncodableList instances;
    for (int i = 0; i < instResult->instance_count; i++) {
      DicomInstance& inst = instResult->instances[i];
      EncodableMap instanceMap;
      instanceMap[EncodableValue("sopInstanceUID")] = EncodableValue(SafeStr(inst.sop_instance_uid));
      instanceMap[EncodableValue("instanceNumber")] = EncodableValue(SafeStr(inst.instance_number));
      instanceMap[EncodableValue("filePath")] = EncodableValue(SafeStr(inst.file_path));
      instanceMap[EncodableValue("contentType")] = EncodableValue(SafeStr(inst.content_type));
      instanceMap[EncodableValue("fileSize")] = EncodableValue(inst.file_size);
      instances.push_back(EncodableValue(instanceMap));
    }
    dcmtk_free_instance_query_result(instResult);
    result->Success(EncodableValue(instances));

  // ===== downloadInstancesViaCMove =====
  } else if (method == "downloadInstancesViaCMove") {
    std::string host = GetString(*args, "serverHost");
    int port = GetInt(*args, "serverPort");
    std::string aeTitle = GetString(*args, "aeTitle");
    std::string calledAeTitle = GetString(*args, "calledAeTitle");
    std::string seriesInstanceUID = GetString(*args, "seriesInstanceUID");
    std::string localStoragePath = GetString(*args, "localStoragePath");

    DicomInstanceQueryResult* instanceResult = dcmtk_download_instances(
        host.c_str(), port, aeTitle.c_str(), calledAeTitle.c_str(),
        seriesInstanceUID.c_str(), localStoragePath.c_str());

    if (instanceResult->error) {
      std::string errorMsg = SafeStr(instanceResult->error_message);
      dcmtk_free_instance_query_result(instanceResult);
      result->Error("DOWNLOAD_ERROR", errorMsg.empty() ? "Unknown C-MOVE download error" : errorMsg);
      return;
    }

    EncodableList instances;
    for (int i = 0; i < instanceResult->instance_count; i++) {
      DicomInstance& inst = instanceResult->instances[i];
      EncodableMap instanceMap;
      instanceMap[EncodableValue("sopInstanceUID")] = EncodableValue(SafeStr(inst.sop_instance_uid));
      instanceMap[EncodableValue("instanceNumber")] = EncodableValue(SafeStr(inst.instance_number));
      instanceMap[EncodableValue("filePath")] = EncodableValue(SafeStr(inst.file_path));
      instanceMap[EncodableValue("contentType")] = EncodableValue(inst.content_type ? std::string(inst.content_type) : std::string("IMAGE"));
      instanceMap[EncodableValue("fileSize")] = EncodableValue(inst.file_size);
      instances.push_back(EncodableValue(instanceMap));
    }
    dcmtk_free_instance_query_result(instanceResult);
    result->Success(EncodableValue(instances));

  // ===== createPatient =====
  } else if (method == "createPatient") {
    std::string host = GetString(*args, "serverHost");
    int port = GetInt(*args, "serverPort");
    std::string aeTitle = GetString(*args, "aeTitle");
    std::string calledAeTitle = GetString(*args, "calledAeTitle");
    std::string patientId = GetString(*args, "patientId");
    std::string patientName = GetString(*args, "patientName");
    std::string birthDate = GetString(*args, "birthDate");
    std::string sex = GetString(*args, "sex");
    std::string comments = GetString(*args, "comments");

    PatientInfo patientInfo;
    patientInfo.patient_id = const_cast<char*>(patientId.c_str());
    patientInfo.patient_name = const_cast<char*>(patientName.c_str());
    patientInfo.patient_birth_date = const_cast<char*>(birthDate.c_str());
    patientInfo.patient_sex = const_cast<char*>(sex.c_str());
    patientInfo.patient_comments = const_cast<char*>(comments.c_str());

    PatientCreationResult* creationResult = dcmtk_create_patient(
        host.c_str(), port, aeTitle.c_str(), calledAeTitle.c_str(), &patientInfo);

    if (!creationResult->success) {
      std::string errorMsg = SafeStr(creationResult->error_message);
      dcmtk_free_patient_creation_result(creationResult);
      result->Error("CREATION_ERROR", errorMsg.empty() ? "Unknown creation error" : errorMsg);
      return;
    }

    EncodableMap resultDict;
    resultDict[EncodableValue("success")] = EncodableValue(creationResult->success != 0);
    resultDict[EncodableValue("patientId")] = EncodableValue(SafeStr(creationResult->generated_patient_id));
    resultDict[EncodableValue("rspStatusCode")] = EncodableValue(creationResult->rsp_status_code);
    resultDict[EncodableValue("warning")] = EncodableValue(SafeStr(creationResult->warning_message));
    dcmtk_free_patient_creation_result(creationResult);
    result->Success(EncodableValue(resultDict));

  // ===== uploadImage =====
  } else if (method == "uploadImage") {
    std::string host = GetString(*args, "serverHost");
    int port = GetInt(*args, "serverPort");
    std::string aeTitle = GetString(*args, "aeTitle");
    std::string calledAeTitle = GetString(*args, "calledAeTitle");
    std::string patientId = GetString(*args, "patientId");
    std::string imagePath = GetString(*args, "imagePath");
    std::string patientName = GetString(*args, "patientName");
    std::string patientBirthDate = GetString(*args, "patientBirthDate");
    std::string studyDesc = GetString(*args, "studyDescription");
    std::string seriesDesc = GetString(*args, "seriesDescription");
    std::string imageComments = GetString(*args, "imageComments");
    std::string modality = GetString(*args, "modality");
    std::string studyUID = GetString(*args, "studyInstanceUID");
    std::string seriesUID = GetString(*args, "seriesInstanceUID");
    int instanceNumber = GetInt(*args, "instanceNumber", 1);

    MediaUploadResult* uploadResult = dcmtk_upload_image(
        host.c_str(), port, aeTitle.c_str(), calledAeTitle.c_str(),
        patientId.c_str(), imagePath.c_str(),
        patientName.c_str(), patientBirthDate.c_str(),
        studyDesc.empty() ? "Uploaded Image" : studyDesc.c_str(),
        seriesDesc.empty() ? "Uploaded Series" : seriesDesc.c_str(),
        imageComments.c_str(),
        modality.empty() ? nullptr : modality.c_str(),
        studyUID.c_str(), seriesUID.c_str(), instanceNumber);

    if (!uploadResult->success) {
      std::string errorMsg = SafeStr(uploadResult->error_message);
      dcmtk_free_media_upload_result(uploadResult);
      result->Error("UPLOAD_ERROR", errorMsg.empty() ? "Unknown upload error" : errorMsg);
      return;
    }

    EncodableMap resultDict;
    resultDict[EncodableValue("success")] = EncodableValue(true);
    resultDict[EncodableValue("studyInstanceUID")] = EncodableValue(SafeStr(uploadResult->study_instance_uid));
    resultDict[EncodableValue("seriesInstanceUID")] = EncodableValue(SafeStr(uploadResult->series_instance_uid));
    resultDict[EncodableValue("sopInstanceUID")] = EncodableValue(SafeStr(uploadResult->sop_instance_uid));
    resultDict[EncodableValue("rspStatusCode")] = EncodableValue(uploadResult->rsp_status_code);
    dcmtk_free_media_upload_result(uploadResult);
    result->Success(EncodableValue(resultDict));

  // ===== uploadMultiframe =====
  } else if (method == "uploadMultiframe") {
    std::string host = GetString(*args, "serverHost");
    int port = GetInt(*args, "serverPort");
    std::string aeTitle = GetString(*args, "aeTitle");
    std::string calledAeTitle = GetString(*args, "calledAeTitle");
    std::string patientId = GetString(*args, "patientId");
    auto imagePaths = GetStringList(*args, "imagePaths");
    std::string patientName = GetString(*args, "patientName");
    std::string patientBirthDate = GetString(*args, "patientBirthDate");
    std::string studyDesc = GetString(*args, "studyDescription");
    std::string seriesDesc = GetString(*args, "seriesDescription");
    std::string imageComments = GetString(*args, "imageComments");
    std::string modality = GetString(*args, "modality");
    std::string studyUID = GetString(*args, "studyInstanceUID");
    std::string seriesUID = GetString(*args, "seriesInstanceUID");

    std::vector<const char*> cPaths;
    for (const auto& p : imagePaths) cPaths.push_back(p.c_str());

    MediaUploadResult* uploadResult = dcmtk_upload_multiframe(
        host.c_str(), port, aeTitle.c_str(), calledAeTitle.c_str(),
        patientId.c_str(), cPaths.data(), static_cast<int>(cPaths.size()),
        patientName.c_str(), patientBirthDate.c_str(),
        studyDesc.empty() ? "Uploaded Study" : studyDesc.c_str(),
        seriesDesc.empty() ? "Multi-frame Series" : seriesDesc.c_str(),
        imageComments.c_str(),
        modality.empty() ? "SC" : modality.c_str(),
        studyUID.c_str(), seriesUID.c_str());

    if (!uploadResult->success) {
      std::string errorMsg = SafeStr(uploadResult->error_message);
      dcmtk_free_media_upload_result(uploadResult);
      result->Error("UPLOAD_ERROR", errorMsg.empty() ? "Unknown error" : errorMsg);
      return;
    }

    EncodableMap resultDict;
    resultDict[EncodableValue("success")] = EncodableValue(true);
    resultDict[EncodableValue("studyInstanceUID")] = EncodableValue(SafeStr(uploadResult->study_instance_uid));
    resultDict[EncodableValue("seriesInstanceUID")] = EncodableValue(SafeStr(uploadResult->series_instance_uid));
    resultDict[EncodableValue("sopInstanceUID")] = EncodableValue(SafeStr(uploadResult->sop_instance_uid));
    dcmtk_free_media_upload_result(uploadResult);
    result->Success(EncodableValue(resultDict));

  // ===== uploadVideo =====
  } else if (method == "uploadVideo") {
    std::string host = GetString(*args, "serverHost");
    int port = GetInt(*args, "serverPort");
    std::string aeTitle = GetString(*args, "aeTitle");
    std::string calledAeTitle = GetString(*args, "calledAeTitle");
    std::string patientId = GetString(*args, "patientId");
    std::string videoPath = GetString(*args, "videoPath");
    std::string patientName = GetString(*args, "patientName");
    std::string patientBirthDate = GetString(*args, "patientBirthDate");
    std::string studyDesc = GetString(*args, "studyDescription");
    std::string seriesDesc = GetString(*args, "seriesDescription");
    std::string imageComments = GetString(*args, "imageComments");
    std::string modality = GetString(*args, "modality");

    MediaUploadResult* uploadResult = dcmtk_upload_video(
        host.c_str(), port, aeTitle.c_str(), calledAeTitle.c_str(),
        patientId.c_str(), videoPath.c_str(),
        patientName.c_str(), patientBirthDate.c_str(),
        studyDesc.empty() ? "Uploaded Video" : studyDesc.c_str(),
        seriesDesc.empty() ? "Uploaded Video Series" : seriesDesc.c_str(),
        imageComments.c_str(),
        modality.empty() ? "SC" : modality.c_str());

    if (!uploadResult->success) {
      std::string errorMsg = SafeStr(uploadResult->error_message);
      dcmtk_free_media_upload_result(uploadResult);
      result->Error("UPLOAD_ERROR", errorMsg.empty() ? "Unknown upload error" : errorMsg);
      return;
    }

    EncodableMap resultDict;
    resultDict[EncodableValue("success")] = EncodableValue(true);
    resultDict[EncodableValue("studyInstanceUID")] = EncodableValue(SafeStr(uploadResult->study_instance_uid));
    resultDict[EncodableValue("seriesInstanceUID")] = EncodableValue(SafeStr(uploadResult->series_instance_uid));
    resultDict[EncodableValue("sopInstanceUID")] = EncodableValue(SafeStr(uploadResult->sop_instance_uid));
    dcmtk_free_media_upload_result(uploadResult);
    result->Success(EncodableValue(resultDict));

  // ===== convertImageToDicom =====
  } else if (method == "convertImageToDicom") {
    std::string imagePath = GetString(*args, "imagePath");
    std::string outputPath = GetString(*args, "outputPath");
    std::string patientId = GetString(*args, "patientId");
    std::string patientName = GetString(*args, "patientName");
    std::string patientBirthDate = GetString(*args, "patientBirthDate");
    std::string studyDesc = GetString(*args, "studyDescription");
    std::string seriesDesc = GetString(*args, "seriesDescription");
    std::string imageComments = GetString(*args, "imageComments");
    std::string modality = GetString(*args, "modality");
    std::string studyUID = GetString(*args, "studyInstanceUID");
    std::string seriesUID = GetString(*args, "seriesInstanceUID");
    int instanceNumber = GetInt(*args, "instanceNumber", 1);

    MediaUploadResult* convResult = dcmtk_convert_image_to_dicom(
        imagePath.c_str(), outputPath.c_str(),
        patientId.c_str(), patientName.c_str(), patientBirthDate.c_str(),
        studyDesc.empty() ? "Exported Image" : studyDesc.c_str(),
        seriesDesc.empty() ? "Exported Series" : seriesDesc.c_str(),
        imageComments.c_str(),
        modality.empty() ? "SC" : modality.c_str(),
        studyUID.c_str(), seriesUID.c_str(), instanceNumber);

    EncodableMap resultDict;
    if (!convResult->success) {
      resultDict[EncodableValue("success")] = EncodableValue(false);
      resultDict[EncodableValue("error")] = EncodableValue(SafeStr(convResult->error_message));
    } else {
      resultDict[EncodableValue("success")] = EncodableValue(true);
      resultDict[EncodableValue("studyInstanceUID")] = EncodableValue(SafeStr(convResult->study_instance_uid));
      resultDict[EncodableValue("seriesInstanceUID")] = EncodableValue(SafeStr(convResult->series_instance_uid));
      resultDict[EncodableValue("sopInstanceUID")] = EncodableValue(SafeStr(convResult->sop_instance_uid));
    }
    dcmtk_free_media_upload_result(convResult);
    result->Success(EncodableValue(resultDict));

  // ===== createGsps =====
  } else if (method == "createGsps") {
    std::string sourceDicomPath = GetString(*args, "sourceDicomPath");
    std::string annotationsJson = GetString(*args, "annotationsJson");
    std::string outputPath = GetString(*args, "outputPath");

    MediaUploadResult* gspsResult = dcmtk_create_gsps(
        sourceDicomPath.c_str(), annotationsJson.c_str(), outputPath.c_str());

    EncodableMap resultDict;
    if (!gspsResult->success) {
      resultDict[EncodableValue("success")] = EncodableValue(false);
      resultDict[EncodableValue("error")] = EncodableValue(SafeStr(gspsResult->error_message));
    } else {
      resultDict[EncodableValue("success")] = EncodableValue(true);
      resultDict[EncodableValue("studyInstanceUID")] = EncodableValue(SafeStr(gspsResult->study_instance_uid));
      resultDict[EncodableValue("seriesInstanceUID")] = EncodableValue(SafeStr(gspsResult->series_instance_uid));
      resultDict[EncodableValue("sopInstanceUID")] = EncodableValue(SafeStr(gspsResult->sop_instance_uid));
      resultDict[EncodableValue("outputPath")] = EncodableValue(outputPath);
    }
    dcmtk_free_media_upload_result(gspsResult);
    result->Success(EncodableValue(resultDict));

  // ===== parseGsps =====
  } else if (method == "parseGsps") {
    std::string filePath = GetString(*args, "filePath");
    char* jsonStr = dcmtk_parse_gsps(filePath.c_str());
    if (jsonStr) {
      result->Success(EncodableValue(std::string(jsonStr)));
      free(jsonStr);
    } else {
      result->Success(EncodableValue());
    }

  // ===== getDicomTag =====
  } else if (method == "getDicomTag") {
    std::string filePath = GetString(*args, "filePath");
    std::string tagName = GetString(*args, "tagName");
    char* tagValue = dcmtk_get_dicom_tag(filePath.c_str(), tagName.c_str());
    std::string resultStr(tagValue);
    dcmtk_free_string(tagValue);
    result->Success(EncodableValue(resultStr));

  // ===== validateDicomFile =====
  } else if (method == "validateDicomFile") {
    std::string filePath = GetString(*args, "filePath");
    int valid = dcmtk_validate_dicom_file(filePath.c_str());
    result->Success(EncodableValue(valid == 1));

  // ===== extractVideo =====
  } else if (method == "extractVideo") {
    std::string dicomPath = GetString(*args, "dicomPath");
    std::string outputPath = GetString(*args, "outputPath");

    VideoExtractionResult* vidResult = dcmtk_extract_video(dicomPath.c_str(), outputPath.c_str());
    if (!vidResult->success) {
      std::string errorMsg = SafeStr(vidResult->error_message);
      dcmtk_free_video_extraction_result(vidResult);
      result->Error("VIDEO_EXTRACTION_ERROR", errorMsg.empty() ? "Unknown error" : errorMsg);
      return;
    }

    EncodableMap videoResult;
    videoResult[EncodableValue("outputPath")] = EncodableValue(SafeStr(vidResult->output_path));
    videoResult[EncodableValue("mimeType")] = EncodableValue(SafeStr(vidResult->mime_type));
    videoResult[EncodableValue("fileSize")] = EncodableValue(static_cast<int64_t>(vidResult->file_size));
    dcmtk_free_video_extraction_result(vidResult);
    result->Success(EncodableValue(videoResult));

  // ===== storeFiles =====
  } else if (method == "storeFiles") {
    std::string host = GetString(*args, "serverHost");
    int port = GetInt(*args, "serverPort");
    std::string aeTitle = GetString(*args, "aeTitle");
    std::string calledAeTitle = GetString(*args, "calledAeTitle");
    auto filePaths = GetStringList(*args, "filePaths");

    std::vector<const char*> cPaths;
    for (const auto& p : filePaths) cPaths.push_back(p.c_str());

    StoreResult* storeRes = dcmtk_store_files(
        host.c_str(), port, aeTitle.c_str(), calledAeTitle.c_str(),
        cPaths.data(), static_cast<int>(cPaths.size()));

    EncodableMap response;
    response[EncodableValue("successCount")] = EncodableValue(storeRes->success_count);
    response[EncodableValue("failCount")] = EncodableValue(storeRes->fail_count);
    response[EncodableValue("totalCount")] = EncodableValue(storeRes->total_count);
    response[EncodableValue("error")] = EncodableValue(storeRes->error);
    response[EncodableValue("errorMessage")] = EncodableValue(SafeStr(storeRes->error_message));
    dcmtk_free_store_result(storeRes);
    result->Success(EncodableValue(response));

  // ===== startStoreSCP =====
  } else if (method == "startStoreSCP") {
    int port = GetInt(*args, "port");
    std::string aeTitle = GetString(*args, "aeTitle");
    std::string storageDir = GetString(*args, "storageDir");

    // Start SCP in a background thread (it blocks until stopped)
    std::string aeCopy = aeTitle;
    std::string dirCopy = storageDir;
    std::thread([port, aeCopy, dirCopy]() {
      dcmtk_start_store_scp(port, aeCopy.c_str(), dirCopy.c_str());
    }).detach();

    EncodableMap response;
    response[EncodableValue("started")] = EncodableValue(true);
    response[EncodableValue("port")] = EncodableValue(port);
    result->Success(EncodableValue(response));

  // ===== stopStoreSCP =====
  } else if (method == "stopStoreSCP") {
    dcmtk_stop_store_scp();
    EncodableMap response;
    response[EncodableValue("stopped")] = EncodableValue(true);
    result->Success(EncodableValue(response));

  // ===== getStoreSCPStatus =====
  } else if (method == "getStoreSCPStatus") {
    StoreSCPStatus* status = dcmtk_get_store_scp_status();
    EncodableMap response;
    response[EncodableValue("running")] = EncodableValue(status->running);
    response[EncodableValue("port")] = EncodableValue(status->port);
    response[EncodableValue("receivedCount")] = EncodableValue(status->received_count);
    response[EncodableValue("storageDir")] = EncodableValue(SafeStr(status->storage_dir));
    response[EncodableValue("errorMessage")] = EncodableValue(SafeStr(status->error_message));
    dcmtk_free_store_scp_status(status);
    result->Success(EncodableValue(response));

  // ===== moveInstances =====
  } else if (method == "moveInstances") {
    std::string host = GetString(*args, "serverHost");
    int port = GetInt(*args, "serverPort");
    std::string aeTitle = GetString(*args, "aeTitle");
    std::string calledAeTitle = GetString(*args, "calledAeTitle");
    std::string seriesInstanceUID = GetString(*args, "seriesInstanceUID");
    std::string localStoragePath = GetString(*args, "localStoragePath");
    int moveSCPPort = GetInt(*args, "moveSCPPort");

    DicomInstanceQueryResult* moveRes = dcmtk_move_instances(
        host.c_str(), port, aeTitle.c_str(), calledAeTitle.c_str(),
        seriesInstanceUID.c_str(), localStoragePath.c_str(), moveSCPPort);

    EncodableList instances;
    if (moveRes->instances) {
      for (int i = 0; i < moveRes->instance_count; i++) {
        EncodableMap inst;
        inst[EncodableValue("filePath")] = EncodableValue(SafeStr(moveRes->instances[i].file_path));
        inst[EncodableValue("fileSize")] = EncodableValue(moveRes->instances[i].file_size);
        instances.push_back(EncodableValue(inst));
      }
    }

    EncodableMap response;
    response[EncodableValue("instances")] = EncodableValue(instances);
    response[EncodableValue("instanceCount")] = EncodableValue(moveRes->instance_count);
    response[EncodableValue("error")] = EncodableValue(moveRes->error);
    response[EncodableValue("errorMessage")] = EncodableValue(SafeStr(moveRes->error_message));
    dcmtk_free_instance_query_result(moveRes);
    result->Success(EncodableValue(response));

  // ===== setTlsConfig =====
  } else if (method == "setTlsConfig") {
    std::string certFile = GetString(*args, "certFile");
    std::string keyFile = GetString(*args, "keyFile");
    std::string caFile = GetString(*args, "caFile");
    dcmtk_set_tls_config(
        certFile.empty() ? nullptr : certFile.c_str(),
        keyFile.empty() ? nullptr : keyFile.c_str(),
        caFile.empty() ? nullptr : caFile.c_str());
    result->Success(EncodableValue(true));

  // ===== clearTlsConfig =====
  } else if (method == "clearTlsConfig") {
    dcmtk_clear_tls_config();
    result->Success(EncodableValue(true));

  // ===== isTlsAvailable =====
  } else if (method == "isTlsAvailable") {
    int available = dcmtk_is_tls_available();
    result->Success(EncodableValue(available == 1));

  // ===== isTlsEnabled =====
  } else if (method == "isTlsEnabled") {
    int enabled = dcmtk_is_tls_enabled();
    result->Success(EncodableValue(enabled == 1));

  // ===== buildMprVolume =====
  } else if (method == "buildMprVolume") {
    auto filePaths = GetStringList(*args, "filePaths");
    if (filePaths.size() < 3) {
      result->Error("INVALID_ARGUMENT", "Need at least 3 file paths for MPR");
      return;
    }

    std::vector<const char*> cPaths;
    for (const auto& p : filePaths) cPaths.push_back(p.c_str());

    MprVolumeInfo* info = dcmtk_build_mpr_volume(cPaths.data(), static_cast<int>(cPaths.size()));
    if (info->error) {
      std::string errMsg = SafeStr(info->error_message);
      dcmtk_free_mpr_volume_info(info);
      result->Error("MPR_ERROR", errMsg.empty() ? "Unknown MPR error" : errMsg);
      return;
    }

    EncodableMap dict;
    dict[EncodableValue("volumeId")] = EncodableValue(info->volume_id);
    dict[EncodableValue("width")] = EncodableValue(info->width);
    dict[EncodableValue("height")] = EncodableValue(info->height);
    dict[EncodableValue("depth")] = EncodableValue(info->depth);
    dict[EncodableValue("pixelSpacingX")] = EncodableValue(info->pixel_spacing_x);
    dict[EncodableValue("pixelSpacingY")] = EncodableValue(info->pixel_spacing_y);
    dict[EncodableValue("sliceSpacing")] = EncodableValue(info->slice_spacing);
    dict[EncodableValue("windowCenter")] = EncodableValue(info->window_center);
    dict[EncodableValue("windowWidth")] = EncodableValue(info->window_width);
    dcmtk_free_mpr_volume_info(info);
    result->Success(EncodableValue(dict));

  // ===== getMprSlice =====
  } else if (method == "getMprSlice") {
    int volumeId = GetInt(*args, "volumeId");
    int plane = GetInt(*args, "plane");
    int sliceIndex = GetInt(*args, "sliceIndex");
    double wc = GetDouble(*args, "windowCenter", 0.0);
    double ww = GetDouble(*args, "windowWidth", 0.0);

    MprSliceData* slice = dcmtk_get_mpr_slice(volumeId, plane, sliceIndex, wc, ww);
    if (slice->error) {
      std::string errMsg = SafeStr(slice->error_message);
      dcmtk_free_mpr_slice_data(slice);
      result->Error("MPR_ERROR", errMsg.empty() ? "Unknown slice error" : errMsg);
      return;
    }

    int dataLen = slice->width * slice->height * 4;
    std::vector<uint8_t> pixelData(slice->data, slice->data + dataLen);
    EncodableMap dict;
    dict[EncodableValue("data")] = EncodableValue(pixelData);
    dict[EncodableValue("width")] = EncodableValue(slice->width);
    dict[EncodableValue("height")] = EncodableValue(slice->height);
    dcmtk_free_mpr_slice_data(slice);
    result->Success(EncodableValue(dict));

  // ===== freeMprVolume =====
  } else if (method == "freeMprVolume") {
    int volumeId = GetInt(*args, "volumeId");
    dcmtk_free_mpr_volume(volumeId);
    result->Success(EncodableValue());

  // ===== renderMip =====
  } else if (method == "renderMip") {
    int volumeId = GetInt(*args, "volumeId");
    double rx = GetDouble(*args, "rotationX", 0.0);
    double ry = GetDouble(*args, "rotationY", 0.0);
    double wc = GetDouble(*args, "windowCenter", 0.0);
    double ww = GetDouble(*args, "windowWidth", 0.0);

    MprSliceData* mip = dcmtk_render_mip(volumeId, rx, ry, wc, ww);
    if (mip->error) {
      std::string errMsg = SafeStr(mip->error_message);
      dcmtk_free_mpr_slice_data(mip);
      result->Error("MIP_ERROR", errMsg.empty() ? "Unknown MIP error" : errMsg);
      return;
    }

    int dataLen = mip->width * mip->height * 4;
    std::vector<uint8_t> pixelData(mip->data, mip->data + dataLen);
    EncodableMap dict;
    dict[EncodableValue("width")] = EncodableValue(mip->width);
    dict[EncodableValue("height")] = EncodableValue(mip->height);
    dict[EncodableValue("data")] = EncodableValue(pixelData);
    dcmtk_free_mpr_slice_data(mip);
    result->Success(EncodableValue(dict));

  // ===== initDictionary =====
  } else if (method == "initDictionary") {
    std::string dictPath = GetString(*args, "dictionaryPath");
    int loaded = dcmtk_init_dictionary(dictPath.c_str());
    result->Success(EncodableValue(loaded == 1));

  } else {
    result->NotImplemented();
  }
}

}  // namespace dcmtk_flutter
