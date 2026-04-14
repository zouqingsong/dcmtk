#import "DcmtkFlutterPlugin.h"

// Import C functions from the DCMTK wrapper
#ifdef __cplusplus
extern "C" {
#endif
    char* dcmtk_load_dicom_file(const char* filename);
    void dcmtk_free_string(char* str);
    
    typedef struct {
        unsigned char* data;
        int width;
        int height;
        int samples_per_pixel;
        int bits_stored;
        int total_frames;
        int error;
        char* error_message;
    } DicomImageData;
    
    DicomImageData* dcmtk_extract_image(const char* filename, int frame_index, double window_center, double window_width);
    void dcmtk_free_image_data(DicomImageData* img_data);
    
    // DICOM Server Communication Functions
    typedef struct {
        char* patient_id;
        char* patient_name;
        char* patient_birth_date;
        char* patient_sex;
        int study_count;
        char* number_of_patient_related_studies;
    } DicomPatient;
    
    typedef struct {
        DicomPatient* patients;
        int patient_count;
        int error;
        char* error_message;
    } DicomQueryResult;

    typedef struct {
      char* study_instance_uid;
      char* study_date;
      char* study_time;
      char* study_description;
      char* accession_number;
      int series_count;
      char* modalities_in_study;
      char* number_of_study_related_series;
      char* number_of_study_related_instances;
      char* referring_physician_name;
    } DicomStudy;

    typedef struct {
      DicomStudy* studies;
      int study_count;
      int error;
      char* error_message;
    } DicomStudyQueryResult;
    
    typedef struct {
        char* series_instance_uid;
        char* series_number;
        char* series_description;
        char* modality;
        char* series_date;
        char* series_time;
        int instance_count;
        char* number_of_series_related_instances;
        char* body_part_examined;
    } DicomSeries;
    
    typedef struct {
        DicomSeries* series;
        int series_count;
        int error;
        char* error_message;
    } DicomSeriesQueryResult;
    
    typedef struct {
        char* sop_instance_uid;
        char* instance_number;
        char* file_path;
        char* content_type;
        int file_size;
    } DicomInstance;
    
    typedef struct {
        DicomInstance* instances;
        int instance_count;
        int error;
        char* error_message;
    } DicomInstanceQueryResult;
    
    typedef struct {
        char* patient_id;
        char* patient_name;
        char* patient_birth_date;
        char* patient_sex;
        char* patient_comments;
    } PatientInfo;
    
    typedef struct {
        int success;
        char* error_message;
        char* generated_patient_id;
    } PatientCreationResult;
    
    typedef struct {
        int success;
        char* error_message;
        char* study_instance_uid;
        char* series_instance_uid;
        char* sop_instance_uid;
    } MediaUploadResult;
    
    int dcmtk_test_server_connection(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title);
    DicomQueryResult* dcmtk_query_patients(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title);
    DicomStudyQueryResult* dcmtk_query_studies_for_patient(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* patient_id);
    DicomSeriesQueryResult* dcmtk_query_series_for_study(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* study_instance_uid);
    PatientCreationResult* dcmtk_create_patient(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, PatientInfo* patient_info);
    MediaUploadResult* dcmtk_upload_image(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* patient_id, const char* image_path, const char* study_description, const char* series_description, const char* image_comments, const char* modality, const char* study_instance_uid, const char* series_instance_uid, int instance_number);
    MediaUploadResult* dcmtk_upload_multiframe(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* patient_id, const char** image_paths, int image_count, const char* study_description, const char* series_description, const char* image_comments, const char* modality, const char* study_instance_uid, const char* series_instance_uid);
    MediaUploadResult* dcmtk_upload_video(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* patient_id, const char* video_path, const char* study_description, const char* series_description, const char* image_comments, const char* modality);
    void dcmtk_free_query_result(DicomQueryResult* result);
    void dcmtk_free_study_query_result(DicomStudyQueryResult* result);
    void dcmtk_free_series_query_result(DicomSeriesQueryResult* result);
    DicomInstanceQueryResult* dcmtk_download_instances(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* series_instance_uid, const char* local_storage_path);
    void dcmtk_free_instance_query_result(DicomInstanceQueryResult* result);
    void dcmtk_free_patient_creation_result(PatientCreationResult* result);
    void dcmtk_free_media_upload_result(MediaUploadResult* result);
    DicomInstanceQueryResult* dcmtk_query_instances_for_series(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* series_instance_uid);
    char* dcmtk_get_dicom_tag(const char* file_path, const char* tag_name);
    int dcmtk_validate_dicom_file(const char* file_path);
    int dcmtk_test_server_connection_tls(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title,
                                          const char* cert_file, const char* key_file, const char* ca_file);

    typedef struct {
        int success;
        char* error_message;
        char* output_path;
        char* mime_type;
        long file_size;
    } VideoExtractionResult;

    VideoExtractionResult* dcmtk_extract_video(const char* dicom_path, const char* output_path);
    void dcmtk_free_video_extraction_result(VideoExtractionResult* result);

    // C-STORE SCU: Send existing DICOM files
    typedef struct {
        int success_count;
        int fail_count;
        int total_count;
        int error;
        char* error_message;
    } StoreResult;

    StoreResult* dcmtk_store_files(const char* server_host, int server_port,
                                    const char* ae_title, const char* called_ae_title,
                                    const char** file_paths, int file_count);
    void dcmtk_free_store_result(StoreResult* result);

    // C-STORE SCP: Receive DICOM files
    typedef struct {
        int running;
        int port;
        int received_count;
        char* storage_dir;
        char* error_message;
    } StoreSCPStatus;

    int dcmtk_start_store_scp(int port, const char* ae_title, const char* storage_dir);
    void dcmtk_stop_store_scp(void);
    StoreSCPStatus* dcmtk_get_store_scp_status(void);
    void dcmtk_free_store_scp_status(StoreSCPStatus* status);

    // C-MOVE
    DicomInstanceQueryResult* dcmtk_move_instances(const char* server_host, int server_port,
                                                    const char* ae_title, const char* called_ae_title,
                                                    const char* series_instance_uid,
                                                    const char* local_storage_path,
                                                    int move_scp_port);

    // TLS Configuration
    void dcmtk_set_tls_config(const char* cert_file, const char* key_file, const char* ca_file);
    void dcmtk_clear_tls_config(void);
    int dcmtk_is_tls_available(void);
    int dcmtk_is_tls_enabled(void);
#ifdef __cplusplus
}
#endif

