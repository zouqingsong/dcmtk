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

DicomImageData* dcmtk_extract_image(const char* filename, int frame_index, double window_center, double window_width);
void dcmtk_free_image_data(DicomImageData* img_data);

// DICOM Server Communication
typedef struct {
    char* patient_id;
    char* patient_name;
    char* patient_birth_date;
    char* patient_sex;
    int study_count;
    char* number_of_patient_related_studies;
} DicomPatient;

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
    char* number_of_series_related_instances;
    char* body_part_examined;
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
    int rsp_status_code;
    char* warning_message;
} PatientCreationResult;

// Media upload result
typedef struct {
    int success;
    char* error_message;
    char* study_instance_uid;
    char* series_instance_uid;
    char* sop_instance_uid;
    int rsp_status_code;
} MediaUploadResult;

// Server connection and query functions
int dcmtk_test_server_connection(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title);
// TLS-enabled server connection test
int dcmtk_test_server_connection_tls(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title,
                                      const char* cert_file, const char* key_file, const char* ca_file);
int dcmtk_init_dictionary(const char* dictionary_path);
DicomQueryResult* dcmtk_query_patients(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* patient_name_filter);
DicomStudyQueryResult* dcmtk_query_studies_for_patient(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* patient_id);

// Extended query functions
DicomSeriesQueryResult* dcmtk_query_series_for_study(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* study_instance_uid);
DicomInstanceQueryResult* dcmtk_query_instances_for_series(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* series_instance_uid);

// Patient management functions
PatientCreationResult* dcmtk_create_patient(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, PatientInfo* patient_info);

// Media upload functions
MediaUploadResult* dcmtk_upload_image(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* patient_id, const char* image_path, const char* patient_name, const char* patient_birth_date, const char* study_description, const char* series_description, const char* image_comments, const char* modality, const char* study_instance_uid, const char* series_instance_uid, int instance_number);
MediaUploadResult* dcmtk_upload_multiframe(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* patient_id, const char** image_paths, int image_count, const char* patient_name, const char* patient_birth_date, const char* study_description, const char* series_description, const char* image_comments, const char* modality, const char* study_instance_uid, const char* series_instance_uid);
MediaUploadResult* dcmtk_upload_video(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* patient_id, const char* video_path, const char* patient_name, const char* patient_birth_date, const char* study_description, const char* series_description, const char* image_comments, const char* modality);

// Local Image2Dcm conversion (no network upload)
// Converts a JPEG/BMP image to a DICOM Secondary Capture file saved at output_path.
MediaUploadResult* dcmtk_convert_image_to_dicom(const char* image_path, const char* output_path,
    const char* patient_id, const char* patient_name, const char* patient_birth_date,
    const char* study_description, const char* series_description, const char* image_comments,
    const char* modality, const char* study_instance_uid, const char* series_instance_uid,
    int instance_number);

// Media retrieval functions
DicomInstanceQueryResult* dcmtk_download_instances(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* series_instance_uid, const char* local_storage_path);

// Video extraction - extracts encapsulated video payload from DICOM to a file
typedef struct {
    int success;
    char* error_message;
    char* output_path;      // Path to extracted video file
    char* mime_type;         // e.g. "video/mp4", "video/mpeg"
    long file_size;
} VideoExtractionResult;

VideoExtractionResult* dcmtk_extract_video(const char* dicom_path, const char* output_path);
void dcmtk_free_video_extraction_result(VideoExtractionResult* result);

// === C-STORE SCU: Send existing DICOM files to a remote PACS ===
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

// === C-STORE SCP: Receive DICOM files from remote peers ===
typedef struct {
    int running;    // 1 if SCP is running, 0 if stopped
    int port;
    int received_count;
    char* storage_dir;
    char* error_message;
} StoreSCPStatus;

// Start SCP listener on given port, storing received files in storage_dir
// Returns 1 on success, 0 on failure
int dcmtk_start_store_scp(int port, const char* ae_title, const char* storage_dir);
// Stop the running SCP listener
void dcmtk_stop_store_scp(void);
// Get current SCP status
StoreSCPStatus* dcmtk_get_store_scp_status(void);
void dcmtk_free_store_scp_status(StoreSCPStatus* status);

