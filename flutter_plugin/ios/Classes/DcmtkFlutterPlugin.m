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
    
    DicomImageData* dcmtk_extract_image(const char* filename, int frame_index);
    void dcmtk_free_image_data(DicomImageData* img_data);
    
    // DICOM Server Communication Functions
    typedef struct {
        char* patient_id;
        char* patient_name;
        char* patient_birth_date;
        char* patient_sex;
        int study_count;
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
    MediaUploadResult* dcmtk_upload_image(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* patient_id, const char* image_path, const char* study_description, const char* series_description, const char* image_comments, const char* modality);
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
    
    if (filePath == nil || filePath.length == 0) {
      result([FlutterError errorWithCode:@"INVALID_ARGUMENT"
                                 message:@"File path is required"
                                 details:nil]);
      return;
    }
    
    int frame = frameIndex ? [frameIndex intValue] : 0;
    const char* cFilePath = [filePath UTF8String];
    DicomImageData* imgData = dcmtk_extract_image(cFilePath, frame);
    
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
        @"studyCount": @(patient->study_count)
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
        @"seriesCount": @(study->series_count)
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
        @"instanceCount": @(series->instance_count)
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
    
    printf("[iOS] Parameters - Host: %s, Port: %d, Patient: %s, Image: %s, Study: %s\\n", 
           [serverHost UTF8String], [serverPort intValue], 
           [patientId UTF8String], [imagePath UTF8String],
           studyDescription ? [studyDescription UTF8String] : "NULL");
    fflush(stdout);
    
    if (serverHost == nil || serverPort == nil || aeTitle == nil || calledAeTitle == nil || patientId == nil || imagePath == nil) {
      printf("[iOS] Missing required parameters for uploadImage\\n");
      fflush(stdout);
      result([FlutterError errorWithCode:@"INVALID_ARGUMENT"
                                 message:@"Server host, port, AE title, called AE title, patient ID, and image path are required"
                                 details:nil]);
      return;
    }
    
    const char* cServerHost = [serverHost UTF8String];
    int cServerPort = [serverPort intValue];
    const char* cAeTitle = [aeTitle UTF8String];
    const char* cCalledAeTitle = [calledAeTitle UTF8String];
    const char* cPatientId = [patientId UTF8String];
    const char* cImagePath = [imagePath UTF8String];
    const char* cStudyDescription = studyDescription ? [studyDescription UTF8String] : "Uploaded Image";
    const char* cSeriesDescription = seriesDescription ? [seriesDescription UTF8String] : "Uploaded Series";
    const char* cImageComments = imageComments ? [imageComments UTF8String] : "";
    const char* cModality = modality ? [modality UTF8String] : "SC";
    
    printf("[iOS] Calling dcmtk_upload_image native function\\n");
    fflush(stdout);
    MediaUploadResult* uploadResult = dcmtk_upload_image(cServerHost, cServerPort, cAeTitle, cCalledAeTitle, cPatientId, cImagePath, cStudyDescription, cSeriesDescription, cImageComments, cModality);
    
    printf("[iOS] dcmtk_upload_image returned, success: %d\\n", uploadResult->success);
    fflush(stdout);
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
    
  } else {
    result(FlutterMethodNotImplemented);
  }
}

@end