@implementation DcmtkFlutterPlugin
+ (void)registerWithRegistrar:(NSObject<FlutterPluginRegistrar>*)registrar {
  FlutterMethodChannel* channel = [FlutterMethodChannel
      methodChannelWithName:@"dcmtk_flutter"
            binaryMessenger:[registrar messenger]];
  DcmtkFlutterPlugin* instance = [[DcmtkFlutterPlugin alloc] init];
  [registrar addMethodCallDelegate:instance channel:channel];
}

- (void)handleMethodCall:(FlutterMethodCall*)call result:(FlutterResult)result {
  if ([@"loadDicomFile" isEqualToString:call.method]) {
    NSString* filePath = call.arguments[@"filePath"];
    if (filePath == nil || filePath.length == 0) {
      result([FlutterError errorWithCode:@"INVALID_ARGUMENT"
                                 message:@"File path is required"
                                 details:nil]);
      return;
    }
    
    const char* cFilePath = [filePath UTF8String];
    const char* resultStr = dcmtk_load_dicom_file(cFilePath);
    
    NSString* resultNSString = [NSString stringWithUTF8String:resultStr];
    dcmtk_free_string(resultStr);
    
    result(resultNSString);
  } else if ([@"extractImage" isEqualToString:call.method]) {
    NSString* filePath = call.arguments[@"filePath"];
    NSNumber* frameIndex = call.arguments[@"frameIndex"];
    NSNumber* windowCenter = call.arguments[@"windowCenter"];
    NSNumber* windowWidth = call.arguments[@"windowWidth"];
    
    if (filePath == nil || filePath.length == 0) {
      result([FlutterError errorWithCode:@"INVALID_ARGUMENT"
                                 message:@"File path is required"
                                 details:nil]);
      return;
    }
    
    int frame = frameIndex ? [frameIndex intValue] : 0;
    double wc = windowCenter ? [windowCenter doubleValue] : 0.0;
    double ww = windowWidth ? [windowWidth doubleValue] : 0.0;
    const char* cFilePath = [filePath UTF8String];
    DicomImageData* imgData = dcmtk_extract_image(cFilePath, frame, wc, ww);
    
    if (imgData->error) {
      NSString* errorMsg = imgData->error_message ? 
          [NSString stringWithUTF8String:imgData->error_message] : @"Unknown error";
      dcmtk_free_image_data(imgData);
      result([FlutterError errorWithCode:@"EXTRACTION_ERROR"
                                 message:errorMsg
                                 details:nil]);
      return;
    }
    
    // Convert RGBA data to FlutterStandardTypedData
    NSData* pixelData = [NSData dataWithBytes:imgData->data 
                                       length:imgData->width * imgData->height * 4];
    FlutterStandardTypedData* typedData = [FlutterStandardTypedData typedDataWithBytes:pixelData];
    
    NSDictionary* imageResult = @{
      @"width": @(imgData->width),
      @"height": @(imgData->height),
      @"data": typedData,
      @"samplesPerPixel": @(imgData->samples_per_pixel),
      @"bitsStored": @(imgData->bits_stored),
      @"totalFrames": @(imgData->total_frames)
    };
    
    dcmtk_free_image_data(imgData);
    result(imageResult);
  } else if ([@"testServerConnection" isEqualToString:call.method]) {
    NSString* serverHost = call.arguments[@"serverHost"];
    NSNumber* serverPort = call.arguments[@"serverPort"];
    NSString* aeTitle = call.arguments[@"aeTitle"];
    NSString* calledAeTitle = call.arguments[@"calledAeTitle"];
    
    if (serverHost == nil || serverPort == nil || aeTitle == nil || calledAeTitle == nil) {
      result([FlutterError errorWithCode:@"INVALID_ARGUMENT"
                                 message:@"Server host, port, AE title, and called AE title are required"
                                 details:nil]);
      return;
    }
    
    const char* cServerHost = [serverHost UTF8String];
    int cServerPort = [serverPort intValue];
    const char* cAeTitle = [aeTitle UTF8String];
    const char* cCalledAeTitle = [calledAeTitle UTF8String];
    
    int connectionResult = dcmtk_test_server_connection(cServerHost, cServerPort, cAeTitle, cCalledAeTitle);
    result([NSNumber numberWithBool:(connectionResult == 1)]);
    
  } else if ([@"queryPatients" isEqualToString:call.method]) {
    NSString* serverHost = call.arguments[@"serverHost"];
    NSNumber* serverPort = call.arguments[@"serverPort"];
    NSString* aeTitle = call.arguments[@"aeTitle"];
    NSString* calledAeTitle = call.arguments[@"calledAeTitle"];
    
    if (serverHost == nil || serverPort == nil || aeTitle == nil || calledAeTitle == nil) {
      result([FlutterError errorWithCode:@"INVALID_ARGUMENT"
                                 message:@"Server host, port, AE title, and called AE title are required"
                                 details:nil]);
      return;
    }
    
    const char* cServerHost = [serverHost UTF8String];
    int cServerPort = [serverPort intValue];
    const char* cAeTitle = [aeTitle UTF8String];
    const char* cCalledAeTitle = [calledAeTitle UTF8String];
    
    DicomQueryResult* queryResult = dcmtk_query_patients(cServerHost, cServerPort, cAeTitle, cCalledAeTitle);
    
    if (queryResult->error) {
      NSString* errorMsg = queryResult->error_message ? 
          [NSString stringWithUTF8String:queryResult->error_message] : @"Unknown query error";
      dcmtk_free_query_result(queryResult);
      result([FlutterError errorWithCode:@"QUERY_ERROR"
                                 message:errorMsg
                                 details:nil]);
      return;
    }
    
    // Convert patients to Flutter format
    NSMutableArray* patientsArray = [NSMutableArray array];
    for (int i = 0; i < queryResult->patient_count; i++) {
      DicomPatient* patient = &queryResult->patients[i];
      NSDictionary* patientDict = @{
        @"patientId": patient->patient_id ? [NSString stringWithUTF8String:patient->patient_id] : @"",
        @"patientName": patient->patient_name ? [NSString stringWithUTF8String:patient->patient_name] : @"",
        @"patientBirthDate": patient->patient_birth_date ? [NSString stringWithUTF8String:patient->patient_birth_date] : @"",
        @"patientSex": patient->patient_sex ? [NSString stringWithUTF8String:patient->patient_sex] : @"",
        @"studyCount": @(patient->study_count),
        @"numberOfPatientRelatedStudies": patient->number_of_patient_related_studies ? [NSString stringWithUTF8String:patient->number_of_patient_related_studies] : @""
      };
      [patientsArray addObject:patientDict];
    }
    
    dcmtk_free_query_result(queryResult);
    result(patientsArray);
    
  } else if ([@"queryStudiesForPatient" isEqualToString:call.method]) {
    NSString* serverHost = call.arguments[@"serverHost"];
    NSNumber* serverPort = call.arguments[@"serverPort"];
    NSString* aeTitle = call.arguments[@"aeTitle"];
    NSString* calledAeTitle = call.arguments[@"calledAeTitle"];
    NSString* patientId = call.arguments[@"patientId"];
    
    if (serverHost == nil || serverPort == nil || aeTitle == nil || calledAeTitle == nil || patientId == nil) {
      result([FlutterError errorWithCode:@"INVALID_ARGUMENT"
                                 message:@"Server host, port, AE title, called AE title, and patient ID are required"
                                 details:nil]);
      return;
    }
    
    const char* cServerHost = [serverHost UTF8String];
    int cServerPort = [serverPort intValue];
    const char* cAeTitle = [aeTitle UTF8String];
    const char* cCalledAeTitle = [calledAeTitle UTF8String];
    const char* cPatientId = [patientId UTF8String];
    
    DicomStudyQueryResult* queryResult = dcmtk_query_studies_for_patient(cServerHost, cServerPort, cAeTitle, cCalledAeTitle, cPatientId);
    
    if (queryResult->error) {
      NSString* errorMsg = queryResult->error_message ? 
          [NSString stringWithUTF8String:queryResult->error_message] : @"Unknown query error";
      dcmtk_free_study_query_result(queryResult);
      result([FlutterError errorWithCode:@"QUERY_ERROR"
                                 message:errorMsg
                                 details:nil]);
      return;
    }

    NSMutableArray* studiesArray = [NSMutableArray array];
    for (int i = 0; i < queryResult->study_count; i++) {
      DicomStudy* study = &queryResult->studies[i];
      NSDictionary* studyDict = @{
        @"studyInstanceUID": study->study_instance_uid ? [NSString stringWithUTF8String:study->study_instance_uid] : @"",
        @"studyDate": study->study_date ? [NSString stringWithUTF8String:study->study_date] : @"",
        @"studyTime": study->study_time ? [NSString stringWithUTF8String:study->study_time] : @"",
        @"studyDescription": study->study_description ? [NSString stringWithUTF8String:study->study_description] : @"",
        @"accessionNumber": study->accession_number ? [NSString stringWithUTF8String:study->accession_number] : @"",
        @"seriesCount": @(study->series_count),
        @"modalitiesInStudy": study->modalities_in_study ? [NSString stringWithUTF8String:study->modalities_in_study] : @"",
        @"numberOfStudyRelatedSeries": study->number_of_study_related_series ? [NSString stringWithUTF8String:study->number_of_study_related_series] : @"",
        @"numberOfStudyRelatedInstances": study->number_of_study_related_instances ? [NSString stringWithUTF8String:study->number_of_study_related_instances] : @"",
        @"referringPhysicianName": study->referring_physician_name ? [NSString stringWithUTF8String:study->referring_physician_name] : @""
      };
      [studiesArray addObject:studyDict];
    }

    dcmtk_free_study_query_result(queryResult);
    result(studiesArray);
    
  } else if ([@"querySeriesForStudy" isEqualToString:call.method]) {
    NSString* serverHost = call.arguments[@"serverHost"];
    NSNumber* serverPort = call.arguments[@"serverPort"];
    NSString* aeTitle = call.arguments[@"aeTitle"];
    NSString* calledAeTitle = call.arguments[@"calledAeTitle"];
    NSString* studyInstanceUID = call.arguments[@"studyInstanceUID"];
    
    if (serverHost == nil || serverPort == nil || aeTitle == nil || calledAeTitle == nil || studyInstanceUID == nil) {
      result([FlutterError errorWithCode:@"INVALID_ARGUMENT"
                                 message:@"Server host, port, AE title, called AE title, and study instance UID are required"
                                 details:nil]);
      return;
    }
    
    const char* cServerHost = [serverHost UTF8String];
    int cServerPort = [serverPort intValue];
    const char* cAeTitle = [aeTitle UTF8String];
    const char* cCalledAeTitle = [calledAeTitle UTF8String];
    const char* cStudyInstanceUID = [studyInstanceUID UTF8String];
    
    DicomSeriesQueryResult* seriesResult = dcmtk_query_series_for_study(cServerHost, cServerPort, cAeTitle, cCalledAeTitle, cStudyInstanceUID);
    
    if (seriesResult->error) {
      NSString* errorMsg = seriesResult->error_message ? 
          [NSString stringWithUTF8String:seriesResult->error_message] : @"Unknown series query error";
      dcmtk_free_series_query_result(seriesResult);
      result([FlutterError errorWithCode:@"QUERY_ERROR"
                                 message:errorMsg
                                 details:nil]);
      return;
    }
    
    NSMutableArray* seriesArray = [NSMutableArray array];
    for (int i = 0; i < seriesResult->series_count; i++) {
      DicomSeries* series = &seriesResult->series[i];
      NSDictionary* seriesDict = @{
        @"seriesInstanceUID": series->series_instance_uid ? [NSString stringWithUTF8String:series->series_instance_uid] : @"",
        @"seriesNumber": series->series_number ? [NSString stringWithUTF8String:series->series_number] : @"",
        @"seriesDescription": series->series_description ? [NSString stringWithUTF8String:series->series_description] : @"",
        @"modality": series->modality ? [NSString stringWithUTF8String:series->modality] : @"",
        @"seriesDate": series->series_date ? [NSString stringWithUTF8String:series->series_date] : @"",
        @"seriesTime": series->series_time ? [NSString stringWithUTF8String:series->series_time] : @"",
        @"instanceCount": @(series->instance_count),
        @"numberOfSeriesRelatedInstances": series->number_of_series_related_instances ? [NSString stringWithUTF8String:series->number_of_series_related_instances] : @"",
        @"bodyPartExamined": series->body_part_examined ? [NSString stringWithUTF8String:series->body_part_examined] : @""
      };
      [seriesArray addObject:seriesDict];
    }

    dcmtk_free_series_query_result(seriesResult);
    result(seriesArray);

  } else if ([@"downloadInstancesViaCMove" isEqualToString:call.method]) {
    NSString* serverHost = call.arguments[@"serverHost"];
    NSNumber* serverPort = call.arguments[@"serverPort"];
    NSString* aeTitle = call.arguments[@"aeTitle"];
    NSString* calledAeTitle = call.arguments[@"calledAeTitle"];
    NSString* seriesInstanceUID = call.arguments[@"seriesInstanceUID"];
    NSString* localStoragePath = call.arguments[@"localStoragePath"];
    
    if (serverHost == nil || serverPort == nil || aeTitle == nil || calledAeTitle == nil || seriesInstanceUID == nil || localStoragePath == nil) {
      result([FlutterError errorWithCode:@"INVALID_ARGUMENT"
                                 message:@"Server host, port, AE title, called AE title, series instance UID, and local storage path are required"
                                 details:nil]);
      return;
    }
    
    const char* cServerHost = [serverHost UTF8String];
    int cServerPort = [serverPort intValue];
    const char* cAeTitle = [aeTitle UTF8String];
    const char* cCalledAeTitle = [calledAeTitle UTF8String];
    const char* cSeriesInstanceUID = [seriesInstanceUID UTF8String];
    const char* cLocalStoragePath = [localStoragePath UTF8String];
    
    DicomInstanceQueryResult* instanceResult = dcmtk_download_instances(cServerHost, cServerPort, cAeTitle, cCalledAeTitle, cSeriesInstanceUID, cLocalStoragePath);
    
    if (instanceResult->error) {
      NSString* errorMsg = instanceResult->error_message ? 
          [NSString stringWithUTF8String:instanceResult->error_message] : @"Unknown C-MOVE download error";
      dcmtk_free_instance_query_result(instanceResult);
      result([FlutterError errorWithCode:@"DOWNLOAD_ERROR"
                                 message:errorMsg
                                 details:nil]);
      return;
    }
    
    NSMutableArray* instanceArray = [NSMutableArray array];
    for (int i = 0; i < instanceResult->instance_count; i++) {
      DicomInstance* instance = &instanceResult->instances[i];
      NSDictionary* instanceDict = @{
        @"sopInstanceUID": instance->sop_instance_uid ? [NSString stringWithUTF8String:instance->sop_instance_uid] : @"",
        @"instanceNumber": instance->instance_number ? [NSString stringWithUTF8String:instance->instance_number] : @"",
        @"filePath": instance->file_path ? [NSString stringWithUTF8String:instance->file_path] : @"",
        @"contentType": instance->content_type ? [NSString stringWithUTF8String:instance->content_type] : @"IMAGE",
        @"fileSize": @(instance->file_size)
      };
      [instanceArray addObject:instanceDict];
    }

    dcmtk_free_instance_query_result(instanceResult);
    result(instanceArray);
    
  } else if ([@"createPatient" isEqualToString:call.method]) {
    printf("[iOS] createPatient method called\\n");
    fflush(stdout);
    NSString* serverHost = call.arguments[@"serverHost"];
    NSNumber* serverPort = call.arguments[@"serverPort"];
    NSString* aeTitle = call.arguments[@"aeTitle"];
    NSString* calledAeTitle = call.arguments[@"calledAeTitle"];
    NSString* patientId = call.arguments[@"patientId"];
    NSString* patientName = call.arguments[@"patientName"];
    NSString* birthDate = call.arguments[@"birthDate"];
    NSString* sex = call.arguments[@"sex"];
    NSString* comments = call.arguments[@"comments"];
    
    printf("[iOS] Parameters - Host: %s, Port: %d, Patient ID: %s, Name: %s\\n", 
           [serverHost UTF8String], [serverPort intValue], 
           [patientId UTF8String], [patientName UTF8String]);
    fflush(stdout);
    
    if (serverHost == nil || serverPort == nil || aeTitle == nil || calledAeTitle == nil || patientId == nil) {
      printf("[iOS] Missing required parameters for createPatient\\n");
      fflush(stdout);
      result([FlutterError errorWithCode:@"INVALID_ARGUMENT"
                                 message:@"Server host, port, AE title, called AE title, and patient ID are required"
                                 details:nil]);
      return;
    }
    
    const char* cServerHost = [serverHost UTF8String];
    int cServerPort = [serverPort intValue];
    const char* cAeTitle = [aeTitle UTF8String];
    const char* cCalledAeTitle = [calledAeTitle UTF8String];
    
    PatientInfo patientInfo;
    patientInfo.patient_id = patientId ? [patientId UTF8String] : "";
    patientInfo.patient_name = patientName ? [patientName UTF8String] : "";
    patientInfo.patient_birth_date = birthDate ? [birthDate UTF8String] : "";
    patientInfo.patient_sex = sex ? [sex UTF8String] : "";
    patientInfo.patient_comments = comments ? [comments UTF8String] : "";
    
    printf("[iOS] Calling dcmtk_create_patient native function\\n");
    fflush(stdout);
    PatientCreationResult* creationResult = dcmtk_create_patient(cServerHost, cServerPort, cAeTitle, cCalledAeTitle, &patientInfo);
    
    printf("[iOS] dcmtk_create_patient returned, success: %d\\n", creationResult->success);
    fflush(stdout);
    if (!creationResult->success) {
      NSString* errorMsg = creationResult->error_message ? 
          [NSString stringWithUTF8String:creationResult->error_message] : @"Unknown creation error";
      printf("[iOS] Patient creation failed with error: %s\\n", [errorMsg UTF8String]);
      fflush(stdout);
      dcmtk_free_patient_creation_result(creationResult);
      result([FlutterError errorWithCode:@"CREATION_ERROR"
                                 message:errorMsg
                                 details:nil]);
      return;
    }
    
    NSDictionary* resultDict = @{
      @"success": @(creationResult->success),
      @"patientId": creationResult->generated_patient_id ? [NSString stringWithUTF8String:creationResult->generated_patient_id] : @""
    };
    
    dcmtk_free_patient_creation_result(creationResult);
    result(resultDict);
    
  } else if ([@"uploadImage" isEqualToString:call.method]) {
    printf("[iOS] uploadImage method called\\n");
    fflush(stdout);
    NSString* serverHost = call.arguments[@"serverHost"];
    NSNumber* serverPort = call.arguments[@"serverPort"];
    NSString* aeTitle = call.arguments[@"aeTitle"];
    NSString* calledAeTitle = call.arguments[@"calledAeTitle"];
    NSString* patientId = call.arguments[@"patientId"];
    NSString* imagePath = call.arguments[@"imagePath"];
    NSString* studyDescription = call.arguments[@"studyDescription"];
    NSString* seriesDescription = call.arguments[@"seriesDescription"];
    NSString* imageComments = call.arguments[@"imageComments"];
    NSString* modality = call.arguments[@"modality"];
    NSString* studyInstanceUID = call.arguments[@"studyInstanceUID"];
    NSString* seriesInstanceUID = call.arguments[@"seriesInstanceUID"];
    NSNumber* instanceNumber = call.arguments[@"instanceNumber"];
    
    printf("[iOS] Parameters - Host: %s, Port: %d, Patient: %s, Image: %s, Instance#: %d\\n", 
           [serverHost UTF8String], [serverPort intValue], 
           [patientId UTF8String], [imagePath UTF8String],
           instanceNumber ? [instanceNumber intValue] : 1);
    fflush(stdout);
    
    if (serverHost == nil || serverPort == nil || aeTitle == nil || calledAeTitle == nil || patientId == nil || imagePath == nil) {
      printf("[iOS] Missing required parameters for uploadImage\\n");
      fflush(stdout);
      result([FlutterError errorWithCode:@"INVALID_ARGUMENT"
                                 message:@"Server host, port, AE title, called AE title, patient ID, and image path are required"
                                 details:nil]);
      return;
    }
    
    // Copy all params for background dispatch
    NSString* sHost = [serverHost copy];
    NSNumber* sPort = [serverPort copy];
    NSString* sAe = [aeTitle copy];
    NSString* sCalled = [calledAeTitle copy];
    NSString* sPid = [patientId copy];
    NSString* sPath = [imagePath copy];
    NSString* sStudyDesc = studyDescription ? [studyDescription copy] : nil;
    NSString* sSeriesDesc = seriesDescription ? [seriesDescription copy] : nil;
    NSString* sComments = imageComments ? [imageComments copy] : nil;
    NSString* sModality = modality ? [modality copy] : nil;
    NSString* sStudyUID = studyInstanceUID ? [studyInstanceUID copy] : nil;
    NSString* sSeriesUID = seriesInstanceUID ? [seriesInstanceUID copy] : nil;
    int instNum = instanceNumber ? [instanceNumber intValue] : 1;
    
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
      const char* cServerHost = [sHost UTF8String];
      int cServerPort = [sPort intValue];
      const char* cAeTitle = [sAe UTF8String];
      const char* cCalledAeTitle = [sCalled UTF8String];
      const char* cPatientId = [sPid UTF8String];
      const char* cImagePath = [sPath UTF8String];
      const char* cStudyDescription = sStudyDesc ? [sStudyDesc UTF8String] : "Uploaded Image";
      const char* cSeriesDescription = sSeriesDesc ? [sSeriesDesc UTF8String] : "Uploaded Series";
      const char* cImageComments = sComments ? [sComments UTF8String] : "";
      const char* cModality = sModality ? [sModality UTF8String] : "SC";
      const char* cStudyInstanceUID = sStudyUID ? [sStudyUID UTF8String] : "";
      const char* cSeriesInstanceUID = sSeriesUID ? [sSeriesUID UTF8String] : "";
    
      printf("[iOS] Calling dcmtk_upload_image native function on background thread\\n");
      fflush(stdout);
      MediaUploadResult* uploadResult = dcmtk_upload_image(cServerHost, cServerPort, cAeTitle, cCalledAeTitle, cPatientId, cImagePath, cStudyDescription, cSeriesDescription, cImageComments, cModality, cStudyInstanceUID, cSeriesInstanceUID, instNum);
    
      printf("[iOS] dcmtk_upload_image returned, success: %d\\n", uploadResult->success);
      fflush(stdout);
      
      dispatch_async(dispatch_get_main_queue(), ^{
        if (!uploadResult->success) {
          NSString* errorMsg = uploadResult->error_message ? 
              [NSString stringWithUTF8String:uploadResult->error_message] : @"Unknown upload error";
          printf("[iOS] Upload failed with error: %s\\n", [errorMsg UTF8String]);
          fflush(stdout);
          dcmtk_free_media_upload_result(uploadResult);
          result([FlutterError errorWithCode:@"UPLOAD_ERROR"
                                     message:errorMsg
                                     details:nil]);
          return;
        }
    
        NSDictionary* resultDict = @{
          @"success": @(uploadResult->success),
          @"studyInstanceUID": uploadResult->study_instance_uid ? [NSString stringWithUTF8String:uploadResult->study_instance_uid] : @"",
          @"seriesInstanceUID": uploadResult->series_instance_uid ? [NSString stringWithUTF8String:uploadResult->series_instance_uid] : @"",
          @"sopInstanceUID": uploadResult->sop_instance_uid ? [NSString stringWithUTF8String:uploadResult->sop_instance_uid] : @""
        };
    
        dcmtk_free_media_upload_result(uploadResult);
        result(resultDict);
      });
    });

  } else if ([@"uploadMultiframe" isEqualToString:call.method]) {
    printf("[iOS] uploadMultiframe method called\\n");
    fflush(stdout);
    NSString* serverHost = call.arguments[@"serverHost"];
    NSNumber* serverPort = call.arguments[@"serverPort"];
    NSString* aeTitle = call.arguments[@"aeTitle"];
    NSString* calledAeTitle = call.arguments[@"calledAeTitle"];
    NSString* patientId = call.arguments[@"patientId"];
    NSArray<NSString*>* imagePaths = call.arguments[@"imagePaths"];
    NSString* studyDescription = call.arguments[@"studyDescription"];
    NSString* seriesDescription = call.arguments[@"seriesDescription"];
    NSString* imageComments = call.arguments[@"imageComments"];
    NSString* modality = call.arguments[@"modality"];
    NSString* studyInstanceUID = call.arguments[@"studyInstanceUID"];
    NSString* seriesInstanceUID = call.arguments[@"seriesInstanceUID"];
    
    if (serverHost == nil || serverPort == nil || aeTitle == nil || calledAeTitle == nil || patientId == nil || imagePaths == nil || imagePaths.count == 0) {
      result([FlutterError errorWithCode:@"INVALID_ARGUMENT"
                                 message:@"Required parameters missing for multi-frame upload"
                                 details:nil]);
      return;
    }
    
    // Copy params for background dispatch
    NSString* sHost = [serverHost copy];
    NSNumber* sPort = [serverPort copy];
    NSString* sAe = [aeTitle copy];
    NSString* sCalled = [calledAeTitle copy];
    NSString* sPid = [patientId copy];
    NSArray<NSString*>* sPaths = [imagePaths copy];
    NSString* sStudyDesc = studyDescription ? [studyDescription copy] : nil;
    NSString* sSeriesDesc = seriesDescription ? [seriesDescription copy] : nil;
    NSString* sComments = imageComments ? [imageComments copy] : nil;
    NSString* sModality = modality ? [modality copy] : nil;
    NSString* sStudyUID = studyInstanceUID ? [studyInstanceUID copy] : nil;
    NSString* sSeriesUID = seriesInstanceUID ? [seriesInstanceUID copy] : nil;
    
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
      int imageCount = (int)sPaths.count;
      const char** cPaths = (const char**)malloc(imageCount * sizeof(const char*));
      for (int i = 0; i < imageCount; i++) {
        cPaths[i] = [sPaths[i] UTF8String];
      }
      
      MediaUploadResult* uploadResult = dcmtk_upload_multiframe(
        [sHost UTF8String], [sPort intValue], [sAe UTF8String], [sCalled UTF8String],
        [sPid UTF8String], cPaths, imageCount,
        sStudyDesc ? [sStudyDesc UTF8String] : "Uploaded Study",
        sSeriesDesc ? [sSeriesDesc UTF8String] : "Multi-frame Series",
        sComments ? [sComments UTF8String] : "",
        sModality ? [sModality UTF8String] : "SC",
        sStudyUID ? [sStudyUID UTF8String] : "",
        sSeriesUID ? [sSeriesUID UTF8String] : "");
      
      free(cPaths);
      
      dispatch_async(dispatch_get_main_queue(), ^{
        if (!uploadResult->success) {
          NSString* errorMsg = uploadResult->error_message ?
              [NSString stringWithUTF8String:uploadResult->error_message] : @"Unknown error";
          dcmtk_free_media_upload_result(uploadResult);
          result([FlutterError errorWithCode:@"UPLOAD_ERROR" message:errorMsg details:nil]);
          return;
        }
        
        NSDictionary* resultDict = @{
          @"success": @(uploadResult->success),
          @"studyInstanceUID": uploadResult->study_instance_uid ? [NSString stringWithUTF8String:uploadResult->study_instance_uid] : @"",
          @"seriesInstanceUID": uploadResult->series_instance_uid ? [NSString stringWithUTF8String:uploadResult->series_instance_uid] : @"",
          @"sopInstanceUID": uploadResult->sop_instance_uid ? [NSString stringWithUTF8String:uploadResult->sop_instance_uid] : @""
        };
        dcmtk_free_media_upload_result(uploadResult);
        result(resultDict);
      });
    });
    
  } else if ([@"uploadVideo" isEqualToString:call.method]) {
    NSString* serverHost = call.arguments[@"serverHost"];
    NSNumber* serverPort = call.arguments[@"serverPort"];
    NSString* aeTitle = call.arguments[@"aeTitle"];
    NSString* calledAeTitle = call.arguments[@"calledAeTitle"];
    NSString* patientId = call.arguments[@"patientId"];
    NSString* videoPath = call.arguments[@"videoPath"];
    NSString* studyDescription = call.arguments[@"studyDescription"];
    NSString* seriesDescription = call.arguments[@"seriesDescription"];
    NSString* imageComments = call.arguments[@"imageComments"];
    NSString* modality = call.arguments[@"modality"];
    
    if (serverHost == nil || serverPort == nil || aeTitle == nil || calledAeTitle == nil || patientId == nil || videoPath == nil) {
      result([FlutterError errorWithCode:@"INVALID_ARGUMENT"
                                 message:@"Server host, port, AE title, called AE title, patient ID, and video path are required"
                                 details:nil]);
      return;
    }
    
    MediaUploadResult* uploadResult = dcmtk_upload_video(
      [serverHost UTF8String], [serverPort intValue], [aeTitle UTF8String], [calledAeTitle UTF8String],
      [patientId UTF8String], [videoPath UTF8String],
      studyDescription ? [studyDescription UTF8String] : "Uploaded Video",
      seriesDescription ? [seriesDescription UTF8String] : "Uploaded Video Series",
      imageComments ? [imageComments UTF8String] : "",
      modality ? [modality UTF8String] : "SC");
    
    if (!uploadResult->success) {
      NSString* errorMsg = uploadResult->error_message ?
          [NSString stringWithUTF8String:uploadResult->error_message] : @"Unknown upload error";
      dcmtk_free_media_upload_result(uploadResult);
      result([FlutterError errorWithCode:@"UPLOAD_ERROR" message:errorMsg details:nil]);
      return;
    }
    
    NSDictionary* resultDict = @{
      @"success": @(uploadResult->success),
      @"studyInstanceUID": uploadResult->study_instance_uid ? [NSString stringWithUTF8String:uploadResult->study_instance_uid] : @"",
      @"seriesInstanceUID": uploadResult->series_instance_uid ? [NSString stringWithUTF8String:uploadResult->series_instance_uid] : @"",
      @"sopInstanceUID": uploadResult->sop_instance_uid ? [NSString stringWithUTF8String:uploadResult->sop_instance_uid] : @""
    };
    dcmtk_free_media_upload_result(uploadResult);
    result(resultDict);
    
  } else if ([@"queryInstancesForSeries" isEqualToString:call.method]) {
    NSString* serverHost = call.arguments[@"serverHost"];
    NSNumber* serverPort = call.arguments[@"serverPort"];
    NSString* aeTitle = call.arguments[@"aeTitle"];
    NSString* calledAeTitle = call.arguments[@"calledAeTitle"];
    NSString* seriesInstanceUID = call.arguments[@"seriesInstanceUID"];
    
    if (serverHost == nil || serverPort == nil || aeTitle == nil || calledAeTitle == nil || seriesInstanceUID == nil) {
      result([FlutterError errorWithCode:@"INVALID_ARGUMENT"
                                 message:@"Server host, port, AE title, called AE title, and series instance UID are required"
                                 details:nil]);
      return;
    }
    
    DicomInstanceQueryResult* instResult = dcmtk_query_instances_for_series(
      [serverHost UTF8String], [serverPort intValue], [aeTitle UTF8String],
      [calledAeTitle UTF8String], [seriesInstanceUID UTF8String]);
    
    if (instResult->error) {
      NSString* errorMsg = instResult->error_message ?
          [NSString stringWithUTF8String:instResult->error_message] : @"Unknown query error";
      dcmtk_free_instance_query_result(instResult);
      result([FlutterError errorWithCode:@"QUERY_ERROR" message:errorMsg details:nil]);
      return;
    }
    
    NSMutableArray* instanceArray = [NSMutableArray array];
    for (int i = 0; i < instResult->instance_count; i++) {
      DicomInstance* instance = &instResult->instances[i];
      [instanceArray addObject:@{
        @"sopInstanceUID": instance->sop_instance_uid ? [NSString stringWithUTF8String:instance->sop_instance_uid] : @"",
        @"instanceNumber": instance->instance_number ? [NSString stringWithUTF8String:instance->instance_number] : @"",
        @"filePath": instance->file_path ? [NSString stringWithUTF8String:instance->file_path] : @"",
        @"contentType": instance->content_type ? [NSString stringWithUTF8String:instance->content_type] : @"IMAGE",
        @"fileSize": @(instance->file_size)
      }];
    }
    dcmtk_free_instance_query_result(instResult);
    result(instanceArray);
    
  } else if ([@"getDicomTag" isEqualToString:call.method]) {
    NSString* filePath = call.arguments[@"filePath"];
    NSString* tagName = call.arguments[@"tagName"];
    
    if (filePath == nil || tagName == nil) {
      result([FlutterError errorWithCode:@"INVALID_ARGUMENT"
                                 message:@"File path and tag name are required"
                                 details:nil]);
      return;
    }
    
    char* tagValue = dcmtk_get_dicom_tag([filePath UTF8String], [tagName UTF8String]);
    NSString* resultStr = [NSString stringWithUTF8String:tagValue];
    dcmtk_free_string(tagValue);
    result(resultStr);
    
  } else if ([@"validateDicomFile" isEqualToString:call.method]) {
    NSString* filePath = call.arguments[@"filePath"];
    
    if (filePath == nil) {
      result([FlutterError errorWithCode:@"INVALID_ARGUMENT"
                                 message:@"File path is required"
                                 details:nil]);
      return;
    }
    
    int valid = dcmtk_validate_dicom_file([filePath UTF8String]);
    result(@(valid == 1));
    
  } else if ([@"testServerConnectionTls" isEqualToString:call.method]) {
    NSString* serverHost = call.arguments[@"serverHost"];
    NSNumber* serverPort = call.arguments[@"serverPort"];
    NSString* aeTitle = call.arguments[@"aeTitle"];
    NSString* calledAeTitle = call.arguments[@"calledAeTitle"];
    NSString* certFile = call.arguments[@"certFile"];
    NSString* keyFile = call.arguments[@"keyFile"];
    NSString* caFile = call.arguments[@"caFile"];
    
    if (serverHost == nil || serverPort == nil || aeTitle == nil || calledAeTitle == nil) {
      result([FlutterError errorWithCode:@"INVALID_ARGUMENT"
                                 message:@"Server host, port, AE title, and called AE title are required"
                                 details:nil]);
      return;
    }
    
    int tlsResult = dcmtk_test_server_connection_tls(
      [serverHost UTF8String], [serverPort intValue],
      [aeTitle UTF8String], [calledAeTitle UTF8String],
      certFile ? [certFile UTF8String] : "",
      keyFile ? [keyFile UTF8String] : "",
      caFile ? [caFile UTF8String] : "");
    
    if (tlsResult == -1) {
      result([FlutterError errorWithCode:@"TLS_NOT_AVAILABLE"
                                 message:@"TLS support not compiled (OpenSSL not available)"
                                 details:nil]);
    } else {
      result(@(tlsResult == 1));
    }
    
  } else if ([@"extractVideo" isEqualToString:call.method]) {
    NSString* dicomPath = call.arguments[@"dicomPath"];
    NSString* outputPath = call.arguments[@"outputPath"];
    
    if (dicomPath == nil || outputPath == nil) {
      result([FlutterError errorWithCode:@"INVALID_ARGUMENT"
                                 message:@"dicomPath and outputPath are required"
                                 details:nil]);
      return;
    }
    
    VideoExtractionResult* vidResult = dcmtk_extract_video([dicomPath UTF8String], [outputPath UTF8String]);
    
    if (!vidResult->success) {
      NSString* errorMsg = vidResult->error_message ?
          [NSString stringWithUTF8String:vidResult->error_message] : @"Unknown error";
      dcmtk_free_video_extraction_result(vidResult);
      result([FlutterError errorWithCode:@"VIDEO_EXTRACTION_ERROR"
                                 message:errorMsg
                                 details:nil]);
      return;
    }
    
    NSDictionary* videoResult = @{
      @"outputPath": [NSString stringWithUTF8String:vidResult->output_path],
      @"mimeType": [NSString stringWithUTF8String:vidResult->mime_type],
      @"fileSize": @(vidResult->file_size),
    };
    
    dcmtk_free_video_extraction_result(vidResult);
    result(videoResult);
    
  } else if ([@"storeFiles" isEqualToString:call.method]) {
    // C-STORE SCU: Send existing DICOM files to a remote PACS
    NSString *serverHost = call.arguments[@"serverHost"];
    NSNumber *serverPort = call.arguments[@"serverPort"];
    NSString *aeTitle = call.arguments[@"aeTitle"];
    NSString *calledAeTitle = call.arguments[@"calledAeTitle"];
    NSArray *filePaths = call.arguments[@"filePaths"];

    // Copy params for background thread
    NSString *hostCopy = [serverHost copy];
    int portVal = [serverPort intValue];
    NSString *aeCopy = [aeTitle copy];
    NSString *calledCopy = [calledAeTitle copy];
    NSArray *pathsCopy = [filePaths copy];

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        int count = (int)[pathsCopy count];
        const char** cPaths = (const char**)malloc(sizeof(const char*) * count);
        for (int i = 0; i < count; i++) {
            cPaths[i] = [pathsCopy[i] UTF8String];
        }

        StoreResult* storeRes = dcmtk_store_files(
            [hostCopy UTF8String], portVal,
            [aeCopy UTF8String], [calledCopy UTF8String],
            cPaths, count);
        free(cPaths);

        NSDictionary *response = @{
            @"successCount": @(storeRes->success_count),
            @"failCount": @(storeRes->fail_count),
            @"totalCount": @(storeRes->total_count),
            @"error": @(storeRes->error),
            @"errorMessage": storeRes->error_message ? [NSString stringWithUTF8String:storeRes->error_message] : @"",
        };
        dcmtk_free_store_result(storeRes);

        dispatch_async(dispatch_get_main_queue(), ^{
            result(response);
        });
    });

  } else if ([@"startStoreSCP" isEqualToString:call.method]) {
    // Start C-STORE SCP listener
    NSNumber *port = call.arguments[@"port"];
    NSString *aeTitle = call.arguments[@"aeTitle"];
    NSString *storageDir = call.arguments[@"storageDir"];

    int portVal = [port intValue];
    NSString *aeCopy = [aeTitle copy];
    NSString *dirCopy = [storageDir copy];

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        int res = dcmtk_start_store_scp(portVal, [aeCopy UTF8String], [dirCopy UTF8String]);
        // This blocks until SCP stops, so we don't call result here
        // The SCP runs until stopped
        NSLog(@"[DcmtkPlugin] SCP exited with code: %d", res);
    });

    // Return immediately — SCP is running in background
    result(@{@"started": @YES, @"port": port});

  } else if ([@"stopStoreSCP" isEqualToString:call.method]) {
    dcmtk_stop_store_scp();
    result(@{@"stopped": @YES});

  } else if ([@"getStoreSCPStatus" isEqualToString:call.method]) {
    StoreSCPStatus* status = dcmtk_get_store_scp_status();
    NSDictionary *response = @{
        @"running": @(status->running),
        @"port": @(status->port),
        @"receivedCount": @(status->received_count),
        @"storageDir": status->storage_dir ? [NSString stringWithUTF8String:status->storage_dir] : @"",
        @"errorMessage": status->error_message ? [NSString stringWithUTF8String:status->error_message] : @"",
    };
    dcmtk_free_store_scp_status(status);
    result(response);

  } else if ([@"moveInstances" isEqualToString:call.method]) {
    // C-MOVE retrieval
    NSString *serverHost = call.arguments[@"serverHost"];
    NSNumber *serverPort = call.arguments[@"serverPort"];
    NSString *aeTitle = call.arguments[@"aeTitle"];
    NSString *calledAeTitle = call.arguments[@"calledAeTitle"];
    NSString *seriesInstanceUID = call.arguments[@"seriesInstanceUID"];
    NSString *localStoragePath = call.arguments[@"localStoragePath"];
    NSNumber *moveSCPPort = call.arguments[@"moveSCPPort"];

    NSString *hostCopy = [serverHost copy];
    int portVal = [serverPort intValue];
    NSString *aeCopy = [aeTitle copy];
    NSString *calledCopy = [calledAeTitle copy];
    NSString *seriesCopy = [seriesInstanceUID copy];
    NSString *storageCopy = [localStoragePath copy];
    int scpPort = [moveSCPPort intValue];

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        DicomInstanceQueryResult* moveRes = dcmtk_move_instances(
            [hostCopy UTF8String], portVal,
            [aeCopy UTF8String], [calledCopy UTF8String],
            [seriesCopy UTF8String], [storageCopy UTF8String],
            scpPort);

        NSMutableArray *instances = [NSMutableArray array];
        if (moveRes->instances) {
            for (int i = 0; i < moveRes->instance_count; i++) {
                NSMutableDictionary *inst = [NSMutableDictionary dictionary];
                if (moveRes->instances[i].file_path) {
                    inst[@"filePath"] = [NSString stringWithUTF8String:moveRes->instances[i].file_path];
                }
                inst[@"fileSize"] = @(moveRes->instances[i].file_size);
                [instances addObject:inst];
            }
        }

        NSDictionary *response = @{
            @"instances": instances,
            @"instanceCount": @(moveRes->instance_count),
            @"error": @(moveRes->error),
            @"errorMessage": moveRes->error_message ? [NSString stringWithUTF8String:moveRes->error_message] : @"",
        };
        dcmtk_free_instance_query_result(moveRes);

        dispatch_async(dispatch_get_main_queue(), ^{
            result(response);
        });
    });

  } else if ([@"setTlsConfig" isEqualToString:call.method]) {
    NSString* certFile = call.arguments[@"certFile"];
    NSString* keyFile = call.arguments[@"keyFile"];
    NSString* caFile = call.arguments[@"caFile"];
    dcmtk_set_tls_config(
        certFile ? [certFile UTF8String] : NULL,
        keyFile  ? [keyFile UTF8String]  : NULL,
        caFile   ? [caFile UTF8String]   : NULL
    );
    result(@YES);

  } else if ([@"clearTlsConfig" isEqualToString:call.method]) {
    dcmtk_clear_tls_config();
    result(@YES);

  } else if ([@"isTlsAvailable" isEqualToString:call.method]) {
    int available = dcmtk_is_tls_available();
    result(@(available == 1));

  } else if ([@"isTlsEnabled" isEqualToString:call.method]) {
    int enabled = dcmtk_is_tls_enabled();
    result(@(enabled == 1));

  } else {
    result(FlutterMethodNotImplemented);
  }
}

@end