// === C-MOVE: Retrieve instances via C-MOVE ===
DicomInstanceQueryResult* dcmtk_move_instances(const char* server_host, int server_port,
                                                const char* ae_title, const char* called_ae_title,
                                                const char* series_instance_uid,
                                                const char* local_storage_path,
                                                int move_scp_port);

// Memory cleanup functions
void dcmtk_free_query_result(DicomQueryResult* result);
void dcmtk_free_study_query_result(DicomStudyQueryResult* result);
void dcmtk_free_series_query_result(DicomSeriesQueryResult* result);
void dcmtk_free_instance_query_result(DicomInstanceQueryResult* result);
void dcmtk_free_patient_creation_result(PatientCreationResult* result);
void dcmtk_free_media_upload_result(MediaUploadResult* result);

// === TLS Configuration API ===
// Set TLS cert/key/CA files. Once set, all subsequent SCU operations use TLS.
// Pass NULL or empty string for any file to skip it.
void dcmtk_set_tls_config(const char* cert_file, const char* key_file, const char* ca_file);
// Clear TLS configuration, reverting to plaintext connections.
void dcmtk_clear_tls_config(void);
// Returns 1 if OpenSSL is compiled in, 0 otherwise.
int dcmtk_is_tls_available(void);
// Returns 1 if TLS is currently enabled, 0 otherwise.
int dcmtk_is_tls_enabled(void);

// === Multi-Planar Reconstruction (MPR) ===
typedef struct {
    int volume_id;
    int width;              // in-plane columns
    int height;             // in-plane rows
    int depth;              // number of slices
    double pixel_spacing_x; // mm per pixel column
    double pixel_spacing_y; // mm per pixel row
    double slice_spacing;   // mm between slices
    double window_center;   // default WC from DICOM
    double window_width;    // default WW from DICOM
    int error;
    char* error_message;
} MprVolumeInfo;

typedef struct {
    unsigned char* data;    // RGBA pixel data (4 bytes per pixel)
    int width;
    int height;
    int error;
    char* error_message;
} MprSliceData;

// Build a 3D volume from a series of DICOM files (must share dimensions + have spatial metadata)
MprVolumeInfo* dcmtk_build_mpr_volume(const char** file_paths, int file_count);
// Extract an MPR slice: plane 0=axial, 1=sagittal, 2=coronal
MprSliceData* dcmtk_get_mpr_slice(int volume_id, int plane, int slice_index, double window_center, double window_width);
// Free a previously built volume
void dcmtk_free_mpr_volume(int volume_id);
void dcmtk_free_mpr_volume_info(MprVolumeInfo* info);
void dcmtk_free_mpr_slice_data(MprSliceData* data);

// === 3D Maximum Intensity Projection (MIP) ===
// Render a MIP image from a loaded volume at given rotation angles.
// rotation_x_deg/rotation_y_deg: rotation in degrees around X/Y axes.
// Returns MprSliceData with RGBA output. Caller must free with dcmtk_free_mpr_slice_data().
MprSliceData* dcmtk_render_mip(int volume_id, double rotation_x_deg, double rotation_y_deg,
                                double window_center, double window_width);

// === GSPS (Grayscale Softcopy Presentation State) ===
// Create a GSPS DICOM file from annotation data on a source DICOM image.
// annotations_json: JSON array of annotation objects (tool, points, text, color, strokeWidth).
// Returns MediaUploadResult with success/error and generated UIDs.
MediaUploadResult* dcmtk_create_gsps(const char* source_dicom_path,
                                      const char* annotations_json,
                                      const char* output_path);

// Parse a GSPS DICOM file and return annotations as a JSON string.
// Returns NULL if the file is not a GSPS or on error. Caller must free() the result.
char* dcmtk_parse_gsps(const char* gsps_file_path);

#ifdef __cplusplus
}
#endif

#endif // DCMTK_FLUTTER_WRAPPER_H
