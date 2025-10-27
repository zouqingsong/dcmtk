#ifndef DCMTK_FLUTTER_WRAPPER_H
#define DCMTK_FLUTTER_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

// Function to load and parse a DICOM file
// Returns JSON string with file information or NULL on error
char* dcmtk_load_dicom_file(const char* file_path);

// Function to get specific DICOM tag value
char* dcmtk_get_dicom_tag(const char* file_path, const char* tag_name);

// Function to convert DICOM to image data
// Returns image data as bytes or NULL on error
unsigned char* dcmtk_convert_to_image(const char* file_path, const char* format, int* data_size);

// Function to free strings allocated by DCMTK wrapper
void dcmtk_free_string(char* str);

// Function to free image data allocated by DCMTK wrapper
void dcmtk_free_image_data(unsigned char* data);

// Function to get DCMTK version information
char* dcmtk_get_version();

// Function to validate DICOM file
int dcmtk_validate_dicom_file(const char* file_path);

#ifdef __cplusplus
}
#endif

#endif // DCMTK_FLUTTER_WRAPPER_H