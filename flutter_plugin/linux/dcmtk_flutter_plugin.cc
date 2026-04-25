#include "include/dcmtk_flutter/dcmtk_flutter_plugin.h"

#include <flutter_linux/flutter_linux.h>
#include <gtk/gtk.h>

#include <cstring>
#include <string>
#include <vector>
#include <thread>

#include "dcmtk_flutter_wrapper.h"

#define DCMTK_FLUTTER_PLUGIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), dcmtk_flutter_plugin_get_type(), \
                              DcmtkFlutterPlugin))

struct _DcmtkFlutterPlugin {
  GObject parent_instance;
};

G_DEFINE_TYPE(DcmtkFlutterPlugin, dcmtk_flutter_plugin, g_object_get_type())

// Helper: get a string value from FlValue map
static std::string GetString(FlValue* args, const char* key) {
  FlValue* val = fl_value_lookup_string(args, key);
  if (val && fl_value_get_type(val) == FL_VALUE_TYPE_STRING) {
    return std::string(fl_value_get_string(val));
  }
  return "";
}

// Helper: get an int value from FlValue map
static int GetInt(FlValue* args, const char* key, int default_val = 0) {
  FlValue* val = fl_value_lookup_string(args, key);
  if (val) {
    if (fl_value_get_type(val) == FL_VALUE_TYPE_INT) {
      return static_cast<int>(fl_value_get_int(val));
    }
  }
  return default_val;
}

// Helper: get a double value from FlValue map
static double GetDouble(FlValue* args, const char* key, double default_val = 0.0) {
  FlValue* val = fl_value_lookup_string(args, key);
  if (val) {
    if (fl_value_get_type(val) == FL_VALUE_TYPE_FLOAT) {
      return fl_value_get_float(val);
    }
    if (fl_value_get_type(val) == FL_VALUE_TYPE_INT) {
      return static_cast<double>(fl_value_get_int(val));
    }
  }
  return default_val;
}

// Helper: get a string list from FlValue map
static std::vector<std::string> GetStringList(FlValue* args, const char* key) {
  std::vector<std::string> result;
  FlValue* val = fl_value_lookup_string(args, key);
  if (val && fl_value_get_type(val) == FL_VALUE_TYPE_LIST) {
    size_t len = fl_value_get_length(val);
    for (size_t i = 0; i < len; i++) {
      FlValue* item = fl_value_get_list_value(val, i);
      if (fl_value_get_type(item) == FL_VALUE_TYPE_STRING) {
        result.push_back(std::string(fl_value_get_string(item)));
      }
    }
  }
  return result;
}

// Helper: safe string from char* (handles nullptr)
static std::string SafeStr(const char* s) {
  return s ? std::string(s) : std::string();
}

