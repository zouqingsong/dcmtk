#include "dcmtk_flutter_wrapper.h"
#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcxfer.h>
#include <dcmtk/dcmdata/dcpixel.h>
#include <dcmtk/dcmdata/dcpxitem.h>
#include <dcmtk/dcmimgle/dcmimage.h>
#include <string>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <cstdio>

// Simple debug logging that works in both C++ and Objective-C++
#define DEBUG_LOG(...) do { \
    fprintf(stderr, "[DCMTK] " __VA_ARGS__); \
    fprintf(stderr, "\n"); \
    printf("[DCMTK] " __VA_ARGS__); \
    printf("\n"); \
    fflush(stdout); \
} while(0)

extern "C" {

char* dcmtk_load_dicom_file(const char* filename) {
    if (!filename) {
        return strdup("Error: filename is null");
    }
    
    DcmFileFormat fileformat;
    OFCondition status = fileformat.loadFile(filename);
    
    if (status.bad()) {
        std::string error = "Error loading file: ";
        error += status.text();
        return strdup(error.c_str());
    }
    
    DcmDataset *dataset = fileformat.getDataset();
    if (!dataset) {
        return strdup("Error: could not get dataset");
    }
    
    std::ostringstream oss;
    
    // Get Patient Name
    OFString patientName;
    if (dataset->findAndGetOFString(DCM_PatientName, patientName).good()) {
        oss << "Patient Name: " << patientName << "\n";
    }
    
    // Get Patient ID
    OFString patientID;
    if (dataset->findAndGetOFString(DCM_PatientID, patientID).good()) {
        oss << "Patient ID: " << patientID << "\n";
    }
    
    // Get Study Description
    OFString studyDescription;
    if (dataset->findAndGetOFString(DCM_StudyDescription, studyDescription).good()) {
        oss << "Study Description: " << studyDescription << "\n";
    }
    
    // Get Modality
    OFString modality;
    if (dataset->findAndGetOFString(DCM_Modality, modality).good()) {
        oss << "Modality: " << modality << "\n";
    }
    
    // Get image dimensions
    Uint16 rows = 0, columns = 0;
    if (dataset->findAndGetUint16(DCM_Rows, rows).good()) {
        oss << "Rows: " << rows << "\n";
    }
    if (dataset->findAndGetUint16(DCM_Columns, columns).good()) {
        oss << "Columns: " << columns << "\n";
    }
    
    // Check for NumberOfFrames (multi-frame images)
    OFString numberOfFrames;
    if (dataset->findAndGetOFString(DCM_NumberOfFrames, numberOfFrames).good()) {
        oss << "NumberOfFrames: " << numberOfFrames << "\n";
    }
    
    // Check for pixel data
    if (dataset->tagExists(DCM_PixelData)) {
        oss << "PixelData: Present\n";
        
        // Get additional pixel information
        Uint16 bitsAllocated = 0, bitsStored = 0, highBit = 0, pixelRepresentation = 0, samplesPerPixel = 0;
        
        if (dataset->findAndGetUint16(DCM_BitsAllocated, bitsAllocated).good()) {
            oss << "BitsAllocated: " << bitsAllocated << "\n";
        }
        if (dataset->findAndGetUint16(DCM_BitsStored, bitsStored).good()) {
            oss << "BitsStored: " << bitsStored << "\n";
        }
        if (dataset->findAndGetUint16(DCM_HighBit, highBit).good()) {
            oss << "HighBit: " << highBit << "\n";
        }
        if (dataset->findAndGetUint16(DCM_PixelRepresentation, pixelRepresentation).good()) {
            oss << "PixelRepresentation: " << pixelRepresentation << "\n";
        }
        if (dataset->findAndGetUint16(DCM_SamplesPerPixel, samplesPerPixel).good()) {
            oss << "SamplesPerPixel: " << samplesPerPixel << "\n";
        }
        
        // Get Photometric Interpretation
        OFString photometricInterpretation;
        if (dataset->findAndGetOFString(DCM_PhotometricInterpretation, photometricInterpretation).good()) {
            oss << "PhotometricInterpretation: " << photometricInterpretation << "\n";
        }
    } else {
        oss << "PixelData: Not present\n";
    }
    
    return strdup(oss.str().c_str());
}

ImageExtractionResult* dcmtk_extract_image(const char* filename, int frame_index) {
    DEBUG_LOG("=== Starting image extraction ===");
    DEBUG_LOG("File: %s, Frame: %d", filename, frame_index);
    
    ImageExtractionResult* result = (ImageExtractionResult*)malloc(sizeof(ImageExtractionResult));
    result->data = nullptr;
    result->width = 0;
    result->height = 0;
    result->error = 0;
    result->error_message = nullptr;
    
    if (!filename) {
        DEBUG_LOG("ERROR: Filename is null");
        result->error = 1;
        result->error_message = strdup("Error: filename is null");
        return result;
    }
    
    // Load DICOM file
    DcmFileFormat fileformat;
    OFCondition status = fileformat.loadFile(filename);
    
    if (status.bad()) {
        DEBUG_LOG("ERROR: Failed to load file - %s", status.text());
        result->error = 1;
        std::string error = "Error loading file: ";
        error += status.text();
        result->error_message = strdup(error.c_str());
        return result;
    }
    
    DcmDataset *dataset = fileformat.getDataset();
    if (!dataset) {
        DEBUG_LOG("ERROR: Could not get dataset");
        result->error = 1;
        result->error_message = strdup("Error: could not get dataset");
        return result;
    }
    
    // Check if this is a multi-frame image
    OFString numFramesStr;
    bool isMultiFrame = dataset->findAndGetOFString(DCM_NumberOfFrames, numFramesStr).good();
    int totalFrames = 1;
    
    if (isMultiFrame) {
        totalFrames = atoi(numFramesStr.c_str());
        DEBUG_LOG("Multi-frame image detected: %d frames", totalFrames);
        
        if (frame_index >= totalFrames) {
            DEBUG_LOG("ERROR: Frame index %d out of range (total: %d)", frame_index, totalFrames);
            result->error = 1;
            result->error_message = strdup("Error: frame index out of range");
            return result;
        }
    }
    
    // Try multiple transfer syntaxes for image creation
    E_TransferSyntax transferSyntaxes[] = {
        EXS_Unknown,                    // Let DCMTK auto-detect
        EXS_LittleEndianExplicit,      
        EXS_LittleEndianImplicit,      // 1.2.840.10008.1.2
        EXS_BigEndianExplicit,
        EXS_DeflatedLittleEndianExplicit,
        EXS_JPEG8,
        EXS_JPEG12,
        EXS_JPEGLossless
    };
    
    DicomImage* image = nullptr;
    EIS_Status imgStatus = EIS_InvalidImage;
    E_TransferSyntax workingTransferSyntax = EXS_Unknown;
    
    // Try each transfer syntax
    for (int i = 0; i < 8; i++) {
        DEBUG_LOG("Trying transfer syntax %d", i);
        
        // For multi-frame images, try the basic constructor and handle frames differently
        if (isMultiFrame) {
            image = new DicomImage(dataset, transferSyntaxes[i]);
        } else {
            image = new DicomImage(dataset, transferSyntaxes[i]);
        }
        
        if (image) {
            imgStatus = image->getStatus();
            DEBUG_LOG("Image status: %d", (int)imgStatus);
            
            if (imgStatus == EIS_Normal) {
                workingTransferSyntax = transferSyntaxes[i];
                DEBUG_LOG("SUCCESS: Image created with transfer syntax %d", i);
                break;
            }
            
            delete image;
            image = nullptr;
        }
    }
    
    if (!image || imgStatus != EIS_Normal) {
        DEBUG_LOG("ERROR: Failed to create DicomImage with all transfer syntaxes");
        result->error = 1;
        
        std::string error = "Error: could not create DicomImage. Last status: ";
        error += std::to_string((int)imgStatus);
        result->error_message = strdup(error.c_str());
        
        if (image) {
            delete image;
        }
        return result;
    }
    
    // Get image dimensions
    result->width = image->getWidth();
    result->height = image->getHeight();
    
    DEBUG_LOG("Image dimensions - Width: %lu, Height: %lu", result->width, result->height);
    DEBUG_LOG("Frame count: %lu", image->getFrameCount());
    
    if (result->width == 0 || result->height == 0) {
        DEBUG_LOG("ERROR: Invalid dimensions detected");
        result->error = 1;
        result->error_message = strdup("Error: invalid image dimensions");
        delete image;
        return result;
    }
    
    // Allocate buffer for 8-bit grayscale data
    const unsigned long pixel_count = result->width * result->height;
    result->data = (unsigned char*)malloc(pixel_count);
    
    DEBUG_LOG("Allocated buffer for %lu pixels", pixel_count);
    
    if (!result->data) {
        DEBUG_LOG("ERROR: Memory allocation failed");
        result->error = 1;
        result->error_message = strdup("Error: memory allocation failed");
        delete image;
        return result;
    }
    
    DEBUG_LOG("Attempting to get pixel data for frame %d...", frame_index);
    
    // Get pixel data - for multi-frame, specify the frame index
    const void* pixels = nullptr;
    
    if (isMultiFrame && frame_index > 0) {
        // Try to get frame-specific data
        pixels = image->getOutputData(8, frame_index, 0);
        if (!pixels) {
            DEBUG_LOG("Frame-specific access failed, trying general access");
            pixels = image->getOutputData(8, 0, 0);
        }
    } else {
        pixels = image->getOutputData(8, 0, 0);
    }
    
    if (pixels == nullptr) {
        DEBUG_LOG("ERROR: Failed to get output data");
        result->error = 1;
        result->error_message = strdup("Error: could not extract pixel data");
        free(result->data);
        result->data = nullptr;
        delete image;
        return result;
    }
    
    DEBUG_LOG("Got pixel data, copying to buffer...");
    
    // Copy pixel data to our buffer
    memcpy(result->data, pixels, pixel_count);
    
    DEBUG_LOG("SUCCESS: Extracted image - %lux%lu pixels", result->width, result->height);
    
    delete image;
    return result;
}

void dcmtk_free_image_result(ImageExtractionResult* result) {
    if (result) {
        if (result->data) {
            free(result->data);
        }
        if (result->error_message) {
            free(result->error_message);
        }
        free(result);
    }
}

}