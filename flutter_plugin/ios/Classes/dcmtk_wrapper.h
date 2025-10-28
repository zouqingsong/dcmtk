#ifndef DCMTK_WRAPPER_H
#define DCMTK_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

// C API for Flutter FFI
const char* dcmtk_load_dicom_file(const char* filename);
void dcmtk_free_string(const char* str);

#ifdef __cplusplus
}
#endif

#endif // DCMTK_WRAPPER_H