static void dcmtk_flutter_plugin_handle_method_call(
    DcmtkFlutterPlugin* self,
    FlMethodCall* method_call) {
  const gchar* method = fl_method_call_get_name(method_call);
  FlValue* args = fl_method_call_get_args(method_call);

  g_autoptr(FlMethodResponse) response = nullptr;

  // ===== loadDicomFile =====
  if (strcmp(method, "loadDicomFile") == 0) {
    std::string filePath = GetString(args, "filePath");
    if (filePath.empty()) {
      response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INVALID_ARGUMENT", "File path is required", nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }
    char* resultStr = dcmtk_load_dicom_file(filePath.c_str());
    std::string resultString(resultStr);
    dcmtk_free_string(resultStr);
    g_autoptr(FlValue) result = fl_value_new_string(resultString.c_str());
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));

  // ===== extractImage =====
  } else if (strcmp(method, "extractImage") == 0) {
    std::string filePath = GetString(args, "filePath");
    if (filePath.empty()) {
      response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INVALID_ARGUMENT", "File path is required", nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }
    int frameIndex = GetInt(args, "frameIndex", 0);
    double wc = GetDouble(args, "windowCenter", 0.0);
    double ww = GetDouble(args, "windowWidth", 0.0);

    DicomImageData* imgData = dcmtk_extract_image(filePath.c_str(), frameIndex, wc, ww);
    if (imgData->error) {
      std::string errorMsg = imgData->error_message ? imgData->error_message : "Unknown error";
      dcmtk_free_image_data(imgData);
      response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "EXTRACTION_ERROR", errorMsg.c_str(), nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }

    int dataLen = imgData->width * imgData->height * 4;
    g_autoptr(FlValue) imageResult = fl_value_new_map();
    fl_value_set_string_take(imageResult, "width", fl_value_new_int(imgData->width));
    fl_value_set_string_take(imageResult, "height", fl_value_new_int(imgData->height));
    fl_value_set_string_take(imageResult, "data",
        fl_value_new_uint8_list(imgData->data, dataLen));
    fl_value_set_string_take(imageResult, "samplesPerPixel", fl_value_new_int(imgData->samples_per_pixel));
    fl_value_set_string_take(imageResult, "bitsStored", fl_value_new_int(imgData->bits_stored));
    fl_value_set_string_take(imageResult, "totalFrames", fl_value_new_int(imgData->total_frames));

    dcmtk_free_image_data(imgData);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(imageResult));

  // ===== testServerConnection =====
  } else if (strcmp(method, "testServerConnection") == 0) {
    std::string host = GetString(args, "serverHost");
    int port = GetInt(args, "serverPort");
    std::string aeTitle = GetString(args, "aeTitle");
    std::string calledAeTitle = GetString(args, "calledAeTitle");

    if (host.empty() || aeTitle.empty() || calledAeTitle.empty()) {
      response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INVALID_ARGUMENT", "Server host, AE title, and called AE title are required", nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }

    int connResult = dcmtk_test_server_connection(host.c_str(), port, aeTitle.c_str(), calledAeTitle.c_str());
    g_autoptr(FlValue) result = fl_value_new_bool(connResult == 1);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));

  // ===== testServerConnectionTls =====
  } else if (strcmp(method, "testServerConnectionTls") == 0) {
    std::string host = GetString(args, "serverHost");
    int port = GetInt(args, "serverPort");
    std::string aeTitle = GetString(args, "aeTitle");
    std::string calledAeTitle = GetString(args, "calledAeTitle");
    std::string certFile = GetString(args, "certFile");
    std::string keyFile = GetString(args, "keyFile");
    std::string caFile = GetString(args, "caFile");

    int tlsResult = dcmtk_test_server_connection_tls(
        host.c_str(), port, aeTitle.c_str(), calledAeTitle.c_str(),
        certFile.c_str(), keyFile.c_str(), caFile.c_str());

    if (tlsResult == -1) {
      response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "TLS_NOT_AVAILABLE", "TLS support not compiled (OpenSSL not available)", nullptr));
    } else {
      g_autoptr(FlValue) result = fl_value_new_bool(tlsResult == 1);
      response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
    }

  // ===== queryPatients =====
  } else if (strcmp(method, "queryPatients") == 0) {
    std::string host = GetString(args, "serverHost");
    int port = GetInt(args, "serverPort");
    std::string aeTitle = GetString(args, "aeTitle");
    std::string calledAeTitle = GetString(args, "calledAeTitle");
    std::string filter = GetString(args, "patientNameFilter");

    DicomQueryResult* queryResult = dcmtk_query_patients(
        host.c_str(), port, aeTitle.c_str(), calledAeTitle.c_str(),
        filter.empty() ? nullptr : filter.c_str());

    if (queryResult->error) {
      std::string errorMsg = SafeStr(queryResult->error_message);
      dcmtk_free_query_result(queryResult);
      response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "QUERY_ERROR", errorMsg.empty() ? "Unknown query error" : errorMsg.c_str(), nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }

    g_autoptr(FlValue) patients = fl_value_new_list();
    for (int i = 0; i < queryResult->patient_count; i++) {
      DicomPatient& p = queryResult->patients[i];
      FlValue* patient = fl_value_new_map();
      fl_value_set_string_take(patient, "patientId", fl_value_new_string(SafeStr(p.patient_id).c_str()));
      fl_value_set_string_take(patient, "patientName", fl_value_new_string(SafeStr(p.patient_name).c_str()));
      fl_value_set_string_take(patient, "patientBirthDate", fl_value_new_string(SafeStr(p.patient_birth_date).c_str()));
      fl_value_set_string_take(patient, "patientSex", fl_value_new_string(SafeStr(p.patient_sex).c_str()));
      fl_value_set_string_take(patient, "studyCount", fl_value_new_int(p.study_count));
      fl_value_set_string_take(patient, "numberOfPatientRelatedStudies", fl_value_new_string(SafeStr(p.number_of_patient_related_studies).c_str()));
      fl_value_append(patients, patient);
      fl_value_unref(patient);
    }
    dcmtk_free_query_result(queryResult);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(patients));

  // ===== queryStudiesForPatient =====
  } else if (strcmp(method, "queryStudiesForPatient") == 0) {
    std::string host = GetString(args, "serverHost");
    int port = GetInt(args, "serverPort");
    std::string aeTitle = GetString(args, "aeTitle");
    std::string calledAeTitle = GetString(args, "calledAeTitle");
    std::string patientId = GetString(args, "patientId");

    DicomStudyQueryResult* queryResult = dcmtk_query_studies_for_patient(
        host.c_str(), port, aeTitle.c_str(), calledAeTitle.c_str(), patientId.c_str());

    if (queryResult->error) {
      std::string errorMsg = SafeStr(queryResult->error_message);
      dcmtk_free_study_query_result(queryResult);
      response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "QUERY_ERROR", errorMsg.empty() ? "Unknown query error" : errorMsg.c_str(), nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }

    g_autoptr(FlValue) studies = fl_value_new_list();
    for (int i = 0; i < queryResult->study_count; i++) {
      DicomStudy& s = queryResult->studies[i];
      FlValue* study = fl_value_new_map();
      fl_value_set_string_take(study, "studyInstanceUID", fl_value_new_string(SafeStr(s.study_instance_uid).c_str()));
      fl_value_set_string_take(study, "studyDate", fl_value_new_string(SafeStr(s.study_date).c_str()));
      fl_value_set_string_take(study, "studyTime", fl_value_new_string(SafeStr(s.study_time).c_str()));
      fl_value_set_string_take(study, "studyDescription", fl_value_new_string(SafeStr(s.study_description).c_str()));
      fl_value_set_string_take(study, "accessionNumber", fl_value_new_string(SafeStr(s.accession_number).c_str()));
      fl_value_set_string_take(study, "seriesCount", fl_value_new_int(s.series_count));
      fl_value_set_string_take(study, "modalitiesInStudy", fl_value_new_string(SafeStr(s.modalities_in_study).c_str()));
      fl_value_set_string_take(study, "numberOfStudyRelatedSeries", fl_value_new_string(SafeStr(s.number_of_study_related_series).c_str()));
      fl_value_set_string_take(study, "numberOfStudyRelatedInstances", fl_value_new_string(SafeStr(s.number_of_study_related_instances).c_str()));
      fl_value_set_string_take(study, "referringPhysicianName", fl_value_new_string(SafeStr(s.referring_physician_name).c_str()));
      fl_value_append(studies, study);
      fl_value_unref(study);
    }
    dcmtk_free_study_query_result(queryResult);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(studies));

  // ===== querySeriesForStudy =====
  } else if (strcmp(method, "querySeriesForStudy") == 0) {
    std::string host = GetString(args, "serverHost");
    int port = GetInt(args, "serverPort");
    std::string aeTitle = GetString(args, "aeTitle");
    std::string calledAeTitle = GetString(args, "calledAeTitle");
    std::string studyInstanceUID = GetString(args, "studyInstanceUID");

    DicomSeriesQueryResult* seriesResult = dcmtk_query_series_for_study(
        host.c_str(), port, aeTitle.c_str(), calledAeTitle.c_str(), studyInstanceUID.c_str());

    if (seriesResult->error) {
      std::string errorMsg = SafeStr(seriesResult->error_message);
      dcmtk_free_series_query_result(seriesResult);
      response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "QUERY_ERROR", errorMsg.empty() ? "Unknown series query error" : errorMsg.c_str(), nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }

    g_autoptr(FlValue) seriesList = fl_value_new_list();
    for (int i = 0; i < seriesResult->series_count; i++) {
      DicomSeries& s = seriesResult->series[i];
      FlValue* series = fl_value_new_map();
      fl_value_set_string_take(series, "seriesInstanceUID", fl_value_new_string(SafeStr(s.series_instance_uid).c_str()));
      fl_value_set_string_take(series, "seriesNumber", fl_value_new_string(SafeStr(s.series_number).c_str()));
      fl_value_set_string_take(series, "seriesDescription", fl_value_new_string(SafeStr(s.series_description).c_str()));
      fl_value_set_string_take(series, "modality", fl_value_new_string(SafeStr(s.modality).c_str()));
      fl_value_set_string_take(series, "seriesDate", fl_value_new_string(SafeStr(s.series_date).c_str()));
      fl_value_set_string_take(series, "seriesTime", fl_value_new_string(SafeStr(s.series_time).c_str()));
      fl_value_set_string_take(series, "instanceCount", fl_value_new_int(s.instance_count));
      fl_value_set_string_take(series, "numberOfSeriesRelatedInstances", fl_value_new_string(SafeStr(s.number_of_series_related_instances).c_str()));
      fl_value_set_string_take(series, "bodyPartExamined", fl_value_new_string(SafeStr(s.body_part_examined).c_str()));
      fl_value_append(seriesList, series);
      fl_value_unref(series);
    }
    dcmtk_free_series_query_result(seriesResult);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(seriesList));

  // ===== queryInstancesForSeries =====
  } else if (strcmp(method, "queryInstancesForSeries") == 0) {
    std::string host = GetString(args, "serverHost");
    int port = GetInt(args, "serverPort");
    std::string aeTitle = GetString(args, "aeTitle");
    std::string calledAeTitle = GetString(args, "calledAeTitle");
    std::string seriesInstanceUID = GetString(args, "seriesInstanceUID");

    DicomInstanceQueryResult* instResult = dcmtk_query_instances_for_series(
        host.c_str(), port, aeTitle.c_str(), calledAeTitle.c_str(), seriesInstanceUID.c_str());

    if (instResult->error) {
      std::string errorMsg = SafeStr(instResult->error_message);
      dcmtk_free_instance_query_result(instResult);
      response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "QUERY_ERROR", errorMsg.empty() ? "Unknown query error" : errorMsg.c_str(), nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }

    g_autoptr(FlValue) instances = fl_value_new_list();
    for (int i = 0; i < instResult->instance_count; i++) {
      DicomInstance& inst = instResult->instances[i];
      FlValue* instanceMap = fl_value_new_map();
      fl_value_set_string_take(instanceMap, "sopInstanceUID", fl_value_new_string(SafeStr(inst.sop_instance_uid).c_str()));
      fl_value_set_string_take(instanceMap, "instanceNumber", fl_value_new_string(SafeStr(inst.instance_number).c_str()));
      fl_value_set_string_take(instanceMap, "filePath", fl_value_new_string(SafeStr(inst.file_path).c_str()));
      fl_value_set_string_take(instanceMap, "contentType", fl_value_new_string(SafeStr(inst.content_type).c_str()));
      fl_value_set_string_take(instanceMap, "fileSize", fl_value_new_int(inst.file_size));
      fl_value_append(instances, instanceMap);
      fl_value_unref(instanceMap);
    }
    dcmtk_free_instance_query_result(instResult);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(instances));

  // ===== downloadInstancesViaCMove =====
  } else if (strcmp(method, "downloadInstancesViaCMove") == 0) {
    std::string host = GetString(args, "serverHost");
    int port = GetInt(args, "serverPort");
    std::string aeTitle = GetString(args, "aeTitle");
    std::string calledAeTitle = GetString(args, "calledAeTitle");
    std::string seriesInstanceUID = GetString(args, "seriesInstanceUID");
    std::string localStoragePath = GetString(args, "localStoragePath");

    DicomInstanceQueryResult* instanceResult = dcmtk_download_instances(
        host.c_str(), port, aeTitle.c_str(), calledAeTitle.c_str(),
        seriesInstanceUID.c_str(), localStoragePath.c_str());

    if (instanceResult->error) {
      std::string errorMsg = SafeStr(instanceResult->error_message);
      dcmtk_free_instance_query_result(instanceResult);
      response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "DOWNLOAD_ERROR", errorMsg.empty() ? "Unknown C-MOVE download error" : errorMsg.c_str(), nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }

    g_autoptr(FlValue) instances = fl_value_new_list();
    for (int i = 0; i < instanceResult->instance_count; i++) {
      DicomInstance& inst = instanceResult->instances[i];
      FlValue* instanceMap = fl_value_new_map();
      fl_value_set_string_take(instanceMap, "sopInstanceUID", fl_value_new_string(SafeStr(inst.sop_instance_uid).c_str()));
      fl_value_set_string_take(instanceMap, "instanceNumber", fl_value_new_string(SafeStr(inst.instance_number).c_str()));
      fl_value_set_string_take(instanceMap, "filePath", fl_value_new_string(SafeStr(inst.file_path).c_str()));
      fl_value_set_string_take(instanceMap, "contentType", fl_value_new_string(inst.content_type ? std::string(inst.content_type).c_str() : "IMAGE"));
      fl_value_set_string_take(instanceMap, "fileSize", fl_value_new_int(inst.file_size));
      fl_value_append(instances, instanceMap);
      fl_value_unref(instanceMap);
    }
    dcmtk_free_instance_query_result(instanceResult);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(instances));

  // ===== createPatient =====
  } else if (strcmp(method, "createPatient") == 0) {
    std::string host = GetString(args, "serverHost");
    int port = GetInt(args, "serverPort");
    std::string aeTitle = GetString(args, "aeTitle");
    std::string calledAeTitle = GetString(args, "calledAeTitle");
    std::string patientId = GetString(args, "patientId");
    std::string patientName = GetString(args, "patientName");
    std::string birthDate = GetString(args, "birthDate");
    std::string sex = GetString(args, "sex");
    std::string comments = GetString(args, "comments");

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
      response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "CREATION_ERROR", errorMsg.empty() ? "Unknown creation error" : errorMsg.c_str(), nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }

    g_autoptr(FlValue) resultDict = fl_value_new_map();
    fl_value_set_string_take(resultDict, "success", fl_value_new_bool(creationResult->success != 0));
    fl_value_set_string_take(resultDict, "patientId", fl_value_new_string(SafeStr(creationResult->generated_patient_id).c_str()));
    fl_value_set_string_take(resultDict, "rspStatusCode", fl_value_new_int(creationResult->rsp_status_code));
    fl_value_set_string_take(resultDict, "warning", fl_value_new_string(SafeStr(creationResult->warning_message).c_str()));
    dcmtk_free_patient_creation_result(creationResult);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(resultDict));

  // ===== uploadImage =====
  } else if (strcmp(method, "uploadImage") == 0) {
    std::string host = GetString(args, "serverHost");
    int port = GetInt(args, "serverPort");
    std::string aeTitle = GetString(args, "aeTitle");
    std::string calledAeTitle = GetString(args, "calledAeTitle");
    std::string patientId = GetString(args, "patientId");
    std::string imagePath = GetString(args, "imagePath");
    std::string patientName = GetString(args, "patientName");
    std::string patientBirthDate = GetString(args, "patientBirthDate");
    std::string studyDesc = GetString(args, "studyDescription");
    std::string seriesDesc = GetString(args, "seriesDescription");
    std::string imageComments = GetString(args, "imageComments");
    std::string modality = GetString(args, "modality");
    std::string studyUID = GetString(args, "studyInstanceUID");
    std::string seriesUID = GetString(args, "seriesInstanceUID");
    int instanceNumber = GetInt(args, "instanceNumber", 1);

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
      response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "UPLOAD_ERROR", errorMsg.empty() ? "Unknown upload error" : errorMsg.c_str(), nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }

    g_autoptr(FlValue) resultDict = fl_value_new_map();
    fl_value_set_string_take(resultDict, "success", fl_value_new_bool(true));
    fl_value_set_string_take(resultDict, "studyInstanceUID", fl_value_new_string(SafeStr(uploadResult->study_instance_uid).c_str()));
    fl_value_set_string_take(resultDict, "seriesInstanceUID", fl_value_new_string(SafeStr(uploadResult->series_instance_uid).c_str()));
    fl_value_set_string_take(resultDict, "sopInstanceUID", fl_value_new_string(SafeStr(uploadResult->sop_instance_uid).c_str()));
    fl_value_set_string_take(resultDict, "rspStatusCode", fl_value_new_int(uploadResult->rsp_status_code));
    dcmtk_free_media_upload_result(uploadResult);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(resultDict));

  // ===== uploadMultiframe =====
  } else if (strcmp(method, "uploadMultiframe") == 0) {
    std::string host = GetString(args, "serverHost");
    int port = GetInt(args, "serverPort");
    std::string aeTitle = GetString(args, "aeTitle");
    std::string calledAeTitle = GetString(args, "calledAeTitle");
    std::string patientId = GetString(args, "patientId");
    auto imagePaths = GetStringList(args, "imagePaths");
    std::string patientName = GetString(args, "patientName");
    std::string patientBirthDate = GetString(args, "patientBirthDate");
    std::string studyDesc = GetString(args, "studyDescription");
    std::string seriesDesc = GetString(args, "seriesDescription");
    std::string imageComments = GetString(args, "imageComments");
    std::string modality = GetString(args, "modality");
    std::string studyUID = GetString(args, "studyInstanceUID");
    std::string seriesUID = GetString(args, "seriesInstanceUID");

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
      response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "UPLOAD_ERROR", errorMsg.empty() ? "Unknown error" : errorMsg.c_str(), nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }

    g_autoptr(FlValue) resultDict = fl_value_new_map();
    fl_value_set_string_take(resultDict, "success", fl_value_new_bool(true));
    fl_value_set_string_take(resultDict, "studyInstanceUID", fl_value_new_string(SafeStr(uploadResult->study_instance_uid).c_str()));
    fl_value_set_string_take(resultDict, "seriesInstanceUID", fl_value_new_string(SafeStr(uploadResult->series_instance_uid).c_str()));
    fl_value_set_string_take(resultDict, "sopInstanceUID", fl_value_new_string(SafeStr(uploadResult->sop_instance_uid).c_str()));
    dcmtk_free_media_upload_result(uploadResult);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(resultDict));

  // ===== uploadVideo =====
  } else if (strcmp(method, "uploadVideo") == 0) {
    std::string host = GetString(args, "serverHost");
    int port = GetInt(args, "serverPort");
    std::string aeTitle = GetString(args, "aeTitle");
    std::string calledAeTitle = GetString(args, "calledAeTitle");
    std::string patientId = GetString(args, "patientId");
    std::string videoPath = GetString(args, "videoPath");
    std::string patientName = GetString(args, "patientName");
    std::string patientBirthDate = GetString(args, "patientBirthDate");
    std::string studyDesc = GetString(args, "studyDescription");
    std::string seriesDesc = GetString(args, "seriesDescription");
    std::string imageComments = GetString(args, "imageComments");
    std::string modality = GetString(args, "modality");

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
      response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "UPLOAD_ERROR", errorMsg.empty() ? "Unknown upload error" : errorMsg.c_str(), nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }

    g_autoptr(FlValue) resultDict = fl_value_new_map();
    fl_value_set_string_take(resultDict, "success", fl_value_new_bool(true));
    fl_value_set_string_take(resultDict, "studyInstanceUID", fl_value_new_string(SafeStr(uploadResult->study_instance_uid).c_str()));
    fl_value_set_string_take(resultDict, "seriesInstanceUID", fl_value_new_string(SafeStr(uploadResult->series_instance_uid).c_str()));
    fl_value_set_string_take(resultDict, "sopInstanceUID", fl_value_new_string(SafeStr(uploadResult->sop_instance_uid).c_str()));
    dcmtk_free_media_upload_result(uploadResult);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(resultDict));

  // ===== convertImageToDicom =====
  } else if (strcmp(method, "convertImageToDicom") == 0) {
    std::string imagePath = GetString(args, "imagePath");
    std::string outputPath = GetString(args, "outputPath");
    std::string patientId = GetString(args, "patientId");
    std::string patientName = GetString(args, "patientName");
    std::string patientBirthDate = GetString(args, "patientBirthDate");
    std::string studyDesc = GetString(args, "studyDescription");
    std::string seriesDesc = GetString(args, "seriesDescription");
    std::string imageComments = GetString(args, "imageComments");
    std::string modality = GetString(args, "modality");
    std::string studyUID = GetString(args, "studyInstanceUID");
    std::string seriesUID = GetString(args, "seriesInstanceUID");
    int instanceNumber = GetInt(args, "instanceNumber", 1);

    MediaUploadResult* convResult = dcmtk_convert_image_to_dicom(
        imagePath.c_str(), outputPath.c_str(),
        patientId.c_str(), patientName.c_str(), patientBirthDate.c_str(),
        studyDesc.empty() ? "Exported Image" : studyDesc.c_str(),
        seriesDesc.empty() ? "Exported Series" : seriesDesc.c_str(),
        imageComments.c_str(),
        modality.empty() ? "SC" : modality.c_str(),
        studyUID.c_str(), seriesUID.c_str(), instanceNumber);

    g_autoptr(FlValue) resultDict = fl_value_new_map();
    if (!convResult->success) {
      fl_value_set_string_take(resultDict, "success", fl_value_new_bool(false));
      fl_value_set_string_take(resultDict, "error", fl_value_new_string(SafeStr(convResult->error_message).c_str()));
    } else {
      fl_value_set_string_take(resultDict, "success", fl_value_new_bool(true));
      fl_value_set_string_take(resultDict, "studyInstanceUID", fl_value_new_string(SafeStr(convResult->study_instance_uid).c_str()));
      fl_value_set_string_take(resultDict, "seriesInstanceUID", fl_value_new_string(SafeStr(convResult->series_instance_uid).c_str()));
      fl_value_set_string_take(resultDict, "sopInstanceUID", fl_value_new_string(SafeStr(convResult->sop_instance_uid).c_str()));
    }
    dcmtk_free_media_upload_result(convResult);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(resultDict));

  // ===== createGsps =====
  } else if (strcmp(method, "createGsps") == 0) {
    std::string sourceDicomPath = GetString(args, "sourceDicomPath");
    std::string annotationsJson = GetString(args, "annotationsJson");
    std::string outputPath = GetString(args, "outputPath");

    MediaUploadResult* gspsResult = dcmtk_create_gsps(
        sourceDicomPath.c_str(), annotationsJson.c_str(), outputPath.c_str());

    g_autoptr(FlValue) resultDict = fl_value_new_map();
    if (!gspsResult->success) {
      fl_value_set_string_take(resultDict, "success", fl_value_new_bool(false));
      fl_value_set_string_take(resultDict, "error", fl_value_new_string(SafeStr(gspsResult->error_message).c_str()));
    } else {
      fl_value_set_string_take(resultDict, "success", fl_value_new_bool(true));
      fl_value_set_string_take(resultDict, "studyInstanceUID", fl_value_new_string(SafeStr(gspsResult->study_instance_uid).c_str()));
      fl_value_set_string_take(resultDict, "seriesInstanceUID", fl_value_new_string(SafeStr(gspsResult->series_instance_uid).c_str()));
      fl_value_set_string_take(resultDict, "sopInstanceUID", fl_value_new_string(SafeStr(gspsResult->sop_instance_uid).c_str()));
      fl_value_set_string_take(resultDict, "outputPath", fl_value_new_string(outputPath.c_str()));
    }
    dcmtk_free_media_upload_result(gspsResult);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(resultDict));

  // ===== parseGsps =====
  } else if (strcmp(method, "parseGsps") == 0) {
    std::string filePath = GetString(args, "filePath");
    char* jsonStr = dcmtk_parse_gsps(filePath.c_str());
    if (jsonStr) {
      g_autoptr(FlValue) result = fl_value_new_string(jsonStr);
      response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
      free(jsonStr);
    } else {
      response = FL_METHOD_RESPONSE(fl_method_success_response_new(fl_value_new_null()));
    }

  // ===== getDicomTag =====
  } else if (strcmp(method, "getDicomTag") == 0) {
    std::string filePath = GetString(args, "filePath");
    std::string tagName = GetString(args, "tagName");
    char* tagValue = dcmtk_get_dicom_tag(filePath.c_str(), tagName.c_str());
    std::string resultStr(tagValue);
    dcmtk_free_string(tagValue);
    g_autoptr(FlValue) result = fl_value_new_string(resultStr.c_str());
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));

  // ===== validateDicomFile =====
  } else if (strcmp(method, "validateDicomFile") == 0) {
    std::string filePath = GetString(args, "filePath");
    int valid = dcmtk_validate_dicom_file(filePath.c_str());
    g_autoptr(FlValue) result = fl_value_new_bool(valid == 1);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));

  // ===== extractVideo =====
  } else if (strcmp(method, "extractVideo") == 0) {
    std::string dicomPath = GetString(args, "dicomPath");
    std::string outputPath = GetString(args, "outputPath");

    VideoExtractionResult* vidResult = dcmtk_extract_video(dicomPath.c_str(), outputPath.c_str());
    if (!vidResult->success) {
      std::string errorMsg = SafeStr(vidResult->error_message);
      dcmtk_free_video_extraction_result(vidResult);
      response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "VIDEO_EXTRACTION_ERROR", errorMsg.empty() ? "Unknown error" : errorMsg.c_str(), nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }

    g_autoptr(FlValue) videoResult = fl_value_new_map();
    fl_value_set_string_take(videoResult, "outputPath", fl_value_new_string(SafeStr(vidResult->output_path).c_str()));
    fl_value_set_string_take(videoResult, "mimeType", fl_value_new_string(SafeStr(vidResult->mime_type).c_str()));
    fl_value_set_string_take(videoResult, "fileSize", fl_value_new_int(static_cast<int64_t>(vidResult->file_size)));
    dcmtk_free_video_extraction_result(vidResult);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(videoResult));

  // ===== storeFiles =====
  } else if (strcmp(method, "storeFiles") == 0) {
    std::string host = GetString(args, "serverHost");
    int port = GetInt(args, "serverPort");
    std::string aeTitle = GetString(args, "aeTitle");
    std::string calledAeTitle = GetString(args, "calledAeTitle");
    auto filePaths = GetStringList(args, "filePaths");

    std::vector<const char*> cPaths;
    for (const auto& p : filePaths) cPaths.push_back(p.c_str());

    StoreResult* storeRes = dcmtk_store_files(
        host.c_str(), port, aeTitle.c_str(), calledAeTitle.c_str(),
        cPaths.data(), static_cast<int>(cPaths.size()));

    g_autoptr(FlValue) resultMap = fl_value_new_map();
    fl_value_set_string_take(resultMap, "successCount", fl_value_new_int(storeRes->success_count));
    fl_value_set_string_take(resultMap, "failCount", fl_value_new_int(storeRes->fail_count));
    fl_value_set_string_take(resultMap, "totalCount", fl_value_new_int(storeRes->total_count));
    fl_value_set_string_take(resultMap, "error", fl_value_new_int(storeRes->error));
    fl_value_set_string_take(resultMap, "errorMessage", fl_value_new_string(SafeStr(storeRes->error_message).c_str()));
    dcmtk_free_store_result(storeRes);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(resultMap));

  // ===== startStoreSCP =====
  } else if (strcmp(method, "startStoreSCP") == 0) {
    int port = GetInt(args, "port");
    std::string aeTitle = GetString(args, "aeTitle");
    std::string storageDir = GetString(args, "storageDir");

    std::string aeCopy = aeTitle;
    std::string dirCopy = storageDir;
    std::thread([port, aeCopy, dirCopy]() {
      dcmtk_start_store_scp(port, aeCopy.c_str(), dirCopy.c_str());
    }).detach();

    g_autoptr(FlValue) resultMap = fl_value_new_map();
    fl_value_set_string_take(resultMap, "started", fl_value_new_bool(true));
    fl_value_set_string_take(resultMap, "port", fl_value_new_int(port));
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(resultMap));

  // ===== stopStoreSCP =====
  } else if (strcmp(method, "stopStoreSCP") == 0) {
    dcmtk_stop_store_scp();
    g_autoptr(FlValue) resultMap = fl_value_new_map();
    fl_value_set_string_take(resultMap, "stopped", fl_value_new_bool(true));
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(resultMap));

  // ===== getStoreSCPStatus =====
  } else if (strcmp(method, "getStoreSCPStatus") == 0) {
    StoreSCPStatus* status = dcmtk_get_store_scp_status();
    g_autoptr(FlValue) resultMap = fl_value_new_map();
    fl_value_set_string_take(resultMap, "running", fl_value_new_int(status->running));
    fl_value_set_string_take(resultMap, "port", fl_value_new_int(status->port));
    fl_value_set_string_take(resultMap, "receivedCount", fl_value_new_int(status->received_count));
    fl_value_set_string_take(resultMap, "storageDir", fl_value_new_string(SafeStr(status->storage_dir).c_str()));
    fl_value_set_string_take(resultMap, "errorMessage", fl_value_new_string(SafeStr(status->error_message).c_str()));
    dcmtk_free_store_scp_status(status);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(resultMap));

  // ===== moveInstances =====
  } else if (strcmp(method, "moveInstances") == 0) {
    std::string host = GetString(args, "serverHost");
    int port = GetInt(args, "serverPort");
    std::string aeTitle = GetString(args, "aeTitle");
    std::string calledAeTitle = GetString(args, "calledAeTitle");
    std::string seriesInstanceUID = GetString(args, "seriesInstanceUID");
    std::string localStoragePath = GetString(args, "localStoragePath");
    int moveSCPPort = GetInt(args, "moveSCPPort");

    DicomInstanceQueryResult* moveRes = dcmtk_move_instances(
        host.c_str(), port, aeTitle.c_str(), calledAeTitle.c_str(),
        seriesInstanceUID.c_str(), localStoragePath.c_str(), moveSCPPort);

    g_autoptr(FlValue) instances = fl_value_new_list();
    if (moveRes->instances) {
      for (int i = 0; i < moveRes->instance_count; i++) {
        FlValue* inst = fl_value_new_map();
        fl_value_set_string_take(inst, "filePath", fl_value_new_string(SafeStr(moveRes->instances[i].file_path).c_str()));
        fl_value_set_string_take(inst, "fileSize", fl_value_new_int(moveRes->instances[i].file_size));
        fl_value_append(instances, inst);
        fl_value_unref(inst);
      }
    }

    g_autoptr(FlValue) resultMap = fl_value_new_map();
    fl_value_set_string_take(resultMap, "instances", fl_value_ref(instances));
    fl_value_set_string_take(resultMap, "instanceCount", fl_value_new_int(moveRes->instance_count));
    fl_value_set_string_take(resultMap, "error", fl_value_new_int(moveRes->error));
    fl_value_set_string_take(resultMap, "errorMessage", fl_value_new_string(SafeStr(moveRes->error_message).c_str()));
    dcmtk_free_instance_query_result(moveRes);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(resultMap));

  // ===== setTlsConfig =====
  } else if (strcmp(method, "setTlsConfig") == 0) {
    std::string certFile = GetString(args, "certFile");
    std::string keyFile = GetString(args, "keyFile");
    std::string caFile = GetString(args, "caFile");
    dcmtk_set_tls_config(
        certFile.empty() ? nullptr : certFile.c_str(),
        keyFile.empty() ? nullptr : keyFile.c_str(),
        caFile.empty() ? nullptr : caFile.c_str());
    g_autoptr(FlValue) result = fl_value_new_bool(true);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));

  // ===== clearTlsConfig =====
  } else if (strcmp(method, "clearTlsConfig") == 0) {
    dcmtk_clear_tls_config();
    g_autoptr(FlValue) result = fl_value_new_bool(true);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));

  // ===== isTlsAvailable =====
  } else if (strcmp(method, "isTlsAvailable") == 0) {
    int available = dcmtk_is_tls_available();
    g_autoptr(FlValue) result = fl_value_new_bool(available == 1);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));

  // ===== isTlsEnabled =====
  } else if (strcmp(method, "isTlsEnabled") == 0) {
    int enabled = dcmtk_is_tls_enabled();
    g_autoptr(FlValue) result = fl_value_new_bool(enabled == 1);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));

  // ===== buildMprVolume =====
  } else if (strcmp(method, "buildMprVolume") == 0) {
    auto filePaths = GetStringList(args, "filePaths");
    if (filePaths.size() < 1) {
      response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INVALID_ARGUMENT", "Need at least 1 file path for MPR", nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }

    std::vector<const char*> cPaths;
    for (const auto& p : filePaths) cPaths.push_back(p.c_str());

    MprVolumeInfo* info = dcmtk_build_mpr_volume(cPaths.data(), static_cast<int>(cPaths.size()));
    if (info->error) {
      std::string errMsg = SafeStr(info->error_message);
      dcmtk_free_mpr_volume_info(info);
      response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "MPR_ERROR", errMsg.empty() ? "Unknown MPR error" : errMsg.c_str(), nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }

    g_autoptr(FlValue) dict = fl_value_new_map();
    fl_value_set_string_take(dict, "volumeId", fl_value_new_int(info->volume_id));
    fl_value_set_string_take(dict, "width", fl_value_new_int(info->width));
    fl_value_set_string_take(dict, "height", fl_value_new_int(info->height));
    fl_value_set_string_take(dict, "depth", fl_value_new_int(info->depth));
    fl_value_set_string_take(dict, "pixelSpacingX", fl_value_new_float(info->pixel_spacing_x));
    fl_value_set_string_take(dict, "pixelSpacingY", fl_value_new_float(info->pixel_spacing_y));
    fl_value_set_string_take(dict, "sliceSpacing", fl_value_new_float(info->slice_spacing));
    fl_value_set_string_take(dict, "windowCenter", fl_value_new_float(info->window_center));
    fl_value_set_string_take(dict, "windowWidth", fl_value_new_float(info->window_width));
    dcmtk_free_mpr_volume_info(info);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(dict));

  // ===== getMprSlice =====
  } else if (strcmp(method, "getMprSlice") == 0) {
    int volumeId = GetInt(args, "volumeId");
    int plane = GetInt(args, "plane");
    int sliceIndex = GetInt(args, "sliceIndex");
    double wc = GetDouble(args, "windowCenter", 0.0);
    double ww = GetDouble(args, "windowWidth", 0.0);

    MprSliceData* slice = dcmtk_get_mpr_slice(volumeId, plane, sliceIndex, wc, ww);
    if (slice->error) {
      std::string errMsg = SafeStr(slice->error_message);
      dcmtk_free_mpr_slice_data(slice);
      response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "MPR_ERROR", errMsg.empty() ? "Unknown slice error" : errMsg.c_str(), nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }

    int dataLen = slice->width * slice->height * 4;
    g_autoptr(FlValue) dict = fl_value_new_map();
    fl_value_set_string_take(dict, "data",
        fl_value_new_uint8_list(slice->data, dataLen));
    fl_value_set_string_take(dict, "width", fl_value_new_int(slice->width));
    fl_value_set_string_take(dict, "height", fl_value_new_int(slice->height));
    dcmtk_free_mpr_slice_data(slice);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(dict));

  // ===== freeMprVolume =====
  } else if (strcmp(method, "freeMprVolume") == 0) {
    int volumeId = GetInt(args, "volumeId");
    dcmtk_free_mpr_volume(volumeId);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(fl_value_new_null()));

  // ===== renderMip =====
  } else if (strcmp(method, "renderMip") == 0) {
    int volumeId = GetInt(args, "volumeId");
    double rx = GetDouble(args, "rotationX", 0.0);
    double ry = GetDouble(args, "rotationY", 0.0);
    double wc = GetDouble(args, "windowCenter", 0.0);
    double ww = GetDouble(args, "windowWidth", 0.0);

    MprSliceData* mip = dcmtk_render_mip(volumeId, rx, ry, wc, ww);
    if (mip->error) {
      std::string errMsg = SafeStr(mip->error_message);
      dcmtk_free_mpr_slice_data(mip);
      response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "MIP_ERROR", errMsg.empty() ? "Unknown MIP error" : errMsg.c_str(), nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }

    int dataLen = mip->width * mip->height * 4;
    g_autoptr(FlValue) dict = fl_value_new_map();
    fl_value_set_string_take(dict, "width", fl_value_new_int(mip->width));
    fl_value_set_string_take(dict, "height", fl_value_new_int(mip->height));
    fl_value_set_string_take(dict, "data",
        fl_value_new_uint8_list(mip->data, dataLen));
    dcmtk_free_mpr_slice_data(mip);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(dict));

  // ===== renderVolume =====
  } else if (strcmp(method, "renderVolume") == 0) {
    int volumeId = GetInt(args, "volumeId");
    double rx = GetDouble(args, "rotationX", 0.0);
    double ry = GetDouble(args, "rotationY", 0.0);
    double wc = GetDouble(args, "windowCenter", 0.0);
    double ww = GetDouble(args, "windowWidth", 0.0);
    std::string preset = GetString(args, "preset");
    if (preset.empty()) preset = "Muscle";
    int previewMode = 0;
    FlValue* previewVal = fl_value_lookup_string(args, "preview");
    if (previewVal && fl_value_get_type(previewVal) == FL_VALUE_TYPE_BOOL) {
      previewMode = fl_value_get_bool(previewVal) ? 1 : 0;
    }

    MprSliceData* volume = dcmtk_render_volume(volumeId, rx, ry, wc, ww, preset.c_str(), previewMode);
    if (volume->error) {
      std::string errMsg = SafeStr(volume->error_message);
      dcmtk_free_mpr_slice_data(volume);
      response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "VOLUME_RENDER_ERROR", errMsg.empty() ? "Unknown volume rendering error" : errMsg.c_str(), nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }

    int dataLen = volume->width * volume->height * 4;
    g_autoptr(FlValue) dict = fl_value_new_map();
    fl_value_set_string_take(dict, "width", fl_value_new_int(volume->width));
    fl_value_set_string_take(dict, "height", fl_value_new_int(volume->height));
    fl_value_set_string_take(dict, "data",
        fl_value_new_uint8_list(volume->data, dataLen));
    dcmtk_free_mpr_slice_data(volume);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(dict));

  // ===== initDictionary =====
  } else if (strcmp(method, "initDictionary") == 0) {
    std::string dictPath = GetString(args, "dictionaryPath");
    int loaded = dcmtk_init_dictionary(dictPath.c_str());
    g_autoptr(FlValue) result = fl_value_new_bool(loaded == 1);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));

  } else {
    response = FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
  }

  fl_method_call_respond(method_call, response, nullptr);
}

