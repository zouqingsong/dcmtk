#ifndef DCMTK_FLUTTER_WRAPPER_H
#define DCMTK_FLUTTER_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

// C API for Flutter FFI
// Returns malloc'd string (caller must free with dcmtk_free_string)
char* dcmtk_load_dicom_file(const char* filename);
void dcmtk_free_string(char* str);

// Get specific DICOM tag value by group,element (e.g. "0010,0010" for PatientName)
// Returns malloc'd string with the tag value, or error message
char* dcmtk_get_dicom_tag(const char* file_path, const char* tag_name);

// Version and validation
char* dcmtk_get_version(void);
// Returns 1 if valid DICOM, 0 otherwise. If details != NULL, writes diagnostic string.
int dcmtk_validate_dicom_file(const char* file_path);

// Legacy conversion API: convert DICOM to raw image bytes (8-bit grayscale)
unsigned char* dcmtk_convert_to_image(const char* file_path, const char* format, int* data_size);
void dcmtk_free_image_data_buffer(unsigned char* data);

// Image extraction - structured result (supports grayscale and color)
typedef struct {
    unsigned char* data;    // RGBA pixel data (4 bytes per pixel)
    int width;
    int height;
    int samples_per_pixel;  // 1=grayscale, 3=RGB, 4=RGBA
    int bits_stored;
    int total_frames;       // Total number of frames in the DICOM file
    int error;
    char* error_message;
} DicomImageData;

DicomImageData* dcmtk_extract_image(const char* filename, int frame_index);
void dcmtk_free_image_data(DicomImageData* img_data);

// DICOM Server Communication
typedef struct {
    char* patient_id;
    char* patient_name;
    char* patient_birth_date;
    char* patient_sex;
    int study_count;
} DicomPatient;

typedef struct {
    char* study_instance_uid;
    char* study_date;
    char* study_time;
    char* study_description;
    char* accession_number;
    int series_count;
} DicomStudy;

typedef struct {
    DicomPatient* patients;
    int patient_count;
    int error;
    char* error_message;
} DicomQueryResult;

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
    char* sop_instance_uid;
    char* instance_number;
    char* file_path;
    char* content_type;
    int file_size;
} DicomInstance;

typedef struct {
    DicomSeries* series;
    int series_count;
    int error;
    char* error_message;
} DicomSeriesQueryResult;

typedef struct {
    DicomInstance* instances;
    int instance_count;
    int error;
    char* error_message;
} DicomInstanceQueryResult;

// Patient management operations
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

// Media upload result
typedef struct {
    int success;
    char* error_message;
    char* study_instance_uid;
    char* series_instance_uid;
    char* sop_instance_uid;
} MediaUploadResult;

// Server connection and query functions
int dcmtk_test_server_connection(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title);
// TLS-enabled server connection test
int dcmtk_test_server_connection_tls(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title,
                                      const char* cert_file, const char* key_file, const char* ca_file);
DicomQueryResult* dcmtk_query_patients(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title);
DicomStudyQueryResult* dcmtk_query_studies_for_patient(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* patient_id);

// Extended query functions
DicomSeriesQueryResult* dcmtk_query_series_for_study(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* study_instance_uid);
DicomInstanceQueryResult* dcmtk_query_instances_for_series(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* series_instance_uid);

// Patient management functions
PatientCreationResult* dcmtk_create_patient(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, PatientInfo* patient_info);

// Media upload functions
MediaUploadResult* dcmtk_upload_image(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* patient_id, const char* image_path, const char* study_description, const char* series_description, const char* image_comments, const char* modality);
MediaUploadResult* dcmtk_upload_video(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* patient_id, const char* video_path, const char* study_description, const char* series_description, const char* image_comments, const char* modality);

// Media retrieval functions
DicomInstanceQueryResult* dcmtk_download_instances(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* series_instance_uid, const char* local_storage_path);

// Memory cleanup functions
void dcmtk_free_query_result(DicomQueryResult* result);
void dcmtk_free_study_query_result(DicomStudyQueryResult* result);
void dcmtk_free_series_query_result(DicomSeriesQueryResult* result);
void dcmtk_free_instance_query_result(DicomInstanceQueryResult* result);
void dcmtk_free_patient_creation_result(PatientCreationResult* result);
void dcmtk_free_media_upload_result(MediaUploadResult* result);

#ifdef __cplusplus
}
#endif

#endif // DCMTK_FLUTTER_WRAPPER_H
