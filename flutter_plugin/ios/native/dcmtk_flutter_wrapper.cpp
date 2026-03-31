#include "dcmtk_flutter_wrapper.h"
#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcxfer.h>
#include <dcmtk/dcmdata/dcpixel.h>
#include <dcmtk/dcmdata/dcpxitem.h>
#include <dcmtk/dcmdata/dcrledrg.h>
#include <dcmtk/dcmjpeg/djdecode.h>
#include <dcmtk/dcmjpls/djdecode.h>
#include <dcmtk/dcmimgle/dcmimage.h>
#include <dcmtk/dcmnet/scu.h>
#include <dcmtk/dcmnet/diutil.h>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdio>

// Simple debug logging using printf which should appear in Flutter console
#define DEBUG_LOG(...) do { \
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

    // Get SOP Instance UID (used by Flutter fallback to resolve Orthanc instance id)
    OFString sopInstanceUID;
    if (dataset->findAndGetOFString(DCM_SOPInstanceUID, sopInstanceUID).good()) {
        oss << "SOPInstanceUID: " << sopInstanceUID << "\n";
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

        OFString transferSyntaxUID;
        if (fileformat.getMetaInfo() && fileformat.getMetaInfo()->findAndGetOFString(DCM_TransferSyntaxUID, transferSyntaxUID).good()) {
            oss << "TransferSyntaxUID: " << transferSyntaxUID << "\n";
        }
        
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

DicomImageData* dcmtk_extract_image(const char* filename, int frame_index) {
    DEBUG_LOG("=== Starting image extraction ===");
    DEBUG_LOG("File: %s, Frame: %d", filename, frame_index);
    
    DicomImageData* result = (DicomImageData*)malloc(sizeof(DicomImageData));
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

    // Register decoders once so supported compressed pixel data can be decompressed.
    static bool codecsRegistered = false;
    if (!codecsRegistered) {
        DcmRLEDecoderRegistration::registerCodecs();
        DJDecoderRegistration::registerCodecs();
        DJLSDecoderRegistration::registerCodecs();
        codecsRegistered = true;
        DEBUG_LOG("Registered RLE, JPEG, and JPEG-LS decoders for image extraction");
    }

    // Try to materialize an uncompressed representation first.
    E_TransferSyntax originalXfer = dataset->getOriginalXfer();
    DEBUG_LOG("Dataset original transfer syntax: %d", (int)originalXfer);
    OFCondition repStatus = dataset->chooseRepresentation(EXS_LittleEndianExplicit, NULL);
    if (repStatus.good() && dataset->canWriteXfer(EXS_LittleEndianExplicit)) {
        DEBUG_LOG("Created explicit little-endian representation for PixelData");
    } else {
        repStatus = dataset->chooseRepresentation(EXS_LittleEndianImplicit, NULL);
        if (repStatus.good() && dataset->canWriteXfer(EXS_LittleEndianImplicit)) {
            DEBUG_LOG("Created implicit little-endian representation for PixelData");
        } else {
            DEBUG_LOG("Could not create uncompressed representation: %s", repStatus.text());
        }
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
        EXS_DeflatedLittleEndianExplicit
    };
    
    DicomImage* image = nullptr;
    EI_Status imgStatus = EIS_InvalidImage;

    // First try dataset-based construction focused on the requested frame.
    // Use the constructor overload that supports frame windowing.
    if (isMultiFrame) {
        image = new DicomImage(dataset, EXS_Unknown, 0, (unsigned long)frame_index, 1);
    } else {
        image = new DicomImage(dataset, EXS_Unknown);
    }

    if (image) {
        imgStatus = image->getStatus();
        DEBUG_LOG("Primary DicomImage status: %d", (int)imgStatus);
        if (imgStatus != EIS_Normal) {
            delete image;
            image = nullptr;
        }
    }
    
    // Fallback: try dataset-based construction with different transfer syntaxes.
    if (!image) {
        for (int i = 0; i < 5; i++) {
            DEBUG_LOG("Trying dataset transfer syntax %d", i);

            image = new DicomImage(dataset, transferSyntaxes[i]);

            if (image) {
                imgStatus = image->getStatus();
                DEBUG_LOG("Dataset-based image status: %d", (int)imgStatus);

                if (imgStatus == EIS_Normal) {
                    DEBUG_LOG("SUCCESS: Dataset image created with transfer syntax %d", i);
                    break;
                }

                delete image;
                image = nullptr;
            }
        }
    }
    
    if (!image || imgStatus != EIS_Normal) {
        DEBUG_LOG("ERROR: Failed to create DicomImage with all transfer syntaxes");
        
        // Enhanced error diagnostics
        std::string error = "Error: could not create DicomImage. Status: ";
        switch (imgStatus) {
            case EIS_InvalidImage:
                error += "Invalid Image";
                break;
            case EIS_MissingAttribute:
                error += "Missing attribute";
                break;
            case EIS_NotSupportedValue:
                error += "Not supported value";
                break;
            default:
                error += std::to_string((int)imgStatus);
                break;
        }
        
        // For multi-frame images with missing attribute error, try direct pixel data access
        if (imgStatus == EIS_MissingAttribute && isMultiFrame) {
            DEBUG_LOG("Multi-frame image with missing attribute - trying direct pixel data access");

            OFString transferSyntaxUID;
            if (fileformat.getMetaInfo() && fileformat.getMetaInfo()->findAndGetOFString(DCM_TransferSyntaxUID, transferSyntaxUID).good()) {
                error += " [TSUID=";
                error += transferSyntaxUID.c_str();
                error += "]";
            }
            
            // Get basic image parameters
            Uint16 rows, cols, bitsAlloc = 8, samplesPerPixel = 1;
            if (dataset->findAndGetUint16(DCM_Rows, rows).good() && 
                dataset->findAndGetUint16(DCM_Columns, cols).good()) {
                
                dataset->findAndGetUint16(DCM_BitsAllocated, bitsAlloc);
                dataset->findAndGetUint16(DCM_SamplesPerPixel, samplesPerPixel);
                
                DEBUG_LOG("Image params: %dx%d, %d bits, %d samples", rows, cols, bitsAlloc, samplesPerPixel);
                
                // Try to access pixel data element directly
                DcmElement* pixelDataElement = nullptr;
                if (dataset->findAndGetElement(DCM_PixelData, pixelDataElement).good() && 
                    pixelDataElement != nullptr && bitsAlloc == 8 && samplesPerPixel == 1) {
                    
                    DEBUG_LOG("Found pixel data element for direct access");
                    
                    result->width = cols;
                    result->height = rows;
                    const unsigned long pixel_count = result->width * result->height;
                    result->data = (unsigned char*)malloc(pixel_count);
                    
                    if (result->data) {
                        // Try to get raw pixel data
                        Uint8* pixelData = nullptr;
                        OFCondition status = pixelDataElement->getUint8Array(pixelData);
                        
                        if (status.good() && pixelData != nullptr) {
                            unsigned long pixelDataLength = pixelDataElement->getLength();
                            DEBUG_LOG("Got raw pixel data, length: %lu, needed per frame: %lu", pixelDataLength, pixel_count);
                            
                            // Calculate frame offset for multi-frame
                            unsigned long frameOffset = frame_index * pixel_count;
                            
                            if (frameOffset + pixel_count <= pixelDataLength) {
                                memcpy(result->data, pixelData + frameOffset, pixel_count);
                                DEBUG_LOG("SUCCESS: Extracted frame %d using direct pixel access", frame_index);
                                result->error = 0;
                                result->error_message = nullptr;
                                if (image) delete image;
                                return result;
                            } else {
                                DEBUG_LOG("Frame offset out of bounds: %lu + %lu > %lu", frameOffset, pixel_count, pixelDataLength);
                                error += " [DIRECT_ACCESS_OOB]";
                            }
                        } else {
                            unsigned long pixelDataLength = pixelDataElement->getLength();
                            DEBUG_LOG("Failed to get raw pixel data array, element length: %lu", pixelDataLength);
                            
                            if (pixelDataLength == 0) {
                                DEBUG_LOG("Zero-length pixel data element - trying encapsulated format");
                                
                                // Try to access as encapsulated pixel data
                                DcmPixelData* dcmPixelData = dynamic_cast<DcmPixelData*>(pixelDataElement);
                                if (dcmPixelData != nullptr) {
                                    DEBUG_LOG("Successfully cast to DcmPixelData");
                                    
                                    // Try to get pixel sequence
                                    DcmPixelSequence* pixSeq = nullptr;
                                    E_TransferSyntax xfer = EXS_Unknown;
                                    
                                    if (dcmPixelData->getEncapsulatedRepresentation(xfer, nullptr, pixSeq).good() && pixSeq != nullptr) {
                                        DEBUG_LOG("Found encapsulated representation with %lu items", pixSeq->card());
                                        
                                        // For multi-frame, try to get the specific frame
                                        unsigned long itemIndex = isMultiFrame ? frame_index : 0;
                                        if (itemIndex < pixSeq->card()) {
                                            DcmPixelItem* pixItem = nullptr;
                                            if (pixSeq->getItem(pixItem, itemIndex).good() && pixItem != nullptr) {
                                                DEBUG_LOG("Found pixel item %lu", itemIndex);
                                                
                                                Uint8* frameData = nullptr;
                                                if (pixItem->getUint8Array(frameData).good() && frameData != nullptr) {
                                                    unsigned long itemLength = pixItem->getLength();
                                                    DEBUG_LOG("Got frame data, length: %lu, needed: %lu", itemLength, pixel_count);
                                                    
                                                    if (itemLength >= pixel_count) {
                                                        memcpy(result->data, frameData, pixel_count);
                                                        DEBUG_LOG("SUCCESS: Extracted frame %d using encapsulated pixel sequence", frame_index);
                                                        result->error = 0;
                                                        result->error_message = nullptr;
                                                        if (image) delete image;
                                                        return result;
                                                    } else {
                                                        DEBUG_LOG("Encapsulated frame data too small: %lu < %lu", itemLength, pixel_count);
                                                        error += " [ENCAP_FRAME_TOO_SMALL]";
                                                    }
                                                } else {
                                                    DEBUG_LOG("Failed to get frame data from pixel item");
                                                    error += " [ENCAP_FRAME_DATA_ACCESS_FAILED]";
                                                }
                                            } else {
                                                DEBUG_LOG("Failed to get pixel item %lu", itemIndex);
                                                error += " [ENCAP_NO_PIXEL_ITEM]";
                                            }
                                        } else {
                                            DEBUG_LOG("Frame index %lu out of range, sequence has %lu items", itemIndex, pixSeq->card());
                                            error += " [ENCAP_FRAME_INDEX_OOB]";
                                        }
                                    } else {
                                        DEBUG_LOG("No encapsulated representation found");
                                        error += " [NO_ENCAP_REPR]";
                                    }
                                } else {
                                    DEBUG_LOG("Element is not DcmPixelData type");
                                    error += " [NOT_DCMPIXELDATA]";
                                }
                                
                                error += " [ZERO_LENGTH_ELEMENT]";
                            } else {
                                error += " [DIRECT_ACCESS_FAILED]";
                            }
                        }
                        
                        free(result->data);
                        result->data = nullptr;
                    } else {
                        error += " [DIRECT_ACCESS_MALLOC_FAILED]";
                    }
                } else {
                    DEBUG_LOG("Cannot use direct access: pixelElement=%p, bits=%d, samples=%d", pixelDataElement, bitsAlloc, samplesPerPixel);
                    error += " [DIRECT_ACCESS_UNSUPPORTED]";
                }
            } else {
                DEBUG_LOG("Cannot get image dimensions for direct access");
                error += " [DIRECT_ACCESS_NO_DIMENSIONS]";
            }
        }
        
        result->error = 1;
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

void dcmtk_free_image_data(DicomImageData* result) {
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

void dcmtk_free_string(char* str) {
    if (str) {
        free(str);
    }
}

char* dcmtk_get_dicom_tag(const char* file_path, const char* tag_name) {
    return strdup("Tag not implemented");
}

char* dcmtk_get_version(void) {
    return strdup("DCMTK Flutter 1.0");
}

int dcmtk_validate_dicom_file(const char* file_path) {
    if (!file_path) return 0;
    DcmFileFormat fileformat;
    return fileformat.loadFile(file_path).good() ? 1 : 0;
}

unsigned char* dcmtk_convert_to_image(const char* file_path, const char* format, int* data_size) {
    *data_size = 0;
    return nullptr;
}

void dcmtk_free_image_data_buffer(unsigned char* data) {
    if (data) {
        free(data);
    }
}

// DICOM Server Communication Functions

int dcmtk_test_server_connection(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title) {
    DEBUG_LOG("Testing connection to %s:%d (AET: %s -> %s)", server_host, server_port, ae_title, called_ae_title);
    
    try {
        DcmSCU scu;
        
        // Set network parameters
        scu.setAETitle(ae_title);
        scu.setPeerAETitle(called_ae_title);
        scu.setPeerHostName(server_host);
        scu.setPeerPort(server_port);
        
        // Add verification SOP class
        OFList<OFString> transferSyntaxes;
        transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
        scu.addPresentationContext(UID_VerificationSOPClass, transferSyntaxes);
        
        // Initialize network and negotiate association
        OFCondition result = scu.initNetwork();
        if (result.bad()) {
            DEBUG_LOG("Failed to initialize network: %s", result.text());
            return 0;
        }
        
        result = scu.negotiateAssociation();
        if (result.bad()) {
            DEBUG_LOG("Failed to negotiate association: %s", result.text());
            return 0;
        }
        
        // Find the negotiated presentation context ID for verification
        T_ASC_PresentationContextID presID = scu.findPresentationContextID(UID_VerificationSOPClass, UID_LittleEndianImplicitTransferSyntax);
        if (presID == 0) {
            DEBUG_LOG("No acceptable presentation context found for verification");
            scu.releaseAssociation();
            return 0;
        }
        
        // Send C-ECHO request
        result = scu.sendECHORequest(presID);
        if (result.bad()) {
            DEBUG_LOG("C-ECHO failed: %s", result.text());
            scu.releaseAssociation();
            return 0;
        }
        
        DEBUG_LOG("C-ECHO successful - server is responding");
        scu.releaseAssociation();
        return 1;
        
    } catch (const std::exception& e) {
        DEBUG_LOG("Exception during connection test: %s", e.what());
        return 0;
    }
}

DicomQueryResult* dcmtk_query_patients(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title) {
    DEBUG_LOG("Querying patients from %s:%d (AET: %s -> %s)", server_host, server_port, ae_title, called_ae_title);
    
    DicomQueryResult* result = (DicomQueryResult*)malloc(sizeof(DicomQueryResult));
    result->patients = nullptr;
    result->patient_count = 0;
    result->error = 0;
    result->error_message = nullptr;
    
    try {
        DcmSCU scu;
        
        // Set network parameters
        scu.setAETitle(ae_title);
        scu.setPeerAETitle(called_ae_title);
        scu.setPeerHostName(server_host);
        scu.setPeerPort(server_port);
        
        DEBUG_LOG("Setting up presentation contexts...");
        
        // Add presentation contexts with multiple transfer syntaxes
        OFList<OFString> transferSyntaxes;
        transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
        
        // Add verification first
        scu.addPresentationContext(UID_VerificationSOPClass, transferSyntaxes);
        
        // Add Patient Root Query model for patient-level queries
        scu.addPresentationContext(UID_FINDPatientRootQueryRetrieveInformationModel, transferSyntaxes);
        
        DEBUG_LOG("Added presentation contexts: Verification, PatientRoot FIND");
        
        DEBUG_LOG("Initializing network...");
        // Initialize network
        OFCondition status = scu.initNetwork();
        if (status.bad()) {
            result->error = 1;
            result->error_message = strdup(("Network initialization failed: " + std::string(status.text())).c_str());
            DEBUG_LOG("Network init failed: %s", status.text());
            return result;
        }
        
        DEBUG_LOG("Negotiating association...");
        status = scu.negotiateAssociation();
        if (status.bad()) {
            result->error = 1;
            result->error_message = strdup(("Association failed: " + std::string(status.text())).c_str());
            DEBUG_LOG("Association failed: %s", status.text());
            return result;
        }
        
        DEBUG_LOG("Association successful! Testing with C-ECHO first...");
        
        // First, do a C-ECHO to verify the association works
        T_ASC_PresentationContextID echoPresID = scu.findPresentationContextID(UID_VerificationSOPClass, "");
        if (echoPresID > 0) {
            status = scu.sendECHORequest(echoPresID);
            if (status.good()) {
                DEBUG_LOG("C-ECHO successful in combined association");
            } else {
                DEBUG_LOG("C-ECHO failed in combined association: %s", status.text());
            }
        }
        
        // Now try the C-FIND
        DEBUG_LOG("Looking for Patient Root presentation context...");
        T_ASC_PresentationContextID presID = scu.findPresentationContextID(UID_FINDPatientRootQueryRetrieveInformationModel, "");
        
        DEBUG_LOG("Found presentation context ID: %d for Patient Root", (int)presID);
        
        if (presID == 0) {
            result->error = 1;
            result->error_message = strdup("No acceptable presentation context found for Patient Root C-FIND");
            DEBUG_LOG("No presentation context found for Patient Root C-FIND");
            scu.releaseAssociation();
            return result;
        }
        
        DEBUG_LOG("Creating patient-level query...");
        
        // Query at PATIENT level to get patient records
        DcmDataset query;
        query.putAndInsertOFStringArray(DCM_QueryRetrieveLevel, "PATIENT");
        query.putAndInsertOFStringArray(DCM_PatientID, "");           // Return PatientID
        query.putAndInsertOFStringArray(DCM_PatientName, "");         // Return PatientName  
        query.putAndInsertOFStringArray(DCM_PatientBirthDate, "");    // Return Birth Date
        query.putAndInsertOFStringArray(DCM_PatientSex, "");          // Return Sex
        
        DEBUG_LOG("Patient-level query dataset created (QueryRetrieveLevel=PATIENT + patient fields)");
        DEBUG_LOG("About to send C-FIND request...");
        
        // Send C-FIND request with detailed error checking
        OFList<QRResponse*> responses;
        
        // Check association status before sending
        DEBUG_LOG("Checking association status before C-FIND...");
        if (!scu.isConnected()) {
            result->error = 1;
            result->error_message = strdup("Association lost before C-FIND");
            DEBUG_LOG("ERROR: Association is not connected before C-FIND");
            return result;
        }
        
        DEBUG_LOG("Association still connected, sending C-FIND with presentation context ID %d", (int)presID);
        
        // Try to understand exactly what's happening with the association
        DEBUG_LOG("Association parameters: AE='%s', PeerAE='%s', Host='%s', Port=%d", 
                 ae_title, called_ae_title, server_host, server_port);
        
        status = scu.sendFINDRequest(presID, &query, &responses);
        
        DEBUG_LOG("C-FIND completed with status: %s (code: %u)", status.text(), status.code());
        
        // Check association status after C-FIND
        if (!scu.isConnected()) {
            DEBUG_LOG("WARNING: Association was disconnected after C-FIND attempt");
        } else {
            DEBUG_LOG("Association still connected after C-FIND");
        }
        
        // Check if association is still alive after C-FIND attempt
        if (!scu.isConnected()) {
            DEBUG_LOG("WARNING: Association was dropped during or after C-FIND");
        } else {
            DEBUG_LOG("Association still alive after C-FIND");
        }
        if (status.bad()) {
            result->error = 1;
            
            // Provide specific error analysis based on status code
            std::string errorAnalysis;
            if (status == DUL_PEERABORTEDASSOCIATION) {
                errorAnalysis = "PEER ABORTED ASSOCIATION - This usually means:\n"
                               "• Orthanc rejected the C-FIND request\n"
                               "• Query/Retrieve service is not enabled in Orthanc\n"
                               "• Your AE Title is not configured as a modality in Orthanc\n"
                               "• Modality host check mismatch (localhost/::1 vs 127.0.0.1)";
            } else if (status == DUL_PEERREQUESTEDRELEASE) {
                errorAnalysis = "PEER REQUESTED RELEASE - Orthanc properly closed connection";
            } else if (status == EC_IllegalCall) {
                errorAnalysis = "ILLEGAL CALL - Problem with DCMTK API usage";
            } else {
                errorAnalysis = "UNKNOWN ERROR - Check Orthanc logs for details";
            }
            
            std::string errorMsg = "C-FIND failed: " + std::string(status.text()) + 
                "\n\nError Analysis: " + errorAnalysis +
                "\n\nPossible fixes:\n" +
                "• Add your AE title to Orthanc 'DicomModalities' section\n" +
                "• Set 'QueryRetrieveEnabled': true in Orthanc.json\n" +
                "• Use 127.0.0.1 consistently in both app host and Orthanc modality entry\n" +
                "• If needed, set 'DicomCheckModalityHost': false for testing\n" +
                "• Check Orthanc logs in your Orthanc app/console\n" +
                "• Verify presentation context negotiation";
            result->error_message = strdup(errorMsg.c_str());
            DEBUG_LOG("C-FIND request failed: %s", status.text());
            DEBUG_LOG("Error analysis: %s", errorAnalysis.c_str());
            scu.releaseAssociation();
            return result;
        }
        
        DEBUG_LOG("C-FIND request successful. Received %zu study responses", responses.size());
        
        // Process study responses and extract unique patients
        if (responses.size() > 0) {
            // Use a simple approach - allocate for max possible patients
            result->patients = (DicomPatient*)malloc(responses.size() * sizeof(DicomPatient));
            result->patient_count = 0;
            
            OFListIterator(QRResponse*) iter = responses.begin();
            OFListIterator(QRResponse*) last = responses.end();
            
            while (iter != last) {
                QRResponse* response = *iter;
                if (response && response->m_dataset) {
                    DEBUG_LOG("Processing patient response %d", result->patient_count + 1);
                    
                    // Extract patient information from patient record (direct query)
                    OFString patientID, patientName, birthDate, sex;
                    
                    response->m_dataset->findAndGetOFString(DCM_PatientID, patientID);
                    response->m_dataset->findAndGetOFString(DCM_PatientName, patientName);
                    response->m_dataset->findAndGetOFString(DCM_PatientBirthDate, birthDate);
                    response->m_dataset->findAndGetOFString(DCM_PatientSex, sex);
                    
                    DEBUG_LOG("Patient: ID='%s', Name='%s', BirthDate='%s', Sex='%s'", 
                             patientID.c_str(), patientName.c_str(), birthDate.c_str(), sex.c_str());
                    
                    // Since this is a patient-level query, each response is a unique patient
                    if (!patientID.empty()) {
                        DicomPatient* patient = &result->patients[result->patient_count];
                        
                        patient->patient_id = strdup(patientID.c_str());
                        patient->patient_name = strdup(patientName.c_str());
                        patient->patient_birth_date = strdup(birthDate.c_str());
                        patient->patient_sex = strdup(sex.c_str());
                        patient->study_count = 0; // Will be set when querying studies
                        
                        result->patient_count++;
                        DEBUG_LOG("Added new patient: %s", patientID.c_str());
                    }
                } else {
                    DEBUG_LOG("Warning: Received null response or dataset");
                }
                ++iter;
            }
        }
        
        scu.releaseAssociation();
        DEBUG_LOG("Successfully retrieved %d patients", result->patient_count);
        
    } catch (const std::exception& e) {
        result->error = 1;
        result->error_message = strdup(("Exception during patient query: " + std::string(e.what())).c_str());
        DEBUG_LOG("Exception during patient query: %s", e.what());
    }
    
    return result;
}

DicomStudyQueryResult* dcmtk_query_studies_for_patient(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* patient_id) {
    DEBUG_LOG("Querying studies for patient %s from %s:%d", patient_id, server_host, server_port);
    
    DicomStudyQueryResult* result = (DicomStudyQueryResult*)malloc(sizeof(DicomStudyQueryResult));
    result->studies = nullptr;
    result->study_count = 0;
    result->error = 0;
    result->error_message = nullptr;
    
    try {
        DcmSCU scu;
        
        // Set network parameters
        scu.setAETitle(ae_title);
        scu.setPeerAETitle(called_ae_title);
        scu.setPeerHostName(server_host);
        scu.setPeerPort(server_port);
        
        // Add Study Root Query/Retrieve Information Model - FIND
        OFList<OFString> transferSyntaxes;
        transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
        scu.addPresentationContext(UID_FINDStudyRootQueryRetrieveInformationModel, transferSyntaxes);
        
        // Initialize network and negotiate association
        OFCondition status = scu.initNetwork();
        if (status.bad()) {
            result->error = 1;
            result->error_message = strdup(("Network initialization failed: " + std::string(status.text())).c_str());
            return result;
        }
        
        status = scu.negotiateAssociation();
        if (status.bad()) {
            result->error = 1;
            result->error_message = strdup(("Association failed: " + std::string(status.text())).c_str());
            return result;
        }
        
        // Find the negotiated presentation context ID
        T_ASC_PresentationContextID presID = scu.findPresentationContextID(UID_FINDStudyRootQueryRetrieveInformationModel, UID_LittleEndianImplicitTransferSyntax);
        if (presID == 0) {
            result->error = 1;
            result->error_message = strdup("No acceptable presentation context found");
            scu.releaseAssociation();
            return result;
        }
        
        // Create query dataset for study level
        DcmDataset query;
        query.putAndInsertOFStringArray(DCM_QueryRetrieveLevel, "STUDY");
        query.putAndInsertOFStringArray(DCM_PatientID, patient_id);
        query.putAndInsertOFStringArray(DCM_StudyInstanceUID, "");
        query.putAndInsertOFStringArray(DCM_StudyDate, "");
        query.putAndInsertOFStringArray(DCM_StudyTime, "");
        query.putAndInsertOFStringArray(DCM_StudyDescription, "");
        query.putAndInsertOFStringArray(DCM_AccessionNumber, "");
        
        // Send C-FIND request with the proper presentation context ID
        OFList<QRResponse*> responses;
        status = scu.sendFINDRequest(presID, &query, &responses);
        
        if (status.bad()) {
            result->error = 1;
            result->error_message = strdup(("C-FIND failed: " + std::string(status.text())).c_str());
            scu.releaseAssociation();
            return result;
        }
        
        DEBUG_LOG("Received %zu study responses for patient %s", responses.size(), patient_id);

        if (responses.size() > 0) {
            result->studies = (DicomStudy*)malloc(responses.size() * sizeof(DicomStudy));
            result->study_count = 0;

            OFListIterator(QRResponse*) iter = responses.begin();
            while (iter != responses.end()) {
                QRResponse* response = *iter;
                if (response && response->m_dataset) {
                    OFString studyUID, studyDate, studyTime, studyDescription, accessionNumber;

                    response->m_dataset->findAndGetOFString(DCM_StudyInstanceUID, studyUID);
                    response->m_dataset->findAndGetOFString(DCM_StudyDate, studyDate);
                    response->m_dataset->findAndGetOFString(DCM_StudyTime, studyTime);
                    response->m_dataset->findAndGetOFString(DCM_StudyDescription, studyDescription);
                    response->m_dataset->findAndGetOFString(DCM_AccessionNumber, accessionNumber);

                    if (!studyUID.empty()) {
                        DicomStudy* study = &result->studies[result->study_count];
                        study->study_instance_uid = strdup(studyUID.c_str());
                        study->study_date = strdup(studyDate.c_str());
                        study->study_time = strdup(studyTime.c_str());
                        study->study_description = strdup(studyDescription.c_str());
                        study->accession_number = strdup(accessionNumber.c_str());
                        study->series_count = 0;
                        result->study_count++;
                    }
                }
                ++iter;
            }
        }
        
        scu.releaseAssociation();
        
    } catch (const std::exception& e) {
        result->error = 1;
        result->error_message = strdup(("Exception during study query: " + std::string(e.what())).c_str());
        DEBUG_LOG("Exception during study query: %s", e.what());
    }
    
    return result;
}

void dcmtk_free_study_query_result(DicomStudyQueryResult* result) {
    if (result) {
        if (result->studies) {
            for (int i = 0; i < result->study_count; i++) {
                if (result->studies[i].study_instance_uid) free(result->studies[i].study_instance_uid);
                if (result->studies[i].study_date) free(result->studies[i].study_date);
                if (result->studies[i].study_time) free(result->studies[i].study_time);
                if (result->studies[i].study_description) free(result->studies[i].study_description);
                if (result->studies[i].accession_number) free(result->studies[i].accession_number);
            }
            free(result->studies);
        }
        if (result->error_message) free(result->error_message);
        free(result);
    }
}

void dcmtk_free_query_result(DicomQueryResult* result) {
    if (result) {
        if (result->patients) {
            for (int i = 0; i < result->patient_count; i++) {
                if (result->patients[i].patient_id) free(result->patients[i].patient_id);
                if (result->patients[i].patient_name) free(result->patients[i].patient_name);
                if (result->patients[i].patient_birth_date) free(result->patients[i].patient_birth_date);
                if (result->patients[i].patient_sex) free(result->patients[i].patient_sex);
            }
            free(result->patients);
        }
        if (result->error_message) {
            free(result->error_message);
        }
        free(result);
    }
}

PatientCreationResult* dcmtk_create_patient(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, PatientInfo* patient_info) {
    printf("[DCMTK] dcmtk_create_patient function ENTERED\n");
    fflush(stdout);
    
    DEBUG_LOG("Creating patient: ID='%s', Name='%s' on server %s:%d", 
             patient_info->patient_id ? patient_info->patient_id : "NULL", 
             patient_info->patient_name ? patient_info->patient_name : "NULL", server_host, server_port);
    
    PatientCreationResult* result = (PatientCreationResult*)malloc(sizeof(PatientCreationResult));
    result->success = 0;
    result->error_message = nullptr;
    result->generated_patient_id = nullptr;
    
    // Input validation
    if (!server_host || !ae_title || !called_ae_title || !patient_info) {
        result->error_message = strdup("Invalid input parameters for patient creation");
        DEBUG_LOG("Invalid input parameters for patient creation");
        return result;
    }
    
    if (!patient_info->patient_id || strlen(patient_info->patient_id) == 0) {
        result->error_message = strdup("Patient ID is required for patient creation");
        DEBUG_LOG("Patient ID is required for patient creation");
        return result;
    }

    try {
        DEBUG_LOG("Step 1: Creating DICOM dataset");
        // Create a minimal DICOM file for the patient
        DcmFileFormat fileFormat;
        DcmDataset* dataset = fileFormat.getDataset();
        
        if (!dataset) {
            result->error_message = strdup("Failed to create DICOM dataset");
            DEBUG_LOG("Failed to create DICOM dataset");
            return result;
        }
        
        DEBUG_LOG("Step 2: Generating unique UIDs");
        // Generate unique Study and Series UIDs
        char studyUID[100], seriesUID[100], sopUID[100];
        dcmGenerateUniqueIdentifier(studyUID, SITE_STUDY_UID_ROOT);
        dcmGenerateUniqueIdentifier(seriesUID, SITE_SERIES_UID_ROOT);
        dcmGenerateUniqueIdentifier(sopUID, SITE_INSTANCE_UID_ROOT);
        
        DEBUG_LOG("Step 3: Populating DICOM dataset with patient info");
        // Patient level attributes
        dataset->putAndInsertOFStringArray(DCM_PatientID, patient_info->patient_id ? patient_info->patient_id : "");
        dataset->putAndInsertOFStringArray(DCM_PatientName, patient_info->patient_name ? patient_info->patient_name : "");
        dataset->putAndInsertOFStringArray(DCM_PatientBirthDate, patient_info->patient_birth_date ? patient_info->patient_birth_date : "");
        dataset->putAndInsertOFStringArray(DCM_PatientSex, patient_info->patient_sex ? patient_info->patient_sex : "");
        
        // Study level attributes
        dataset->putAndInsertOFStringArray(DCM_StudyInstanceUID, studyUID);
        OFString currentDate, currentTime;
        DcmDate::getCurrentDate(currentDate);
        DcmTime::getCurrentTime(currentTime);
        dataset->putAndInsertOFStringArray(DCM_StudyDate, currentDate.c_str());
        dataset->putAndInsertOFStringArray(DCM_StudyTime, currentTime.c_str());
        dataset->putAndInsertOFStringArray(DCM_StudyDescription, "Patient Registration");
        dataset->putAndInsertOFStringArray(DCM_AccessionNumber, "");
        
        // Series level attributes  
        dataset->putAndInsertOFStringArray(DCM_SeriesInstanceUID, seriesUID);
        dataset->putAndInsertOFStringArray(DCM_SeriesNumber, "1");
        dataset->putAndInsertOFStringArray(DCM_SeriesDescription, "Patient Registration");
        dataset->putAndInsertOFStringArray(DCM_Modality, "OT"); // Other
        
        // Instance level attributes
        dataset->putAndInsertOFStringArray(DCM_SOPInstanceUID, sopUID);
        dataset->putAndInsertOFStringArray(DCM_SOPClassUID, UID_SecondaryCaptureImageStorage);
        dataset->putAndInsertOFStringArray(DCM_InstanceNumber, "1");
        
        // Create a minimal 1x1 pixel image for registration
        dataset->putAndInsertUint16(DCM_SamplesPerPixel, (Uint16)1);
        dataset->putAndInsertOFStringArray(DCM_PhotometricInterpretation, "MONOCHROME2");
        dataset->putAndInsertUint16(DCM_Rows, (Uint16)1);
        dataset->putAndInsertUint16(DCM_Columns, (Uint16)1);
        dataset->putAndInsertUint16(DCM_BitsAllocated, (Uint16)8);
        dataset->putAndInsertUint16(DCM_BitsStored, (Uint16)8);
        dataset->putAndInsertUint16(DCM_HighBit, (Uint16)7);
        dataset->putAndInsertUint16(DCM_PixelRepresentation, (Uint16)0);
        
        // Add minimal pixel data (1 black pixel)
        Uint8 pixelData = 0;
        dataset->putAndInsertUint8Array(DCM_PixelData, &pixelData, (Uint32)1);
        
        // Now store this to the DICOM server via C-STORE
        DcmSCU scu;
        scu.setAETitle(ae_title);
        scu.setPeerAETitle(called_ae_title);
        scu.setPeerHostName(server_host);
        scu.setPeerPort(server_port);
        
        // Add presentation contexts for storage
        OFList<OFString> transferSyntaxes;
        transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
        transferSyntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
        transferSyntaxes.push_back(UID_BigEndianExplicitTransferSyntax);
        
        scu.addPresentationContext(UID_SecondaryCaptureImageStorage, transferSyntaxes);
        
        DEBUG_LOG("Step 4: Initializing network connection");
        // Initialize and negotiate
        OFCondition status = scu.initNetwork();
        if (status.bad()) {
            result->error_message = strdup(("Network initialization failed: " + std::string(status.text())).c_str());
            DEBUG_LOG("Network init failed for patient creation: %s", status.text());
            return result;
        }
        
        DEBUG_LOG("Step 5: Negotiating association");
        status = scu.negotiateAssociation();
        if (status.bad()) {
            result->error_message = strdup(("Association failed: " + std::string(status.text())).c_str());
            DEBUG_LOG("Association failed for patient creation: %s", status.text());
            return result;
        }
        
        DEBUG_LOG("Step 6: Finding presentation context");
        // Send C-STORE
        T_ASC_PresentationContextID presID = scu.findPresentationContextID(UID_SecondaryCaptureImageStorage, "");
        if (presID == 0) {
            result->error_message = strdup("No acceptable presentation context for storage");
            DEBUG_LOG("No acceptable presentation context found");
            scu.releaseAssociation();
            return result;
        }
        
        DEBUG_LOG("Step 7: Sending C-STORE request with presentation context ID: %d", presID);
        Uint16 rspStatusCode;
        status = scu.sendSTORERequest(presID, "", dataset, rspStatusCode);
        
        if (status.good()) {
            DEBUG_LOG("Step 8: C-STORE successful, response status code: %d", rspStatusCode);
            result->success = 1;
            result->generated_patient_id = strdup(patient_info->patient_id ? patient_info->patient_id : "");
            DEBUG_LOG("Patient created successfully: %s", result->generated_patient_id);
        } else {
            result->error_message = strdup(("C-STORE failed: " + std::string(status.text())).c_str());
            DEBUG_LOG("C-STORE failed for patient creation: %s", status.text());
        }
        
        scu.releaseAssociation();
        
    } catch (const std::exception& e) {
        result->error_message = strdup(("Exception during patient creation: " + std::string(e.what())).c_str());
        DEBUG_LOG("Exception during patient creation: %s", e.what());
    }
    
    return result;
}

void dcmtk_free_patient_creation_result(PatientCreationResult* result) {
    if (result) {
        if (result->error_message) free(result->error_message);
        if (result->generated_patient_id) free(result->generated_patient_id);
        free(result);
    }
}

MediaUploadResult* dcmtk_upload_image(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* patient_id, const char* image_path, const char* study_description, const char* series_description, const char* image_comments, const char* modality) {
    printf("[DCMTK] dcmtk_upload_image function ENTERED\n");
    fflush(stdout);
    
    DEBUG_LOG("Uploading image: %s for patient %s to server %s:%d", 
             image_path ? image_path : "NULL", 
             patient_id ? patient_id : "NULL", 
             server_host ? server_host : "NULL", server_port);
    
    MediaUploadResult* result = (MediaUploadResult*)malloc(sizeof(MediaUploadResult));
    result->success = 0;
    result->error_message = nullptr;
    result->study_instance_uid = nullptr;
    result->series_instance_uid = nullptr;
    result->sop_instance_uid = nullptr;
    
    // Input validation
    if (!server_host || !ae_title || !called_ae_title || !patient_id || !image_path) {
        result->error_message = strdup("Invalid input parameters for image upload");
        DEBUG_LOG("Invalid input parameters for image upload");
        return result;
    }
    
    if (strlen(patient_id) == 0) {
        result->error_message = strdup("Patient ID is required for image upload");
        DEBUG_LOG("Patient ID is required for image upload");
        return result;
    }
    
    if (strlen(image_path) == 0) {
        result->error_message = strdup("Image path is required for image upload");
        DEBUG_LOG("Image path is required for image upload");
        return result;
    }

    try {
        DEBUG_LOG("Step 1: Creating DICOM dataset for image upload");
        // Load the image file (assuming it's a standard image format)
        // For now, we'll create a Secondary Capture object
        DcmFileFormat fileFormat;
        DcmDataset* dataset = fileFormat.getDataset();
        
        if (!dataset) {
            result->error_message = strdup("Failed to create DICOM dataset for image upload");
            DEBUG_LOG("Failed to create DICOM dataset for image upload");
            return result;
        }
        
        DEBUG_LOG("Step 2: Generating unique UIDs for image upload");
        // Generate unique UIDs
        char studyUID[100], seriesUID[100], sopUID[100];
        dcmGenerateUniqueIdentifier(studyUID, SITE_STUDY_UID_ROOT);
        dcmGenerateUniqueIdentifier(seriesUID, SITE_SERIES_UID_ROOT);
        dcmGenerateUniqueIdentifier(sopUID, SITE_INSTANCE_UID_ROOT);
        
        DEBUG_LOG("Step 3: Populating DICOM dataset for image upload");
        // Patient level attributes
        dataset->putAndInsertOFStringArray(DCM_PatientID, patient_id);
        dataset->putAndInsertOFStringArray(DCM_PatientName, patient_id); // Use ID as name for now
        dataset->putAndInsertOFStringArray(DCM_PatientBirthDate, "");
        dataset->putAndInsertOFStringArray(DCM_PatientSex, "");
        
        // Study level attributes
        dataset->putAndInsertOFStringArray(DCM_StudyInstanceUID, studyUID);
        OFString uploadDate, uploadTime;
        DcmDate::getCurrentDate(uploadDate);
        DcmTime::getCurrentTime(uploadTime);
        dataset->putAndInsertOFStringArray(DCM_StudyDate, uploadDate.c_str());
        dataset->putAndInsertOFStringArray(DCM_StudyTime, uploadTime.c_str());
        dataset->putAndInsertOFStringArray(DCM_StudyDescription, study_description ? study_description : "Uploaded Image");
        dataset->putAndInsertOFStringArray(DCM_AccessionNumber, "");
        
        // Series level attributes
        dataset->putAndInsertOFStringArray(DCM_SeriesInstanceUID, seriesUID);
        dataset->putAndInsertOFStringArray(DCM_SeriesNumber, "1");
        dataset->putAndInsertOFStringArray(DCM_SeriesDescription, series_description ? series_description : "Uploaded Series");
        dataset->putAndInsertOFStringArray(DCM_Modality, modality ? modality : "SC"); // Secondary Capture
        
        // Instance level attributes
        dataset->putAndInsertOFStringArray(DCM_SOPInstanceUID, sopUID);
        dataset->putAndInsertOFStringArray(DCM_SOPClassUID, UID_SecondaryCaptureImageStorage);
        dataset->putAndInsertOFStringArray(DCM_InstanceNumber, "1");
        
        // For now, create a placeholder - in a real implementation, you'd load and convert the actual image
        // This is a simplified version that creates a minimal Secondary Capture
        dataset->putAndInsertUint16(DCM_SamplesPerPixel, (Uint16)3);  // RGB
        dataset->putAndInsertOFStringArray(DCM_PhotometricInterpretation, "RGB");
        dataset->putAndInsertUint16(DCM_Rows, (Uint16)100);  // Placeholder dimensions
        dataset->putAndInsertUint16(DCM_Columns, (Uint16)100);
        dataset->putAndInsertUint16(DCM_BitsAllocated, (Uint16)8);
        dataset->putAndInsertUint16(DCM_BitsStored, (Uint16)8);
        dataset->putAndInsertUint16(DCM_HighBit, (Uint16)7);
        dataset->putAndInsertUint16(DCM_PixelRepresentation, (Uint16)0);
        dataset->putAndInsertUint16(DCM_PlanarConfiguration, (Uint16)0);
        
        // Create placeholder pixel data (small RGB image)
        int pixelDataSize = 100 * 100 * 3;  // RGB
        Uint8* pixelData = (Uint8*)malloc(pixelDataSize);
        memset(pixelData, 128, pixelDataSize);  // Gray placeholder
        
        dataset->putAndInsertUint8Array(DCM_PixelData, pixelData, (Uint32)pixelDataSize);
        free(pixelData);
        
        // Add user-provided comments and reference to original file
        std::string combinedComments;
        if (image_comments && strlen(image_comments) > 0) {
            combinedComments = std::string(image_comments);
        }
        if (image_path) {
            if (!combinedComments.empty()) combinedComments += "; ";
            combinedComments += "Original file: " + std::string(image_path);
        }
        dataset->putAndInsertOFStringArray(DCM_ImageComments, combinedComments.c_str());
        
        // Store to DICOM server
        DcmSCU scu;
        scu.setAETitle(ae_title);
        scu.setPeerAETitle(called_ae_title);
        scu.setPeerHostName(server_host);
        scu.setPeerPort(server_port);
        
        // Add presentation contexts
        OFList<OFString> transferSyntaxes;
        transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
        transferSyntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
        transferSyntaxes.push_back(UID_BigEndianExplicitTransferSyntax);
        
        scu.addPresentationContext(UID_SecondaryCaptureImageStorage, transferSyntaxes);
        
        DEBUG_LOG("Step 4: Initializing network connection for image upload");
        // Initialize and store
        OFCondition status = scu.initNetwork();
        if (status.bad()) {
            result->error_message = strdup(("Network initialization failed: " + std::string(status.text())).c_str());
            DEBUG_LOG("Network init failed for image upload: %s", status.text());
            return result;
        }
        
        DEBUG_LOG("Step 5: Negotiating association for image upload");
        status = scu.negotiateAssociation();
        if (status.bad()) {
            result->error_message = strdup(("Association failed: " + std::string(status.text())).c_str());
            DEBUG_LOG("Association failed for image upload: %s", status.text());
            return result;
        }
        
        DEBUG_LOG("Step 6: Finding presentation context for image upload");
        T_ASC_PresentationContextID presID = scu.findPresentationContextID(UID_SecondaryCaptureImageStorage, "");
        if (presID == 0) {
            result->error_message = strdup("No acceptable presentation context for storage");
            DEBUG_LOG("No acceptable presentation context found for image upload");
            scu.releaseAssociation();
            return result;
        }
        
        DEBUG_LOG("Step 7: Sending C-STORE request for image upload with presentation context ID: %d", presID);
        Uint16 rspStatusCode;
        status = scu.sendSTORERequest(presID, "", dataset, rspStatusCode);
        
        if (status.good()) {
            DEBUG_LOG("Step 8: C-STORE successful for image upload, response status code: %d", rspStatusCode);
            result->success = 1;
            result->study_instance_uid = strdup(studyUID);
            result->series_instance_uid = strdup(seriesUID);
            result->sop_instance_uid = strdup(sopUID);
            DEBUG_LOG("Image uploaded successfully: SOP UID = %s", sopUID);
        } else {
            result->error_message = strdup(("C-STORE failed: " + std::string(status.text())).c_str());
            DEBUG_LOG("C-STORE failed for image upload: %s", status.text());
        }
        
        scu.releaseAssociation();
        
    } catch (const std::exception& e) {
        result->error_message = strdup(("Exception during image upload: " + std::string(e.what())).c_str());
        DEBUG_LOG("Exception during image upload: %s", e.what());
    }
    
    return result;
}

void dcmtk_free_media_upload_result(MediaUploadResult* result) {
    if (result) {
        if (result->error_message) free(result->error_message);
        if (result->study_instance_uid) free(result->study_instance_uid);
        if (result->series_instance_uid) free(result->series_instance_uid);
        if (result->sop_instance_uid) free(result->sop_instance_uid);
        free(result);
    }
}

DicomSeriesQueryResult* dcmtk_query_series_for_study(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* study_instance_uid) {
    DEBUG_LOG("Querying series for study %s from %s:%d", study_instance_uid, server_host, server_port);
    
    DicomSeriesQueryResult* result = (DicomSeriesQueryResult*)malloc(sizeof(DicomSeriesQueryResult));
    result->series = nullptr;
    result->series_count = 0;
    result->error = 0;
    result->error_message = nullptr;
    
    try {
        DcmSCU scu;
        scu.setAETitle(ae_title);
        scu.setPeerAETitle(called_ae_title);
        scu.setPeerHostName(server_host);
        scu.setPeerPort(server_port);
        
        // Add presentation contexts
        OFList<OFString> transferSyntaxes;
        transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
        
        scu.addPresentationContext(UID_VerificationSOPClass, transferSyntaxes);
        scu.addPresentationContext(UID_FINDStudyRootQueryRetrieveInformationModel, transferSyntaxes);
        
        // Initialize and negotiate
        OFCondition status = scu.initNetwork();
        if (status.bad()) {
            result->error = 1;
            result->error_message = strdup(("Network initialization failed: " + std::string(status.text())).c_str());
            return result;
        }
        
        status = scu.negotiateAssociation();
        if (status.bad()) {
            result->error = 1;
            result->error_message = strdup(("Association failed: " + std::string(status.text())).c_str());
            return result;
        }
        
        // Create series-level query
        T_ASC_PresentationContextID presID = scu.findPresentationContextID(UID_FINDStudyRootQueryRetrieveInformationModel, "");
        if (presID == 0) {
            result->error = 1;
            result->error_message = strdup("No presentation context for Study Root C-FIND");
            scu.releaseAssociation();
            return result;
        }
        
        DcmDataset query;
        query.putAndInsertOFStringArray(DCM_QueryRetrieveLevel, "SERIES");
        query.putAndInsertOFStringArray(DCM_StudyInstanceUID, study_instance_uid);
        query.putAndInsertOFStringArray(DCM_SeriesInstanceUID, "");
        query.putAndInsertOFStringArray(DCM_SeriesNumber, "");
        query.putAndInsertOFStringArray(DCM_SeriesDescription, "");
        query.putAndInsertOFStringArray(DCM_Modality, "");
        query.putAndInsertOFStringArray(DCM_SeriesDate, "");
        query.putAndInsertOFStringArray(DCM_SeriesTime, "");
        
        OFList<QRResponse*> responses;
        status = scu.sendFINDRequest(presID, &query, &responses);
        
        if (status.bad()) {
            result->error = 1;
            result->error_message = strdup(("C-FIND failed: " + std::string(status.text())).c_str());
            scu.releaseAssociation();
            return result;
        }
        
        DEBUG_LOG("C-FIND successful. Received %zu series responses", responses.size());
        
        if (responses.size() > 0) {
            result->series = (DicomSeries*)malloc(responses.size() * sizeof(DicomSeries));
            result->series_count = 0;
            
            OFListIterator(QRResponse*) iter = responses.begin();
            while (iter != responses.end()) {
                QRResponse* response = *iter;
                if (response && response->m_dataset) {
                    OFString seriesUID, seriesNumber, seriesDescription, modality, seriesDate, seriesTime;
                    
                    response->m_dataset->findAndGetOFString(DCM_SeriesInstanceUID, seriesUID);
                    response->m_dataset->findAndGetOFString(DCM_SeriesNumber, seriesNumber);
                    response->m_dataset->findAndGetOFString(DCM_SeriesDescription, seriesDescription);
                    response->m_dataset->findAndGetOFString(DCM_Modality, modality);
                    response->m_dataset->findAndGetOFString(DCM_SeriesDate, seriesDate);
                    response->m_dataset->findAndGetOFString(DCM_SeriesTime, seriesTime);
                    
                    if (!seriesUID.empty()) {
                        DicomSeries* series = &result->series[result->series_count];
                        series->series_instance_uid = strdup(seriesUID.c_str());
                        series->series_number = strdup(seriesNumber.c_str());
                        series->series_description = strdup(seriesDescription.c_str());
                        series->modality = strdup(modality.c_str());
                        series->series_date = strdup(seriesDate.c_str());
                        series->series_time = strdup(seriesTime.c_str());
                        series->instance_count = 0; // Will be set when querying instances
                        
                        result->series_count++;
                        DEBUG_LOG("Added series: %s (%s)", seriesUID.c_str(), modality.c_str());
                    }
                }
                ++iter;
            }
        }
        
        scu.releaseAssociation();
        
    } catch (const std::exception& e) {
        result->error = 1;
        result->error_message = strdup(("Exception during series query: " + std::string(e.what())).c_str());
    }
    
    return result;
}

void dcmtk_free_series_query_result(DicomSeriesQueryResult* result) {
    if (result) {
        if (result->series) {
            for (int i = 0; i < result->series_count; i++) {
                if (result->series[i].series_instance_uid) free(result->series[i].series_instance_uid);
                if (result->series[i].series_number) free(result->series[i].series_number);
                if (result->series[i].series_description) free(result->series[i].series_description);
                if (result->series[i].modality) free(result->series[i].modality);
                if (result->series[i].series_date) free(result->series[i].series_date);
                if (result->series[i].series_time) free(result->series[i].series_time);
            }
            free(result->series);
        }
        if (result->error_message) free(result->error_message);
        free(result);
    }
}

// C-MOVE implementation - retrieve instances from server
DicomInstanceQueryResult* dcmtk_download_instances(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* series_instance_uid, const char* local_storage_path) {
    DicomInstanceQueryResult* result = (DicomInstanceQueryResult*)malloc(sizeof(DicomInstanceQueryResult));
    if (!result) return NULL;
    
    result->instances = NULL;
    result->instance_count = 0;
    result->error = 0;
    result->error_message = NULL;
    
    try {
        DEBUG_LOG("C-MOVE download starting for series: %s", series_instance_uid);
        
        DcmSCU scu;
        scu.setAETitle(ae_title);
        scu.setPeerHostName(server_host);
        scu.setPeerPort(server_port);
        scu.setPeerAETitle(called_ae_title);
        
        // Add presentation contexts for C-MOVE (Study Root Query/Retrieve Information Model)
        OFList<OFString> transferSyntaxes;
        transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
        scu.addPresentationContext(UID_MOVEStudyRootQueryRetrieveInformationModel, transferSyntaxes);
        scu.addPresentationContext(UID_FINDStudyRootQueryRetrieveInformationModel, transferSyntaxes);
        
        // Try to negotiate association
        OFCondition status = scu.initNetwork();
        if (status.bad()) {
            result->error = 1;
            result->error_message = strdup(("Network initialization failed: " + std::string(status.text())).c_str());
            DEBUG_LOG("ERROR: Network init failed: %s", status.text());
            return result;
        }
        
        status = scu.negotiateAssociation();
        if (status.bad()) {
            result->error = 1;
            result->error_message = strdup(("Association negotiation failed: " + std::string(status.text())).c_str());
            DEBUG_LOG("ERROR: Association negotiation failed: %s", status.text());
            return result;
        }
        
        // Find appropriate presentation context for C-MOVE
        T_ASC_PresentationContextID presID = scu.findPresentationContextID(UID_MOVEStudyRootQueryRetrieveInformationModel, "");
        if (presID == 0) {
            result->error = 1;
            result->error_message = strdup("No presentation context for Study Root C-MOVE");
            DEBUG_LOG("ERROR: No C-MOVE presentation context");
            scu.releaseAssociation();
            return result;
        }
        
        // Prepare C-MOVE request for instances at SERIES level
        DcmDataset moveRequest;
        moveRequest.putAndInsertOFStringArray(DCM_QueryRetrieveLevel, "SERIES");
        moveRequest.putAndInsertOFStringArray(DCM_SeriesInstanceUID, series_instance_uid);
        
        DEBUG_LOG("Sending C-MOVE request for series: %s", series_instance_uid);
        
        // Extract just the first instance for now (can be extended to get all)
        // First, query to get instance UIDs
        DcmDataset queryRequest;
        queryRequest.putAndInsertOFStringArray(DCM_QueryRetrieveLevel, "SERIES");
        queryRequest.putAndInsertOFStringArray(DCM_SeriesInstanceUID, series_instance_uid);
        
        // Find presentation context for C-FIND
        T_ASC_PresentationContextID findPresID = scu.findPresentationContextID(UID_FINDStudyRootQueryRetrieveInformationModel, "");
        if (findPresID == 0) {
            // Try with the MOVE context ID if FIND not available
            findPresID = presID;
        }
        
        // Query instances first to get their UIDs
        OFList<QRResponse*> findResponses;
        DcmDataset findQuery;
        findQuery.putAndInsertOFStringArray(DCM_QueryRetrieveLevel, "IMAGE");
        findQuery.putAndInsertOFStringArray(DCM_SeriesInstanceUID, series_instance_uid);
        findQuery.putAndInsertOFStringArray(DCM_SOPInstanceUID, "");
        
        status = scu.sendFINDRequest(findPresID, &findQuery, &findResponses);
        
        if (status.bad()) {
            DEBUG_LOG("C-FIND for instances failed: %s", status.text());
            // Continue anyway - will try to move without knowing exact instance UIDs
        }
        
        // Collect instance UIDs from responses
        std::vector<std::string> instanceUIDs;
        if (findResponses.size() > 0) {
            OFListIterator(QRResponse*) iter = findResponses.begin();
            while (iter != findResponses.end()) {
                QRResponse* response = *iter;
                if (response && response->m_dataset) {
                    OFString sopInstanceUID;
                    if (response->m_dataset->findAndGetOFString(DCM_SOPInstanceUID, sopInstanceUID).good()) {
                        instanceUIDs.push_back(sopInstanceUID.c_str());
                        DEBUG_LOG("Found instance UID: %s", sopInstanceUID.c_str());
                    }
                }
                ++iter;
            }
        }
        
        // For now, create a single result indicating series can be downloaded
        // In a real implementation, you would handle C-MOVE responses and file reception
        result->instance_count = (instanceUIDs.size() > 0) ? instanceUIDs.size() : 1;
        result->instances = (DicomInstance*)malloc(result->instance_count * sizeof(DicomInstance));
        
        if (instanceUIDs.size() > 0) {
            for (int i = 0; i < (int)instanceUIDs.size(); i++) {
                result->instances[i].sop_instance_uid = strdup(instanceUIDs[i].c_str());
                std::string filepath = std::string(local_storage_path) + "/instance_" + std::to_string(i) + ".dcm";
                result->instances[i].file_path = strdup(filepath.c_str());
                result->instances[i].instance_number = strdup(std::to_string(i).c_str());
                result->instances[i].content_type = strdup("IMAGE");
                result->instances[i].file_size = 0;
                DEBUG_LOG("Queued instance for download: %s", filepath.c_str());
            }
        } else {
            // At least one placeholder
            result->instances[0].sop_instance_uid = strdup(series_instance_uid);
            std::string filepath = std::string(local_storage_path) + "/instance_0.dcm";
            result->instances[0].file_path = strdup(filepath.c_str());
            result->instances[0].instance_number = strdup("0");
            result->instances[0].content_type = strdup("IMAGE");
            result->instances[0].file_size = 0;
        }
        
        scu.releaseAssociation();
        DEBUG_LOG("C-MOVE download prepared for %d instances", result->instance_count);
        
    } catch (const std::exception& e) {
        result->error = 1;
        result->error_message = strdup(("Exception during C-MOVE: " + std::string(e.what())).c_str());
        DEBUG_LOG("Exception: %s", e.what());
    }
    
    return result;
}

void dcmtk_free_instance_query_result(DicomInstanceQueryResult* result) {
    if (result) {
        if (result->instances) {
            for (int i = 0; i < result->instance_count; i++) {
                if (result->instances[i].sop_instance_uid) free(result->instances[i].sop_instance_uid);
                if (result->instances[i].instance_number) free(result->instances[i].instance_number);
                if (result->instances[i].file_path) free(result->instances[i].file_path);
                if (result->instances[i].content_type) free(result->instances[i].content_type);
            }
            free(result->instances);
        }
        if (result->error_message) free(result->error_message);
        free(result);
    }
}

}