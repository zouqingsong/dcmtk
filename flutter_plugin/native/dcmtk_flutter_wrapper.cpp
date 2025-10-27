#include "dcmtk_flutter_wrapper.h"
#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmimgle/dcmimage.h>
#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <cstdlib>

// JSON-like string builder helper
class SimpleJson {
private:
    std::ostringstream oss;
    bool first_item = true;

public:
    SimpleJson() { oss << "{"; }
    
    void addString(const char* key, const char* value) {
        if (!first_item) oss << ",";
        oss << "\"" << key << "\":\"" << value << "\"";
        first_item = false;
    }
    
    void addNumber(const char* key, long value) {
        if (!first_item) oss << ",";
        oss << "\"" << key << "\":" << value;
        first_item = false;
    }
    
    std::string toString() {
        oss << "}";
        return oss.str();
    }
};

extern "C" {

char* dcmtk_load_dicom_file(const char* file_path) {
    if (!file_path) {
        return nullptr;
    }

    try {
        DcmFileFormat fileformat;
        OFCondition status = fileformat.loadFile(file_path);
        
        if (status.bad()) {
            return nullptr;
        }

        DcmDataset* dataset = fileformat.getDataset();
        if (!dataset) {
            return nullptr;
        }

        SimpleJson json;
        json.addString("file_path", file_path);
        json.addString("status", "success");

        // Get some basic DICOM tags
        OFString patientName, studyDate, modality;
        if (dataset->findAndGetOFString(DCM_PatientName, patientName).good()) {
            json.addString("patient_name", patientName.c_str());
        }
        if (dataset->findAndGetOFString(DCM_StudyDate, studyDate).good()) {
            json.addString("study_date", studyDate.c_str());
        }
        if (dataset->findAndGetOFString(DCM_Modality, modality).good()) {
            json.addString("modality", modality.c_str());
        }

        // Get image dimensions if available
        Uint16 rows, cols;
        if (dataset->findAndGetUint16(DCM_Rows, rows).good()) {
            json.addNumber("rows", rows);
        }
        if (dataset->findAndGetUint16(DCM_Columns, cols).good()) {
            json.addNumber("columns", cols);
        }

        std::string result = json.toString();
        char* output = (char*)malloc(result.length() + 1);
        strcpy(output, result.c_str());
        return output;

    } catch (const std::exception& e) {
        return nullptr;
    }
}

char* dcmtk_get_dicom_tag(const char* file_path, const char* tag_name) {
    if (!file_path || !tag_name) {
        return nullptr;
    }

    try {
        DcmFileFormat fileformat;
        OFCondition status = fileformat.loadFile(file_path);
        
        if (status.bad()) {
            return nullptr;
        }

        DcmDataset* dataset = fileformat.getDataset();
        if (!dataset) {
            return nullptr;
        }

        // This is a simplified implementation
        // In a real implementation, you'd need to map tag names to DcmTagKey
        OFString value;
        if (strcmp(tag_name, "PatientName") == 0) {
            dataset->findAndGetOFString(DCM_PatientName, value);
        } else if (strcmp(tag_name, "StudyDate") == 0) {
            dataset->findAndGetOFString(DCM_StudyDate, value);
        } else if (strcmp(tag_name, "Modality") == 0) {
            dataset->findAndGetOFString(DCM_Modality, value);
        }

        if (value.length() > 0) {
            char* output = (char*)malloc(value.length() + 1);
            strcpy(output, value.c_str());
            return output;
        }

    } catch (const std::exception& e) {
        // Handle exception
    }

    return nullptr;
}

unsigned char* dcmtk_convert_to_image(const char* file_path, const char* format, int* data_size) {
    if (!file_path || !format || !data_size) {
        return nullptr;
    }

    try {
        DicomImage* image = new DicomImage(file_path);
        
        if (image == nullptr || image->getStatus() != EIS_Normal) {
            delete image;
            return nullptr;
        }

        // Get image properties
        unsigned long width = image->getWidth();
        unsigned long height = image->getHeight();
        
        // For simplicity, we'll return raw pixel data
        // In a real implementation, you'd convert to the requested format
        const void* pixelData = image->getOutputData(8); // 8-bit data
        
        if (pixelData) {
            *data_size = width * height;
            unsigned char* output = (unsigned char*)malloc(*data_size);
            memcpy(output, pixelData, *data_size);
            
            delete image;
            return output;
        }

        delete image;
    } catch (const std::exception& e) {
        // Handle exception
    }

    *data_size = 0;
    return nullptr;
}

void dcmtk_free_string(char* str) {
    if (str) {
        free(str);
    }
}

void dcmtk_free_image_data(unsigned char* data) {
    if (data) {
        free(data);
    }
}

char* dcmtk_get_version() {
    std::string version = "DCMTK Flutter Wrapper 1.0.0";
    char* output = (char*)malloc(version.length() + 1);
    strcpy(output, version.c_str());
    return output;
}

int dcmtk_validate_dicom_file(const char* file_path) {
    if (!file_path) {
        return 0;
    }

    try {
        DcmFileFormat fileformat;
        OFCondition status = fileformat.loadFile(file_path);
        return status.good() ? 1 : 0;
    } catch (const std::exception& e) {
        return 0;
    }
}

} // extern "C"