static void dcmtk_flutter_plugin_dispose(GObject* object) {
  G_OBJECT_CLASS(dcmtk_flutter_plugin_parent_class)->dispose(object);
}

static void dcmtk_flutter_plugin_class_init(DcmtkFlutterPluginClass* klass) {
  G_OBJECT_CLASS(klass)->dispose = dcmtk_flutter_plugin_dispose;
}

static void dcmtk_flutter_plugin_init(DcmtkFlutterPlugin* self) {}

static void method_call_cb(FlMethodChannel* channel, FlMethodCall* method_call,
                           gpointer user_data) {
  DcmtkFlutterPlugin* plugin = DCMTK_FLUTTER_PLUGIN(user_data);
  dcmtk_flutter_plugin_handle_method_call(plugin, method_call);
}

void dcmtk_flutter_plugin_register_with_registrar(FlPluginRegistrar* registrar) {
  DcmtkFlutterPlugin* plugin = DCMTK_FLUTTER_PLUGIN(
      g_object_new(dcmtk_flutter_plugin_get_type(), nullptr));

  g_autoptr(FlStandardMethodCodec) codec = fl_standard_method_codec_new();
  g_autoptr(FlMethodChannel) channel =
      fl_method_channel_new(fl_plugin_registrar_get_messenger(registrar),
                            "dcmtk_flutter",
                            FL_METHOD_CODEC(codec));
  fl_method_channel_set_method_call_handler(channel, method_call_cb,
                                            g_object_ref(plugin),
                                            g_object_unref);

  g_object_unref(plugin);
}
