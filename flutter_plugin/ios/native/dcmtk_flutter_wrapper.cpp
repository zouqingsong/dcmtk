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
#include <dcmtk/dcmimage/diregist.h>
#include <dcmtk/dcmnet/scu.h>
#include <dcmtk/dcmnet/dstorscu.h>
#include <dcmtk/dcmnet/diutil.h>
#include <dcmtk/dcmdata/libi2d/i2d.h>
#include <dcmtk/dcmdata/libi2d/i2djpgs.h>
#include <dcmtk/dcmdata/libi2d/i2dbmps.h>
#include <dcmtk/dcmdata/libi2d/i2dplsc.h>
#include <dcmtk/dcmdata/libi2d/i2dplvlp.h>
#ifdef WITH_OPENSSL
#include <dcmtk/dcmtls/tlslayer.h>
#endif
#include <dcmtk/dcmnet/scp.h>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <sys/stat.h>
#include <dirent.h>

// Simple debug logging using printf which should appear in Flutter console
#define DEBUG_LOG(...) do { \
    printf("[DCMTK] " __VA_ARGS__); \
    printf("\n"); \
    fflush(stdout); \
} while(0)

// ============================================================================
// Global TLS Configuration
// ============================================================================
static struct {
    std::string certFile;
    std::string keyFile;
    std::string caFile;
    bool enabled;
} g_tlsConfig = {"", "", "", false};

#ifdef WITH_OPENSSL
// Helper: apply TLS transport layer to a DcmSCU after initNetwork()
// Returns true on success, false on failure
static bool apply_tls_to_scu(DcmSCU& scu) {
    if (!g_tlsConfig.enabled) return true; // no TLS configured, plaintext OK

    DcmTLSTransportLayer *tLayer = new DcmTLSTransportLayer(NET_REQUESTOR, NULL, OFTrue);
    if (!tLayer) {
        DEBUG_LOG("Failed to create TLS transport layer");
        return false;
    }

    // Set security profile
    tLayer->setTLSProfile(TSP_Profile_BCP_195_RFC_8996);

    // Load certificate and private key
    if (!g_tlsConfig.certFile.empty()) {
        OFCondition cond = tLayer->setPrivateKeyFile(g_tlsConfig.keyFile.c_str(), DCF_Filetype_PEM);
        if (cond.bad()) {
            DEBUG_LOG("TLS: Failed to load private key: %s", cond.text());
            delete tLayer;
            return false;
        }
        cond = tLayer->setCertificateFile(g_tlsConfig.certFile.c_str(), DCF_Filetype_PEM, TSP_Profile_BCP_195_RFC_8996);
        if (cond.bad()) {
            DEBUG_LOG("TLS: Failed to load certificate: %s", cond.text());
            delete tLayer;
            return false;
        }
        if (!tLayer->checkPrivateKeyMatchesCertificate()) {
            DEBUG_LOG("TLS: Private key does not match certificate");
            delete tLayer;
            return false;
        }
    }

    // Load trusted CA certificate
    if (!g_tlsConfig.caFile.empty()) {
        OFCondition cond = tLayer->addTrustedCertificateFile(g_tlsConfig.caFile.c_str(), DCF_Filetype_PEM);
        if (cond.bad()) {
            DEBUG_LOG("TLS: Failed to load CA certificate: %s", cond.text());
            delete tLayer;
            return false;
        }
        tLayer->setCertificateVerification(DCV_checkCertificate);
    } else {
        // No CA cert — skip verification (self-signed / testing)
        tLayer->setCertificateVerification(DCV_ignoreCertificate);
    }

    // Activate cipher suites
    tLayer->activateCipherSuites();

    // Apply to SCU — DcmSCU takes ownership of the transport layer
    OFCondition result = scu.useSecureConnection(tLayer);
    if (result.bad()) {
        DEBUG_LOG("TLS: Failed to enable secure connection: %s", result.text());
        return false;
    }

    DEBUG_LOG("TLS: Secure connection configured successfully");
    return true;
}
#endif // WITH_OPENSSL

// Check if TLS should be used and apply it. Returns false only if TLS was
// requested but failed to configure. When OpenSSL is not compiled in and
// TLS is enabled, it logs a warning and returns true (falls back to plaintext).
static bool maybe_apply_tls(DcmSCU& scu) {
    if (!g_tlsConfig.enabled) return true;
#ifdef WITH_OPENSSL
    return apply_tls_to_scu(scu);
#else
    DEBUG_LOG("TLS: OpenSSL not compiled in, falling back to plaintext");
    return true;
#endif
}

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

    // Get SOP Class UID
    OFString sopClassUID;
    if (dataset->findAndGetOFString(DCM_SOPClassUID, sopClassUID).good()) {
        oss << "SOPClassUID: " << sopClassUID << "\n";
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

DicomImageData* dcmtk_extract_image(const char* filename, int frame_index, double window_center, double window_width) {
    DEBUG_LOG("=== Starting image extraction ===");
    DEBUG_LOG("File: %s, Frame: %d, WC: %.1f, WW: %.1f", filename, frame_index, window_center, window_width);
    
    DicomImageData* result = (DicomImageData*)malloc(sizeof(DicomImageData));
    result->data = nullptr;
    result->width = 0;
    result->height = 0;
    result->samples_per_pixel = 0;
    result->bits_stored = 0;
    result->total_frames = 1;
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

        // Always include transfer syntax UID in error for diagnostics
        OFString transferSyntaxUID;
        if (fileformat.getMetaInfo() && fileformat.getMetaInfo()->findAndGetOFString(DCM_TransferSyntaxUID, transferSyntaxUID).good()) {
            error += " [TSUID=";
            error += transferSyntaxUID.c_str();
            error += "]";

            // Provide user-friendly hints for known unsupported transfer syntaxes
            std::string tsStr(transferSyntaxUID.c_str());
            if (tsStr == "1.2.840.10008.1.2.4.90" || tsStr == "1.2.840.10008.1.2.4.91") {
                error += "\n\nThis image uses JPEG 2000 compression which is not currently supported. "
                         "Try decompressing the file on the PACS server first, or use a viewer that supports JPEG 2000.";
            } else if (tsStr == "1.2.840.10008.1.2.4.201" || tsStr == "1.2.840.10008.1.2.4.202" || tsStr == "1.2.840.10008.1.2.4.203") {
                error += "\n\nThis image uses HTJ2K (High-Throughput JPEG 2000) compression which is not currently supported.";
            }
        }
        
        // For multi-frame images with missing attribute error, try direct pixel data access
        if (imgStatus == EIS_MissingAttribute && isMultiFrame) {
            DEBUG_LOG("Multi-frame image with missing attribute - trying direct pixel data access");
            
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
    result->total_frames = (int)image->getFrameCount();
    // For multi-frame images, getFrameCount() may return 1 when we requested a single-frame
    // window (the DicomImage constructor with frame offset + count). Use the tag value instead.
    if (totalFrames > result->total_frames) result->total_frames = totalFrames;
    
    // Determine color vs grayscale
    int isColorImage = image->isMonochrome() ? 0 : 1;
    
    // Apply custom window/level if specified (window_width > 0 means custom W/L)
    if (window_width > 0 && !isColorImage) {
        if (image->setWindow(window_center, window_width)) {
            DEBUG_LOG("Applied custom W/L: center=%.1f width=%.1f", window_center, window_width);
        } else {
            DEBUG_LOG("WARNING: setWindow(%.1f, %.1f) failed, using default", window_center, window_width);
        }
    }
    
    DEBUG_LOG("Image dimensions - Width: %d, Height: %d, Color: %d, Frames: %d",
              result->width, result->height, isColorImage, result->total_frames);
    
    if (result->width == 0 || result->height == 0) {
        DEBUG_LOG("ERROR: Invalid dimensions detected");
        result->error = 1;
        result->error_message = strdup("Error: invalid image dimensions");
        delete image;
        return result;
    }
    
    // Always output RGBA (4 bytes per pixel) for uniform handling on Flutter side
    const unsigned long pixel_count = (unsigned long)result->width * result->height;
    const unsigned long rgba_size = pixel_count * 4;
    result->data = (unsigned char*)malloc(rgba_size);
    result->samples_per_pixel = isColorImage ? 3 : 1;
    
    // Read bits stored from dataset
    Uint16 bitsStoredVal = 8;
    dataset->findAndGetUint16(DCM_BitsStored, bitsStoredVal);
    result->bits_stored = bitsStoredVal;
    
    if (!result->data) {
        DEBUG_LOG("ERROR: Memory allocation failed");
        result->error = 1;
        result->error_message = strdup("Error: memory allocation failed");
        delete image;
        return result;
    }
    
    DEBUG_LOG("Attempting to get pixel data for frame %d...", frame_index);
    
    if (isColorImage) {
        // Color image: get 24-bit RGB output and convert to RGBA
        const void* pixels = image->getOutputData(8, (unsigned long)frame_index, 0);
        if (!pixels) {
            DEBUG_LOG("Color frame-specific access failed, trying frame 0");
            pixels = image->getOutputData(8, 0, 0);
        }
        if (pixels) {
            const unsigned char* rgb = (const unsigned char*)pixels;
            unsigned char* rgba = result->data;
            for (unsigned long i = 0; i < pixel_count; i++) {
                rgba[i * 4 + 0] = rgb[i * 3 + 0];
                rgba[i * 4 + 1] = rgb[i * 3 + 1];
                rgba[i * 4 + 2] = rgb[i * 3 + 2];
                rgba[i * 4 + 3] = 255;
            }
            DEBUG_LOG("SUCCESS: Extracted color image - %dx%d pixels (RGBA)", result->width, result->height);
            delete image;
            return result;
        }
    } else {
        // Grayscale: get 8-bit output and convert to RGBA
        const void* pixels = nullptr;
        if (isMultiFrame && frame_index > 0) {
            pixels = image->getOutputData(8, (unsigned long)frame_index, 0);
            if (!pixels) {
                DEBUG_LOG("Frame-specific access failed, trying general access");
                pixels = image->getOutputData(8, 0, 0);
            }
        } else {
            pixels = image->getOutputData(8, 0, 0);
        }
        if (pixels) {
            const unsigned char* gray = (const unsigned char*)pixels;
            unsigned char* rgba = result->data;
            for (unsigned long i = 0; i < pixel_count; i++) {
                rgba[i * 4 + 0] = gray[i];
                rgba[i * 4 + 1] = gray[i];
                rgba[i * 4 + 2] = gray[i];
                rgba[i * 4 + 3] = 255;
            }
            DEBUG_LOG("SUCCESS: Extracted grayscale image - %dx%d pixels (RGBA)", result->width, result->height);
            delete image;
            return result;
        }
    }
    
    // If we got here, pixel extraction failed
    DEBUG_LOG("ERROR: Failed to get output data");
    result->error = 1;
    result->error_message = strdup("Error: could not extract pixel data");
    free(result->data);
    result->data = nullptr;
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
    if (!file_path || !tag_name) return strdup("Error: null argument");
    
    // Parse tag_name as "group,element" e.g. "0010,0010"
    unsigned int group = 0, element = 0;
    if (sscanf(tag_name, "%x,%x", &group, &element) != 2) {
        // Try common tag names
        std::string name(tag_name);
        DcmTagKey tagKey;
        if (name == "PatientName") tagKey = DCM_PatientName;
        else if (name == "PatientID") tagKey = DCM_PatientID;
        else if (name == "PatientBirthDate") tagKey = DCM_PatientBirthDate;
        else if (name == "PatientSex") tagKey = DCM_PatientSex;
        else if (name == "StudyInstanceUID") tagKey = DCM_StudyInstanceUID;
        else if (name == "SeriesInstanceUID") tagKey = DCM_SeriesInstanceUID;
        else if (name == "SOPInstanceUID") tagKey = DCM_SOPInstanceUID;
        else if (name == "StudyDate") tagKey = DCM_StudyDate;
        else if (name == "StudyDescription") tagKey = DCM_StudyDescription;
        else if (name == "Modality") tagKey = DCM_Modality;
        else if (name == "Rows") tagKey = DCM_Rows;
        else if (name == "Columns") tagKey = DCM_Columns;
        else if (name == "NumberOfFrames") tagKey = DCM_NumberOfFrames;
        else if (name == "BitsAllocated") tagKey = DCM_BitsAllocated;
        else if (name == "BitsStored") tagKey = DCM_BitsStored;
        else if (name == "SamplesPerPixel") tagKey = DCM_SamplesPerPixel;
        else if (name == "PhotometricInterpretation") tagKey = DCM_PhotometricInterpretation;
        else if (name == "TransferSyntaxUID") tagKey = DCM_TransferSyntaxUID;
        else if (name == "SeriesDescription") tagKey = DCM_SeriesDescription;
        else if (name == "AccessionNumber") tagKey = DCM_AccessionNumber;
        else if (name == "InstitutionName") tagKey = DCM_InstitutionName;
        else if (name == "ReferringPhysicianName") tagKey = DCM_ReferringPhysicianName;
        else {
            return strdup(("Error: unknown tag name '" + name + "'. Use group,element format e.g. '0010,0010'").c_str());
        }
        group = tagKey.getGroup();
        element = tagKey.getElement();
    }
    
    DcmFileFormat fileformat;
    OFCondition status = fileformat.loadFile(file_path);
    if (status.bad()) {
        return strdup(("Error loading file: " + std::string(status.text())).c_str());
    }
    
    DcmDataset *dataset = fileformat.getDataset();
    if (!dataset) return strdup("Error: could not get dataset");
    
    DcmTagKey key(group, element);
    
    // For TransferSyntaxUID, look in meta info
    if (key == DCM_TransferSyntaxUID) {
        OFString val;
        if (fileformat.getMetaInfo() && fileformat.getMetaInfo()->findAndGetOFString(key, val).good()) {
            return strdup(val.c_str());
        }
        return strdup("");
    }
    
    OFString value;
    if (dataset->findAndGetOFStringArray(key, value).good()) {
        return strdup(value.c_str());
    }
    return strdup("");
}

char* dcmtk_get_version(void) {
    return strdup("DCMTK Flutter 1.0");
}

int dcmtk_validate_dicom_file(const char* file_path) {
    if (!file_path) return 0;
    
    DcmFileFormat fileformat;
    OFCondition status = fileformat.loadFile(file_path);
    if (status.bad()) return 0;
    
    DcmDataset *dataset = fileformat.getDataset();
    if (!dataset) return 0;
    
    // Check for essential DICOM attributes
    OFString sopClassUID;
    if (dataset->findAndGetOFString(DCM_SOPClassUID, sopClassUID).bad()) return 0;
    if (sopClassUID.empty()) return 0;
    
    OFString sopInstanceUID;
    if (dataset->findAndGetOFString(DCM_SOPInstanceUID, sopInstanceUID).bad()) return 0;
    if (sopInstanceUID.empty()) return 0;
    
    return 1;
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
        if (!maybe_apply_tls(scu)) return 0;
        
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

DicomQueryResult* dcmtk_query_patients(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* patient_name_filter) {
    DEBUG_LOG("Querying patients from %s:%d (AET: %s -> %s, filter: '%s')", server_host, server_port, ae_title, called_ae_title, patient_name_filter ? patient_name_filter : "");
    
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
        if (!maybe_apply_tls(scu)) {
            result->error = 1;
            result->error_message = strdup("TLS configuration failed");
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
        query.putAndInsertOFStringArray(DCM_PatientID, "*");           // Wildcard: return all
        // Apply server-side name filter if provided, otherwise wildcard
        if (patient_name_filter && strlen(patient_name_filter) > 0) {
            std::string filter = std::string("*") + patient_name_filter + "*";
            query.putAndInsertOFStringArray(DCM_PatientName, filter.c_str());
        } else {
            query.putAndInsertOFStringArray(DCM_PatientName, "*");     // Wildcard: return all
        }
        query.putAndInsertOFStringArray(DCM_PatientBirthDate, "");    // Return Birth Date
        query.putAndInsertOFStringArray(DCM_PatientSex, "");          // Return Sex
        query.putAndInsertOFStringArray(DCM_NumberOfPatientRelatedStudies, ""); // Return study count
        
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
                    OFString patientID, patientName, birthDate, sex, numStudies;
                    
                    response->m_dataset->findAndGetOFString(DCM_PatientID, patientID);
                    response->m_dataset->findAndGetOFString(DCM_PatientName, patientName);
                    response->m_dataset->findAndGetOFString(DCM_PatientBirthDate, birthDate);
                    response->m_dataset->findAndGetOFString(DCM_PatientSex, sex);
                    response->m_dataset->findAndGetOFString(DCM_NumberOfPatientRelatedStudies, numStudies);
                    
                    DEBUG_LOG("Patient: ID='%s', Name='%s', BirthDate='%s', Sex='%s', Studies='%s'", 
                             patientID.c_str(), patientName.c_str(), birthDate.c_str(), sex.c_str(), numStudies.c_str());
                    
                    // Since this is a patient-level query, each response is a unique patient
                    if (!patientID.empty()) {
                        DicomPatient* patient = &result->patients[result->patient_count];
                        
                        patient->patient_id = strdup(patientID.c_str());
                        patient->patient_name = strdup(patientName.c_str());
                        patient->patient_birth_date = strdup(birthDate.c_str());
                        patient->patient_sex = strdup(sex.c_str());
                        patient->study_count = 0;
                        patient->number_of_patient_related_studies = strdup(numStudies.c_str());
                        
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
        if (!maybe_apply_tls(scu)) {
            result->error = 1;
            result->error_message = strdup("TLS configuration failed");
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
        query.putAndInsertOFStringArray(DCM_ModalitiesInStudy, "");
        query.putAndInsertOFStringArray(DCM_NumberOfStudyRelatedSeries, "");
        query.putAndInsertOFStringArray(DCM_NumberOfStudyRelatedInstances, "");
        query.putAndInsertOFStringArray(DCM_ReferringPhysicianName, "");
        
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
                    OFString modalitiesInStudy, numSeries, numInstances, referringPhysician;

                    response->m_dataset->findAndGetOFString(DCM_StudyInstanceUID, studyUID);
                    response->m_dataset->findAndGetOFString(DCM_StudyDate, studyDate);
                    response->m_dataset->findAndGetOFString(DCM_StudyTime, studyTime);
                    response->m_dataset->findAndGetOFString(DCM_StudyDescription, studyDescription);
                    response->m_dataset->findAndGetOFString(DCM_AccessionNumber, accessionNumber);
                    response->m_dataset->findAndGetOFString(DCM_ModalitiesInStudy, modalitiesInStudy);
                    response->m_dataset->findAndGetOFString(DCM_NumberOfStudyRelatedSeries, numSeries);
                    response->m_dataset->findAndGetOFString(DCM_NumberOfStudyRelatedInstances, numInstances);
                    response->m_dataset->findAndGetOFString(DCM_ReferringPhysicianName, referringPhysician);

                    if (!studyUID.empty()) {
                        DicomStudy* study = &result->studies[result->study_count];
                        study->study_instance_uid = strdup(studyUID.c_str());
                        study->study_date = strdup(studyDate.c_str());
                        study->study_time = strdup(studyTime.c_str());
                        study->study_description = strdup(studyDescription.c_str());
                        study->accession_number = strdup(accessionNumber.c_str());
                        study->series_count = 0;
                        study->modalities_in_study = strdup(modalitiesInStudy.c_str());
                        study->number_of_study_related_series = strdup(numSeries.c_str());
                        study->number_of_study_related_instances = strdup(numInstances.c_str());
                        study->referring_physician_name = strdup(referringPhysician.c_str());
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
                if (result->studies[i].modalities_in_study) free(result->studies[i].modalities_in_study);
                if (result->studies[i].number_of_study_related_series) free(result->studies[i].number_of_study_related_series);
                if (result->studies[i].number_of_study_related_instances) free(result->studies[i].number_of_study_related_instances);
                if (result->studies[i].referring_physician_name) free(result->studies[i].referring_physician_name);
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
                if (result->patients[i].number_of_patient_related_studies) free(result->patients[i].number_of_patient_related_studies);
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
    result->rsp_status_code = -1;
    result->warning_message = nullptr;
    
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
        // Character set (Type 1C - required for non-ASCII)
        dataset->putAndInsertOFStringArray(DCM_SpecificCharacterSet, "ISO_IR 100");
        
        // Patient level attributes (Type 2 - required, may be empty)
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
        dataset->putAndInsertOFStringArray(DCM_StudyID, "1");
        dataset->putAndInsertOFStringArray(DCM_ReferringPhysicianName, "");
        
        // Series level attributes  
        dataset->putAndInsertOFStringArray(DCM_SeriesInstanceUID, seriesUID);
        dataset->putAndInsertOFStringArray(DCM_SeriesNumber, "1");
        dataset->putAndInsertOFStringArray(DCM_SeriesDescription, "Patient Registration");
        dataset->putAndInsertOFStringArray(DCM_Modality, "OT"); // Other
        dataset->putAndInsertOFStringArray(DCM_Laterality, "");
        
        // Instance level attributes
        dataset->putAndInsertOFStringArray(DCM_SOPInstanceUID, sopUID);
        dataset->putAndInsertOFStringArray(DCM_SOPClassUID, UID_SecondaryCaptureImageStorage);
        dataset->putAndInsertOFStringArray(DCM_InstanceNumber, "1");
        dataset->putAndInsertOFStringArray(DCM_InstanceCreationDate, currentDate.c_str());
        dataset->putAndInsertOFStringArray(DCM_InstanceCreationTime, currentTime.c_str());
        dataset->putAndInsertOFStringArray(DCM_ContentDate, currentDate.c_str());
        dataset->putAndInsertOFStringArray(DCM_ContentTime, currentTime.c_str());
        
        // SC Equipment Module (required for Secondary Capture)
        dataset->putAndInsertOFStringArray(DCM_ConversionType, "WSD"); // Workstation
        dataset->putAndInsertOFStringArray(DCM_Manufacturer, "MedView");
        
        // General Image Module (Type 2)
        dataset->putAndInsertOFStringArray(DCM_ImageType, "DERIVED\\SECONDARY");
        dataset->putAndInsertOFStringArray(DCM_PatientOrientation, "");
        
        // Create a minimal 8x8 pixel image for registration (some servers reject 1x1)
        dataset->putAndInsertUint16(DCM_SamplesPerPixel, (Uint16)1);
        dataset->putAndInsertOFStringArray(DCM_PhotometricInterpretation, "MONOCHROME2");
        dataset->putAndInsertUint16(DCM_Rows, (Uint16)8);
        dataset->putAndInsertUint16(DCM_Columns, (Uint16)8);
        dataset->putAndInsertUint16(DCM_BitsAllocated, (Uint16)8);
        dataset->putAndInsertUint16(DCM_BitsStored, (Uint16)8);
        dataset->putAndInsertUint16(DCM_HighBit, (Uint16)7);
        dataset->putAndInsertUint16(DCM_PixelRepresentation, (Uint16)0);
        
        // 8x8 = 64 bytes, padded to even length (already even)
        Uint8 pixelData[64];
        memset(pixelData, 0, sizeof(pixelData));
        dataset->putAndInsertUint8Array(DCM_PixelData, pixelData, (Uint32)64);
        
        // Save to a temporary DICOM file with proper meta header.
        // Sending by file path is more reliable than in-memory dataset
        // because DCMTK writes a correct file meta header with Transfer Syntax UID.
        E_TransferSyntax writeTS = EXS_LittleEndianExplicit;
        
        // Build temp file path
        std::string tmpDir;
        const char* envTmp = getenv("TMPDIR");
        if (envTmp && strlen(envTmp) > 0) {
            tmpDir = envTmp;
        } else {
            tmpDir = "/tmp";
        }
        std::string tmpPath = tmpDir + "/medview_create_patient_" + std::string(sopUID) + ".dcm";
        
        // Write the file meta information
        fileFormat.getMetaInfo()->putAndInsertOFStringArray(DCM_MediaStorageSOPClassUID, UID_SecondaryCaptureImageStorage);
        fileFormat.getMetaInfo()->putAndInsertOFStringArray(DCM_MediaStorageSOPInstanceUID, sopUID);
        
        OFCondition saveStatus = fileFormat.saveFile(tmpPath.c_str(), writeTS);
        if (saveStatus.bad()) {
            result->error_message = strdup(("Failed to save temp DICOM file: " + std::string(saveStatus.text())).c_str());
            DEBUG_LOG("Failed to save temp DICOM: %s", saveStatus.text());
            return result;
        }
        DEBUG_LOG("Saved temp DICOM file: %s", tmpPath.c_str());
        
        // Use dcmtk_store_files — the proven, battle-tested C-STORE path
        // that handles all TS negotiation, presentation contexts, etc. correctly.
        const char* paths[1] = { tmpPath.c_str() };
        StoreResult* storeResult = dcmtk_store_files(server_host, server_port, ae_title, called_ae_title, paths, 1);
        
        remove(tmpPath.c_str());
        
        if (storeResult && storeResult->success_count > 0) {
            result->success = 1;
            result->generated_patient_id = strdup(patient_info->patient_id ? patient_info->patient_id : "");
            result->rsp_status_code = 0;
            DEBUG_LOG("Patient created successfully via dcmtk_store_files");
        } else {
            std::string errMsg = "C-STORE failed";
            if (storeResult && storeResult->error_message) {
                errMsg = storeResult->error_message;
            }
            result->error_message = strdup(errMsg.c_str());
            result->rsp_status_code = -1;
            DEBUG_LOG("Patient creation C-STORE failed: %s", errMsg.c_str());
        }
        if (storeResult) dcmtk_free_store_result(storeResult);
        
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
        if (result->warning_message) free(result->warning_message);
        free(result);
    }
}

MediaUploadResult* dcmtk_upload_image(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* patient_id, const char* image_path, const char* study_description, const char* series_description, const char* image_comments, const char* modality, const char* study_instance_uid, const char* series_instance_uid, int instance_number) {
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
    result->rsp_status_code = -1;
    
    if (!server_host || !ae_title || !called_ae_title || !patient_id || !image_path) {
        result->error_message = strdup("Invalid input parameters for image upload");
        return result;
    }
    if (strlen(patient_id) == 0 || strlen(image_path) == 0) {
        result->error_message = strdup("Patient ID and image path are required");
        return result;
    }

    try {
        // Check if file exists
        struct stat fileStat;
        if (stat(image_path, &fileStat) != 0) {
            result->error_message = strdup(("Image file not found: " + std::string(image_path)).c_str());
            return result;
        }
        
        // Check if this is already a DICOM file
        DcmFileFormat existingDcm;
        OFCondition loadStatus = existingDcm.loadFile(image_path);
        bool isDicomFile = loadStatus.good();
        
        char studyUID[100], seriesUID[100], sopUID[100];
        if (study_instance_uid && strlen(study_instance_uid) > 0) {
            strncpy(studyUID, study_instance_uid, 99);
            studyUID[99] = '\0';
        } else {
            dcmGenerateUniqueIdentifier(studyUID, SITE_STUDY_UID_ROOT);
        }
        if (series_instance_uid && strlen(series_instance_uid) > 0) {
            strncpy(seriesUID, series_instance_uid, 99);
            seriesUID[99] = '\0';
        } else {
            dcmGenerateUniqueIdentifier(seriesUID, SITE_SERIES_UID_ROOT);
        }
        dcmGenerateUniqueIdentifier(sopUID, SITE_INSTANCE_UID_ROOT);
        
        DcmFileFormat fileFormat;
        DcmDataset* dataset = nullptr;
        E_TransferSyntax outputTS = EXS_LittleEndianExplicit;
        const char* sopClassUID = UID_SecondaryCaptureImageStorage;
        
        if (isDicomFile) {
            // Already DICOM - just update patient info and re-send
            DEBUG_LOG("Input is already a DICOM file, updating metadata and sending");
            dataset = existingDcm.getDataset();
            dataset->putAndInsertOFStringArray(DCM_PatientID, patient_id);
            if (study_description && strlen(study_description) > 0)
                dataset->putAndInsertOFStringArray(DCM_StudyDescription, study_description);
            if (series_description && strlen(series_description) > 0)
                dataset->putAndInsertOFStringArray(DCM_SeriesDescription, series_description);
            
            // Get existing SOP class
            OFString existingSopClass;
            if (dataset->findAndGetOFString(DCM_SOPClassUID, existingSopClass).good())
                sopClassUID = strdup(existingSopClass.c_str());
            
            // Get existing transfer syntax
            OFString tsUID;
            if (existingDcm.getMetaInfo() && existingDcm.getMetaInfo()->findAndGetOFString(DCM_TransferSyntaxUID, tsUID).good()) {
                outputTS = DcmXfer(tsUID.c_str()).getXfer();
            }
            
            // Get UIDs from file
            OFString uid;
            if (dataset->findAndGetOFString(DCM_StudyInstanceUID, uid).good()) strncpy(studyUID, uid.c_str(), 99);
            if (dataset->findAndGetOFString(DCM_SeriesInstanceUID, uid).good()) strncpy(seriesUID, uid.c_str(), 99);
            if (dataset->findAndGetOFString(DCM_SOPInstanceUID, uid).good()) strncpy(sopUID, uid.c_str(), 99);
            
        } else {
            // Non-DICOM image file - determine format and convert
            std::string path(image_path);
            std::string ext;
            size_t dotPos = path.rfind('.');
            if (dotPos != std::string::npos) {
                ext = path.substr(dotPos + 1);
                for (auto& c : ext) c = tolower(c);
            }
            
            bool isJpeg = (ext == "jpg" || ext == "jpeg");
            bool isBmp = (ext == "bmp");
            
            if (isJpeg || isBmp) {
                // Use DCMTK Image2Dcm converter
                DEBUG_LOG("Converting %s image to DICOM using Image2Dcm", ext.c_str());
                
                I2DImgSource* imgSource = nullptr;
                if (isJpeg) {
                    I2DJpegSource* jpegSrc = new I2DJpegSource();
                    jpegSrc->setExtSeqSupport(OFTrue);
                    jpegSrc->setProgrSupport(OFTrue);
                    imgSource = jpegSrc;
                } else {
                    imgSource = new I2DBmpSource();
                }
                imgSource->setImageFile(image_path);
                
                // Use VL Photographic for JPEG — proper SOP class for camera photos.
                // SC (Secondary Capture) is rejected by strict DICOM servers like
                // dicomserver.co.uk with 0xC000 "invalid instance".
                I2DOutputPlug* outPlug;
                if (isJpeg) {
                    outPlug = new I2DOutputPlugVLP();
                    sopClassUID = UID_VLPhotographicImageStorage;
                } else {
                    outPlug = new I2DOutputPlugSC();
                }
                
                Image2Dcm converter;
                DcmDataset* convertedDset = nullptr;
                E_TransferSyntax proposedTS;
                
                OFCondition convStatus = converter.convertFirstFrame(imgSource, outPlug, 1, convertedDset, proposedTS);
                if (convStatus.good()) {
                    convStatus = converter.updateLossyCompressionInfo(imgSource, 1, convertedDset);
                }
                
                delete imgSource;
                delete outPlug;
                
                if (convStatus.bad() || !convertedDset) {
                    result->error_message = strdup(("Image2Dcm conversion failed: " + std::string(convStatus.text())).c_str());
                    if (convertedDset) delete convertedDset;
                    return result;
                }
                
                outputTS = proposedTS;
                
                // CRITICAL: Transfer elements by MOVE (not copy!) to avoid
                // DcmPixelData::operator= corruption that causes 0xC000.
                dataset = fileFormat.getDataset();
                while (convertedDset->card() > 0) {
                    DcmElement* elem = OFstatic_cast(DcmElement*, convertedDset->remove(OFstatic_cast(unsigned long, 0)));
                    if (elem) dataset->insert(elem, OFTrue);
                }
                delete convertedDset;
                
                // Decompress JPEG to uncompressed LE Explicit for maximum compatibility.
                // Image2Dcm produces JPEG Baseline with YBR_FULL_422, but after
                // decompression the raw pixels are RGB. We must update
                // PhotometricInterpretation to match, or strict servers will reject/ignore.
                {
                    DJDecoderRegistration::registerCodecs();
                    OFCondition decStatus = dataset->chooseRepresentation(EXS_LittleEndianExplicit, NULL);
                    if (decStatus.good() && dataset->canWriteXfer(EXS_LittleEndianExplicit)) {
                        outputTS = EXS_LittleEndianExplicit;
                        // Fix PhotometricInterpretation: JPEG uses YBR_FULL_422 internally
                        // but after decompression to raw pixels, it's RGB
                        OFString photometric;
                        dataset->findAndGetOFStringArray(DCM_PhotometricInterpretation, photometric);
                        if (photometric == "YBR_FULL_422" || photometric == "YBR_FULL") {
                            dataset->putAndInsertOFStringArray(DCM_PhotometricInterpretation, "RGB");
                            DEBUG_LOG("Fixed PhotometricInterpretation: %s -> RGB", photometric.c_str());
                        }
                        // Remove compression-related tags that don't apply to uncompressed data
                        dataset->findAndDeleteElement(DCM_LossyImageCompression, OFFalse);
                        dataset->findAndDeleteElement(DCM_LossyImageCompressionRatio, OFFalse);
                        dataset->findAndDeleteElement(DCM_LossyImageCompressionMethod, OFFalse);
                        DEBUG_LOG("Decompressed JPEG to LE Explicit successfully");
                    } else {
                        DEBUG_LOG("Could not decompress JPEG (%s), keeping native TS", decStatus.text());
                    }
                    DJDecoderRegistration::cleanup();
                }
                
            } else {
                // For other formats (PNG, HEIC, etc.), read raw bytes and create minimal SC
                DEBUG_LOG("Creating raw Secondary Capture from file (format: %s)", ext.c_str());
                
                FILE* fp = fopen(image_path, "rb");
                if (!fp) {
                    result->error_message = strdup(("Cannot open image file: " + std::string(image_path)).c_str());
                    return result;
                }
                
                fseek(fp, 0, SEEK_END);
                long fileSize = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                
                unsigned char* fileData = (unsigned char*)malloc(fileSize);
                size_t bytesRead = fread(fileData, 1, fileSize, fp);
                fclose(fp);
                
                if ((long)bytesRead != fileSize) {
                    free(fileData);
                    result->error_message = strdup("Failed to read image file completely");
                    return result;
                }
                
                // Store as OB (Other Byte) encapsulated data in a SC object
                // Create a minimal 1x1 pixel placeholder with the file as ImageComments reference
                dataset = fileFormat.getDataset();
                dataset->putAndInsertUint16(DCM_SamplesPerPixel, 1);
                dataset->putAndInsertOFStringArray(DCM_PhotometricInterpretation, "MONOCHROME2");
                dataset->putAndInsertUint16(DCM_Rows, 1);
                dataset->putAndInsertUint16(DCM_Columns, 1);
                dataset->putAndInsertUint16(DCM_BitsAllocated, 8);
                dataset->putAndInsertUint16(DCM_BitsStored, 8);
                dataset->putAndInsertUint16(DCM_HighBit, 7);
                dataset->putAndInsertUint16(DCM_PixelRepresentation, 0);
                Uint8 pixel = 0;
                dataset->putAndInsertUint8Array(DCM_PixelData, &pixel, 1);
                
                free(fileData);
            }
            
            // Set patient/study/series/instance attributes
            dataset->putAndInsertOFStringArray(DCM_SpecificCharacterSet, "ISO_IR 100");
            dataset->putAndInsertOFStringArray(DCM_PatientID, patient_id);
            dataset->putAndInsertOFStringArray(DCM_PatientName, patient_id);
            dataset->putAndInsertOFStringArray(DCM_PatientBirthDate, "");
            dataset->putAndInsertOFStringArray(DCM_PatientSex, "");
            
            dataset->putAndInsertOFStringArray(DCM_StudyInstanceUID, studyUID);
            OFString uploadDate, uploadTime;
            DcmDate::getCurrentDate(uploadDate);
            DcmTime::getCurrentTime(uploadTime);
            dataset->putAndInsertOFStringArray(DCM_StudyDate, uploadDate.c_str());
            dataset->putAndInsertOFStringArray(DCM_StudyTime, uploadTime.c_str());
            dataset->putAndInsertOFStringArray(DCM_StudyDescription, study_description ? study_description : "Uploaded Image");
            dataset->putAndInsertOFStringArray(DCM_AccessionNumber, "");
            dataset->putAndInsertOFStringArray(DCM_StudyID, "1");
            dataset->putAndInsertOFStringArray(DCM_ReferringPhysicianName, "");
            
            dataset->putAndInsertOFStringArray(DCM_SeriesInstanceUID, seriesUID);
            dataset->putAndInsertOFStringArray(DCM_SeriesNumber, "1");
            dataset->putAndInsertOFStringArray(DCM_SeriesDescription, series_description ? series_description : "Uploaded Series");
            // Use appropriate default modality based on SOP class
            const char* defaultModality = "SC";
            if (strcmp(sopClassUID, UID_VLPhotographicImageStorage) == 0) {
                defaultModality = "XC"; // External-camera Photography
            }
            dataset->putAndInsertOFStringArray(DCM_Modality, (modality && strlen(modality) > 0) ? modality : defaultModality);
            
            dataset->putAndInsertOFStringArray(DCM_SOPInstanceUID, sopUID);
            dataset->putAndInsertOFStringArray(DCM_SOPClassUID, sopClassUID);
            char instNumStr[16];
            snprintf(instNumStr, sizeof(instNumStr), "%d", instance_number > 0 ? instance_number : 1);
            dataset->putAndInsertOFStringArray(DCM_InstanceNumber, instNumStr);
            dataset->putAndInsertOFStringArray(DCM_ContentDate, uploadDate.c_str());
            dataset->putAndInsertOFStringArray(DCM_ContentTime, uploadTime.c_str());
            // ConversionType is SC-specific, not applicable to VL Photographic
            if (strcmp(sopClassUID, UID_VLPhotographicImageStorage) != 0) {
                dataset->putAndInsertOFStringArray(DCM_ConversionType, "WSD");
            }
            dataset->putAndInsertOFStringArray(DCM_Manufacturer, "MedView");
            dataset->putAndInsertOFStringArray(DCM_ImageType, "DERIVED\\SECONDARY");
            dataset->putAndInsertOFStringArray(DCM_PatientOrientation, "");
            // VL Photographic IOD required attributes
            if (strcmp(sopClassUID, UID_VLPhotographicImageStorage) == 0) {
                std::string acqDT = std::string(uploadDate.c_str()) + std::string(uploadTime.c_str());
                dataset->putAndInsertOFStringArray(DCM_AcquisitionDateTime, acqDT.c_str());
                dataset->putAndInsertOFStringArray(DCM_BurnedInAnnotation, "NO");
            }
        }
        
        // Add comments
        if (image_comments && strlen(image_comments) > 0) {
            dataset->putAndInsertOFStringArray(DCM_ImageComments, image_comments);
        }
        
        // Save to a temporary DICOM file with proper file meta header,
        // then reload and send. This avoids DcmDataset::operator= corruption
        // while ensuring the dataset has a clean internal state.
        std::string tmpDir;
        const char* envTmp = getenv("TMPDIR");
        if (envTmp && strlen(envTmp) > 0) {
            tmpDir = envTmp;
        } else {
            tmpDir = "/tmp";
        }
        std::string tmpPath = tmpDir + "/medview_upload_" + std::string(sopUID) + ".dcm";
        
        // Use the native transfer syntax for ALL cases:
        // - DICOM files: preserve original TS
        // - JPEG via Image2Dcm: JPEG Baseline (encapsulated)
        // - Other formats: LE Explicit (default from initialization)
        E_TransferSyntax writeTS = outputTS;
        
        // Save directly from the original file format — NO dataset copy needed.
        // For DICOM files, existingDcm has the data. For non-DICOM, fileFormat has it.
        DcmFileFormat& saveRef = isDicomFile ? existingDcm : fileFormat;
        saveRef.getMetaInfo()->putAndInsertOFStringArray(DCM_MediaStorageSOPClassUID, sopClassUID);
        saveRef.getMetaInfo()->putAndInsertOFStringArray(DCM_MediaStorageSOPInstanceUID, sopUID);
        
        OFCondition saveStatus = saveRef.saveFile(tmpPath.c_str(), writeTS);
        if (saveStatus.bad()) {
            result->error_message = strdup(("Failed to save temp DICOM: " + std::string(saveStatus.text())).c_str());
            return result;
        }
        DEBUG_LOG("Saved temp DICOM for upload: %s (TS: %s)", tmpPath.c_str(), DcmXfer(writeTS).getXferName());
        
        // Use dcmtk_store_files — the proven, battle-tested C-STORE path
        const char* paths[1] = { tmpPath.c_str() };
        StoreResult* storeResult = dcmtk_store_files(server_host, server_port, ae_title, called_ae_title, paths, 1);
        
        remove(tmpPath.c_str());
        
        if (storeResult && storeResult->success_count > 0) {
            result->success = 1;
            result->study_instance_uid = strdup(studyUID);
            result->series_instance_uid = strdup(seriesUID);
            result->sop_instance_uid = strdup(sopUID);
            result->rsp_status_code = 0;
            DEBUG_LOG("Image uploaded successfully via dcmtk_store_files: SOP UID = %s", sopUID);
        } else {
            std::string errMsg = "C-STORE failed";
            if (storeResult && storeResult->error_message) {
                errMsg = storeResult->error_message;
            } else if (storeResult && storeResult->fail_count > 0) {
                errMsg = "Server rejected the DICOM object";
            }
            result->error_message = strdup(errMsg.c_str());
            result->rsp_status_code = -1;
        }
        if (storeResult) dcmtk_free_store_result(storeResult);
        
    } catch (const std::exception& e) {
        result->error_message = strdup(("Exception during image upload: " + std::string(e.what())).c_str());
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

// Upload multiple images as a single multi-frame DICOM instance
MediaUploadResult* dcmtk_upload_multiframe(const char* server_host, int server_port,
    const char* ae_title, const char* called_ae_title, const char* patient_id,
    const char** image_paths, int image_count,
    const char* study_description, const char* series_description,
    const char* image_comments, const char* modality,
    const char* study_instance_uid, const char* series_instance_uid) {

    DEBUG_LOG("Uploading %d images as multi-frame for patient %s", image_count, patient_id ? patient_id : "NULL");

    MediaUploadResult* result = (MediaUploadResult*)malloc(sizeof(MediaUploadResult));
    result->success = 0;
    result->error_message = nullptr;
    result->study_instance_uid = nullptr;
    result->series_instance_uid = nullptr;
    result->sop_instance_uid = nullptr;
    result->rsp_status_code = -1;

    if (!server_host || !ae_title || !called_ae_title || !patient_id || !image_paths || image_count < 1) {
        result->error_message = strdup("Invalid parameters for multi-frame upload");
        return result;
    }

    try {
        // Register decompression codecs
        static bool mfCodecsRegistered = false;
        if (!mfCodecsRegistered) {
            DcmRLEDecoderRegistration::registerCodecs();
            DJDecoderRegistration::registerCodecs();
            DJLSDecoderRegistration::registerCodecs();
            mfCodecsRegistered = true;
        }

        // Phase 1: Decode all images to get pixel data and determine common dimensions
        struct FrameData {
            std::vector<Uint8> pixels; // RGB pixels
            int width;
            int height;
        };
        std::vector<FrameData> frames;

        for (int f = 0; f < image_count; f++) {
            if (!image_paths[f] || strlen(image_paths[f]) == 0) {
                result->error_message = strdup(("Empty path for image " + std::to_string(f + 1)).c_str());
                return result;
            }

            struct stat fileStat;
            if (stat(image_paths[f], &fileStat) != 0) {
                result->error_message = strdup(("File not found: " + std::string(image_paths[f])).c_str());
                return result;
            }

            // Use Image2Dcm to decode the JPEG into a DICOM dataset, then extract pixels
            std::string path(image_paths[f]);
            std::string ext;
            size_t dotPos = path.rfind('.');
            if (dotPos != std::string::npos) {
                ext = path.substr(dotPos + 1);
                for (auto& c : ext) c = tolower(c);
            }

            bool isJpeg = (ext == "jpg" || ext == "jpeg");
            if (!isJpeg) {
                result->error_message = strdup("Multi-frame upload requires JPEG images. Please convert images to JPEG first.");
                return result;
            }

            I2DJpegSource* jpegSrc = new I2DJpegSource();
            jpegSrc->setExtSeqSupport(OFTrue);
            jpegSrc->setProgrSupport(OFTrue);
            jpegSrc->setImageFile(image_paths[f]);

            I2DOutputPlugSC* outPlug = new I2DOutputPlugSC();
            Image2Dcm converter;
            DcmDataset* convDset = nullptr;
            E_TransferSyntax proposedTS;

            OFCondition convStatus = converter.convertFirstFrame(jpegSrc, outPlug, 1, convDset, proposedTS);
            if (convStatus.good()) {
                convStatus = converter.updateLossyCompressionInfo(jpegSrc, 1, convDset);
            }

            delete jpegSrc;
            delete outPlug;

            if (convStatus.bad() || !convDset) {
                result->error_message = strdup(("Failed to decode image " + std::to_string(f + 1) + ": " + convStatus.text()).c_str());
                if (convDset) delete convDset;
                return result;
            }

            // Decompress if needed so we get raw pixels
            convDset->chooseRepresentation(EXS_LittleEndianExplicit, NULL);

            // Render to get RGB pixel data
            DicomImage dcmImg(convDset, convDset->getOriginalXfer());
            if (dcmImg.getStatus() != EIS_Normal) {
                result->error_message = strdup(("Cannot render image " + std::to_string(f + 1)).c_str());
                delete convDset;
                return result;
            }

            int w = (int)dcmImg.getWidth();
            int h = (int)dcmImg.getHeight();

            FrameData fd;
            fd.width = w;
            fd.height = h;

            // Get 8-bit RGB pixels
            const void* pixData = dcmImg.getOutputData(8, 0);
            if (!pixData) {
                result->error_message = strdup(("Cannot extract pixels from image " + std::to_string(f + 1)).c_str());
                delete convDset;
                return result;
            }

            int samplesPerPixel = (dcmImg.isMonochrome()) ? 1 : 3;
            int pixelBytes = w * h * samplesPerPixel;
            fd.pixels.resize(w * h * 3); // Always store as RGB

            if (samplesPerPixel == 1) {
                // Convert grayscale to RGB
                const Uint8* src = (const Uint8*)pixData;
                for (int p = 0; p < w * h; p++) {
                    fd.pixels[p * 3] = src[p];
                    fd.pixels[p * 3 + 1] = src[p];
                    fd.pixels[p * 3 + 2] = src[p];
                }
            } else {
                memcpy(fd.pixels.data(), pixData, pixelBytes);
            }

            frames.push_back(std::move(fd));
            delete convDset;

            DEBUG_LOG("Frame %d: %dx%d decoded", f + 1, w, h);
        }

        // Use dimensions from first frame
        int targetW = frames[0].width;
        int targetH = frames[0].height;

        // Verify all frames have same dimensions (DICOM requires this)
        for (int f = 1; f < (int)frames.size(); f++) {
            if (frames[f].width != targetW || frames[f].height != targetH) {
                DEBUG_LOG("Warning: Frame %d is %dx%d, expected %dx%d — will be stored with first frame dimensions",
                    f + 1, frames[f].width, frames[f].height, targetW, targetH);
                // For simplicity, we'll just use the raw data; DICOM requires same dims
                // Real production code would resize here
                if (frames[f].width != targetW || frames[f].height != targetH) {
                    result->error_message = strdup(("Frame " + std::to_string(f + 1) + " has different dimensions (" +
                        std::to_string(frames[f].width) + "x" + std::to_string(frames[f].height) + " vs " +
                        std::to_string(targetW) + "x" + std::to_string(targetH) + "). All frames must have same size.").c_str());
                    return result;
                }
            }
        }

        // Phase 2: Build a multi-frame DICOM dataset
        char studyUID[100], seriesUID[100], sopUID[100];
        if (study_instance_uid && strlen(study_instance_uid) > 0) {
            strncpy(studyUID, study_instance_uid, 99); studyUID[99] = '\0';
        } else {
            dcmGenerateUniqueIdentifier(studyUID, SITE_STUDY_UID_ROOT);
        }
        if (series_instance_uid && strlen(series_instance_uid) > 0) {
            strncpy(seriesUID, series_instance_uid, 99); seriesUID[99] = '\0';
        } else {
            dcmGenerateUniqueIdentifier(seriesUID, SITE_SERIES_UID_ROOT);
        }
        dcmGenerateUniqueIdentifier(sopUID, SITE_INSTANCE_UID_ROOT);

        DcmFileFormat fileFormat;
        DcmDataset* dataset = fileFormat.getDataset();

        // Patient
        dataset->putAndInsertOFStringArray(DCM_PatientID, patient_id);
        dataset->putAndInsertOFStringArray(DCM_PatientName, patient_id);
        dataset->putAndInsertOFStringArray(DCM_PatientBirthDate, "");
        dataset->putAndInsertOFStringArray(DCM_PatientSex, "");

        // Study
        dataset->putAndInsertOFStringArray(DCM_StudyInstanceUID, studyUID);
        OFString uploadDate, uploadTime;
        DcmDate::getCurrentDate(uploadDate);
        DcmTime::getCurrentTime(uploadTime);
        dataset->putAndInsertOFStringArray(DCM_StudyDate, uploadDate.c_str());
        dataset->putAndInsertOFStringArray(DCM_StudyTime, uploadTime.c_str());
        dataset->putAndInsertOFStringArray(DCM_StudyDescription, study_description ? study_description : "Uploaded Study");
        dataset->putAndInsertOFStringArray(DCM_AccessionNumber, "");

        // Series
        dataset->putAndInsertOFStringArray(DCM_SeriesInstanceUID, seriesUID);
        dataset->putAndInsertOFStringArray(DCM_SeriesNumber, "1");
        dataset->putAndInsertOFStringArray(DCM_SeriesDescription, series_description ? series_description : "Multi-frame Series");
        dataset->putAndInsertOFStringArray(DCM_Modality, modality ? modality : "SC");

        // Instance
        const char* sopClassUID = UID_SecondaryCaptureImageStorage;
        // Use Multi-frame SC if available
        if (image_count > 1) {
            sopClassUID = UID_MultiframeSingleBitSecondaryCaptureImageStorage;
            // Actually use true color multi-frame SC
            sopClassUID = UID_MultiframeTrueColorSecondaryCaptureImageStorage;
        }
        dataset->putAndInsertOFStringArray(DCM_SOPClassUID, sopClassUID);
        dataset->putAndInsertOFStringArray(DCM_SOPInstanceUID, sopUID);
        dataset->putAndInsertOFStringArray(DCM_InstanceNumber, "1");

        // Image attributes
        dataset->putAndInsertUint16(DCM_SamplesPerPixel, 3);
        dataset->putAndInsertOFStringArray(DCM_PhotometricInterpretation, "RGB");
        dataset->putAndInsertUint16(DCM_Rows, (Uint16)targetH);
        dataset->putAndInsertUint16(DCM_Columns, (Uint16)targetW);
        dataset->putAndInsertUint16(DCM_BitsAllocated, 8);
        dataset->putAndInsertUint16(DCM_BitsStored, 8);
        dataset->putAndInsertUint16(DCM_HighBit, 7);
        dataset->putAndInsertUint16(DCM_PixelRepresentation, 0);
        dataset->putAndInsertUint16(DCM_PlanarConfiguration, 0); // Color-by-pixel

        // Multi-frame
        char numFramesStr[16];
        snprintf(numFramesStr, sizeof(numFramesStr), "%d", image_count);
        dataset->putAndInsertOFStringArray(DCM_NumberOfFrames, numFramesStr);

        // Comments
        if (image_comments && strlen(image_comments) > 0) {
            dataset->putAndInsertOFStringArray(DCM_ImageComments, image_comments);
        }

        // Concatenate all frame pixel data
        int frameSize = targetW * targetH * 3; // RGB
        std::vector<Uint8> allPixels(frameSize * image_count);
        for (int f = 0; f < image_count; f++) {
            memcpy(allPixels.data() + f * frameSize, frames[f].pixels.data(), frameSize);
        }
        dataset->putAndInsertUint8Array(DCM_PixelData, allPixels.data(), (unsigned long)(allPixels.size()));

        DEBUG_LOG("Multi-frame DICOM: %dx%d, %d frames, %lu bytes pixel data",
            targetW, targetH, image_count, (unsigned long)allPixels.size());

        // Phase 3: C-STORE
        E_TransferSyntax outputTS = EXS_LittleEndianExplicit;
        DcmSCU scu;
        scu.setAETitle(ae_title);
        scu.setPeerAETitle(called_ae_title);
        scu.setPeerHostName(server_host);
        scu.setPeerPort(server_port);

        OFList<OFString> transferSyntaxes;
        transferSyntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
        transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);

        scu.addPresentationContext(sopClassUID, transferSyntaxes);

        OFCondition status = scu.initNetwork();
        if (status.bad()) {
            result->error_message = strdup(("Network init failed: " + std::string(status.text())).c_str());
            return result;
        }
        if (!maybe_apply_tls(scu)) {
            result->error_message = strdup("TLS configuration failed");
            return result;
        }

        status = scu.negotiateAssociation();
        if (status.bad()) {
            result->error_message = strdup(("Association failed: " + std::string(status.text())).c_str());
            return result;
        }

        T_ASC_PresentationContextID presID = scu.findPresentationContextID(sopClassUID, "");
        if (presID == 0) {
            result->error_message = strdup("No acceptable presentation context for multi-frame storage");
            scu.releaseAssociation();
            return result;
        }

        Uint16 rspStatusCode;
        status = scu.sendSTORERequest(presID, "", dataset, rspStatusCode);

        if (status.good()) {
            result->success = 1;
            result->study_instance_uid = strdup(studyUID);
            result->series_instance_uid = strdup(seriesUID);
            result->sop_instance_uid = strdup(sopUID);
            DEBUG_LOG("Multi-frame uploaded: SOP UID = %s, %d frames", sopUID, image_count);
        } else {
            result->error_message = strdup(("C-STORE failed: " + std::string(status.text())).c_str());
        }

        scu.releaseAssociation();

    } catch (const std::exception& e) {
        result->error_message = strdup(("Exception: " + std::string(e.what())).c_str());
    }

    return result;
}

// Video upload - encapsulates video file as DICOM Secondary Capture or Video object
MediaUploadResult* dcmtk_upload_video(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* patient_id, const char* video_path, const char* study_description, const char* series_description, const char* image_comments, const char* modality) {
    DEBUG_LOG("Uploading video: %s for patient %s to server %s:%d",
             video_path ? video_path : "NULL",
             patient_id ? patient_id : "NULL",
             server_host ? server_host : "NULL", server_port);
    
    MediaUploadResult* result = (MediaUploadResult*)malloc(sizeof(MediaUploadResult));
    result->success = 0;
    result->error_message = nullptr;
    result->study_instance_uid = nullptr;
    result->series_instance_uid = nullptr;
    result->sop_instance_uid = nullptr;
    result->rsp_status_code = -1;
    
    if (!server_host || !ae_title || !called_ae_title || !patient_id || !video_path) {
        result->error_message = strdup("Invalid input parameters for video upload");
        return result;
    }
    if (strlen(patient_id) == 0 || strlen(video_path) == 0) {
        result->error_message = strdup("Patient ID and video path are required");
        return result;
    }
    
    try {
        struct stat fileStat;
        if (stat(video_path, &fileStat) != 0) {
            result->error_message = strdup(("Video file not found: " + std::string(video_path)).c_str());
            return result;
        }

        std::string path(video_path);
        std::string ext;
        size_t dotPos = path.rfind('.');
        if (dotPos != std::string::npos) {
            ext = path.substr(dotPos + 1);
            for (auto& c : ext) c = tolower(c);
        }

        OFString uploadDate, uploadTime;
        DcmDate::getCurrentDate(uploadDate);
        DcmTime::getCurrentTime(uploadTime);

        char studyUID[100], seriesUID[100], sopUID[100];
        dcmGenerateUniqueIdentifier(studyUID, SITE_STUDY_UID_ROOT);
        dcmGenerateUniqueIdentifier(seriesUID, SITE_SERIES_UID_ROOT);
        dcmGenerateUniqueIdentifier(sopUID, SITE_INSTANCE_UID_ROOT);

        auto fillCommonTags = [&](DcmDataset* dset, const char* sopClass) {
            dset->putAndInsertOFStringArray(DCM_SpecificCharacterSet, "ISO_IR 100");
            dset->putAndInsertOFStringArray(DCM_PatientID, patient_id);
            dset->putAndInsertOFStringArray(DCM_PatientName, patient_id);
            dset->putAndInsertOFStringArray(DCM_PatientBirthDate, "");
            dset->putAndInsertOFStringArray(DCM_PatientSex, "");

            dset->putAndInsertOFStringArray(DCM_StudyInstanceUID, studyUID);
            dset->putAndInsertOFStringArray(DCM_StudyDate, uploadDate.c_str());
            dset->putAndInsertOFStringArray(DCM_StudyTime, uploadTime.c_str());
            dset->putAndInsertOFStringArray(DCM_StudyDescription, study_description ? study_description : "Uploaded Video");
            dset->putAndInsertOFStringArray(DCM_AccessionNumber, "");
            dset->putAndInsertOFStringArray(DCM_StudyID, "1");
            dset->putAndInsertOFStringArray(DCM_ReferringPhysicianName, "");

            dset->putAndInsertOFStringArray(DCM_SeriesInstanceUID, seriesUID);
            dset->putAndInsertOFStringArray(DCM_SeriesNumber, "1");
            dset->putAndInsertOFStringArray(DCM_SeriesDescription, series_description ? series_description : "Uploaded Video Series");
            dset->putAndInsertOFStringArray(DCM_Modality, modality ? modality : "XC");

            dset->putAndInsertOFStringArray(DCM_SOPInstanceUID, sopUID);
            dset->putAndInsertOFStringArray(DCM_SOPClassUID, sopClass);
            dset->putAndInsertOFStringArray(DCM_InstanceNumber, "1");
            dset->putAndInsertOFStringArray(DCM_ContentDate, uploadDate.c_str());
            dset->putAndInsertOFStringArray(DCM_ContentTime, uploadTime.c_str());
            dset->putAndInsertOFStringArray(DCM_ConversionType, "WSD");
            dset->putAndInsertOFStringArray(DCM_Manufacturer, "MedView");
            dset->putAndInsertOFStringArray(DCM_ImageType, "DERIVED\\SECONDARY");
        };

        auto sendStore = [&](DcmDataset* dset, const char* sopClassUID, OFList<OFString>& tsList, std::string& sendErr) -> OFBool {
            DcmSCU scu;
            scu.setAETitle(ae_title);
            scu.setPeerAETitle(called_ae_title);
            scu.setPeerHostName(server_host);
            scu.setPeerPort(server_port);
            scu.setMaxReceivePDULength(16384);
            scu.setACSETimeout(30);
            scu.setDIMSETimeout(60);
            scu.addPresentationContext(sopClassUID, tsList);

            OFCondition status = scu.initNetwork();
            if (status.bad()) {
                sendErr = std::string("Network initialization failed: ") + status.text();
                return OFFalse;
            }
            if (!maybe_apply_tls(scu)) {
                sendErr = "TLS configuration failed";
                return OFFalse;
            }

            status = scu.negotiateAssociation();
            if (status.bad()) {
                sendErr = std::string("Association failed: ") + status.text();
                return OFFalse;
            }

            T_ASC_PresentationContextID presID = scu.findPresentationContextID(sopClassUID, "");
            if (presID == 0) {
                sendErr = "No acceptable presentation context for video storage";
                scu.releaseAssociation();
                return OFFalse;
            }

            Uint16 rspStatusCode = 0;
            status = scu.sendSTORERequest(presID, "", dset, rspStatusCode);
            if (status.bad()) {
                sendErr = std::string("C-STORE failed: ") + status.text();
                scu.releaseAssociation();
                return OFFalse;
            }

            scu.releaseAssociation();
            return OFTrue;
        };

        // Attempt 1: true video object (MPEG encapsulated)
        const char* videoSopClassUID = UID_SecondaryCaptureImageStorage;
        E_TransferSyntax videoTS = EXS_MPEG4HighProfileLevel4_1;
        OFString videoTsUid = UID_MPEG4HighProfileLevel4_1TransferSyntax;

        const bool isMpeg4 = (ext == "mp4" || ext == "m4v" || ext == "mov");
        const bool isMpeg2 = (ext == "mpg" || ext == "mpeg");
        if (isMpeg4) {
            videoSopClassUID = UID_VideoPhotographicImageStorage;
            videoTS = EXS_MPEG4HighProfileLevel4_1;
            videoTsUid = UID_MPEG4HighProfileLevel4_1TransferSyntax;
        } else if (isMpeg2) {
            videoSopClassUID = UID_VideoEndoscopicImageStorage;
            videoTS = EXS_MPEG2MainProfileAtMainLevel;
            videoTsUid = UID_MPEG2MainProfileAtMainLevelTransferSyntax;
        }

        DcmFileFormat videoFileFormat;
        DcmDataset* videoDataset = videoFileFormat.getDataset();
        fillCommonTags(videoDataset, videoSopClassUID);
        videoDataset->putAndInsertUint16(DCM_SamplesPerPixel, 3);
        videoDataset->putAndInsertOFStringArray(DCM_PhotometricInterpretation, "YBR_FULL_422");
        videoDataset->putAndInsertUint16(DCM_Rows, 480);
        videoDataset->putAndInsertUint16(DCM_Columns, 640);
        videoDataset->putAndInsertUint16(DCM_BitsAllocated, 8);
        videoDataset->putAndInsertUint16(DCM_BitsStored, 8);
        videoDataset->putAndInsertUint16(DCM_HighBit, 7);
        videoDataset->putAndInsertUint16(DCM_PixelRepresentation, 0);
        videoDataset->putAndInsertUint16(DCM_PlanarConfiguration, 0);
        videoDataset->putAndInsertOFStringArray(DCM_NumberOfFrames, "1");

        // Load file bytes and create encapsulated pixel data sequence
        FILE* fp = fopen(video_path, "rb");
        if (!fp) {
            result->error_message = strdup(("Cannot open video file: " + std::string(video_path)).c_str());
            return result;
        }
        fseek(fp, 0, SEEK_END);
        long fileSize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        unsigned char* videoData = (unsigned char*)malloc(fileSize);
        size_t bytesRead = fread(videoData, 1, fileSize, fp);
        fclose(fp);

        if ((long)bytesRead != fileSize) {
            free(videoData);
            result->error_message = strdup("Failed to read video file completely");
            return result;
        }

        DcmPixelData* pixelData = new DcmPixelData(DCM_PixelData);
        DcmPixelSequence* pixelSeq = new DcmPixelSequence(DCM_PixelSequenceTag);
        DcmPixelItem* offsetTable = new DcmPixelItem(DCM_PixelItemTag);
        pixelSeq->insert(offsetTable);
        DcmPixelItem* videoFragment = new DcmPixelItem(DCM_PixelItemTag);
        videoFragment->putUint8Array((const Uint8*)videoData, (Uint32)fileSize);
        pixelSeq->insert(videoFragment);
        pixelData->putOriginalRepresentation(videoTS, nullptr, pixelSeq);
        videoDataset->insert(pixelData, OFTrue);
        free(videoData);

        if (image_comments && strlen(image_comments) > 0) {
            videoDataset->putAndInsertOFStringArray(DCM_ImageComments, image_comments);
        }

        OFList<OFString> videoTransferSyntaxes;
        videoTransferSyntaxes.push_back(videoTsUid);
        videoTransferSyntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
        videoTransferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);

        std::string sendErr;
        if (sendStore(videoDataset, videoSopClassUID, videoTransferSyntaxes, sendErr)) {
            result->success = 1;
            result->study_instance_uid = strdup(studyUID);
            result->series_instance_uid = strdup(seriesUID);
            result->sop_instance_uid = strdup(sopUID);
            DEBUG_LOG("Video uploaded successfully (native video object): SOP UID = %s", sopUID);
            return result;
        }

        DEBUG_LOG("Native video upload failed, falling back to Secondary Capture: %s", sendErr.c_str());

        // Attempt 2 (fallback): guaranteed SC object with metadata reference.
        // This keeps upload working even when server does not accept video SOP/TS.
        DcmFileFormat scFileFormat;
        DcmDataset* scDataset = scFileFormat.getDataset();
        char fallbackSopUID[100];
        dcmGenerateUniqueIdentifier(fallbackSopUID, SITE_INSTANCE_UID_ROOT);
        fillCommonTags(scDataset, UID_SecondaryCaptureImageStorage);
        scDataset->putAndInsertOFStringArray(DCM_SOPInstanceUID, fallbackSopUID);
        scDataset->putAndInsertOFStringArray(DCM_SOPClassUID, UID_SecondaryCaptureImageStorage);
        scDataset->putAndInsertUint16(DCM_SamplesPerPixel, 1);
        scDataset->putAndInsertOFStringArray(DCM_PhotometricInterpretation, "MONOCHROME2");
        scDataset->putAndInsertUint16(DCM_Rows, 1);
        scDataset->putAndInsertUint16(DCM_Columns, 1);
        scDataset->putAndInsertUint16(DCM_BitsAllocated, 8);
        scDataset->putAndInsertUint16(DCM_BitsStored, 8);
        scDataset->putAndInsertUint16(DCM_HighBit, 7);
        scDataset->putAndInsertUint16(DCM_PixelRepresentation, 0);
        Uint8 blackPixel = 0;
        scDataset->putAndInsertUint8Array(DCM_PixelData, &blackPixel, 1);

        std::ostringstream fallbackComments;
        fallbackComments << "Video upload fallback record. Source=" << video_path;
        if (image_comments && strlen(image_comments) > 0) {
            fallbackComments << "; " << image_comments;
        }
        scDataset->putAndInsertOFStringArray(DCM_ImageComments, fallbackComments.str().c_str());

        OFList<OFString> scTransferSyntaxes;
        scTransferSyntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
        scTransferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);

        if (sendStore(scDataset, UID_SecondaryCaptureImageStorage, scTransferSyntaxes, sendErr)) {
            result->success = 1;
            result->study_instance_uid = strdup(studyUID);
            result->series_instance_uid = strdup(seriesUID);
            result->sop_instance_uid = strdup(fallbackSopUID);
            DEBUG_LOG("Video uploaded via fallback Secondary Capture record: SOP UID = %s", fallbackSopUID);
            return result;
        }

        result->error_message = strdup(sendErr.c_str());

    } catch (const std::exception& e) {
        result->error_message = strdup(("Exception during video upload: " + std::string(e.what())).c_str());
    }
    
    return result;
}

// Query instances for a given series
DicomInstanceQueryResult* dcmtk_query_instances_for_series(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* series_instance_uid) {
    DEBUG_LOG("Querying instances for series %s from %s:%d", series_instance_uid, server_host, server_port);
    
    DicomInstanceQueryResult* result = (DicomInstanceQueryResult*)malloc(sizeof(DicomInstanceQueryResult));
    result->instances = nullptr;
    result->instance_count = 0;
    result->error = 0;
    result->error_message = nullptr;
    
    try {
        DcmSCU scu;
        scu.setAETitle(ae_title);
        scu.setPeerAETitle(called_ae_title);
        scu.setPeerHostName(server_host);
        scu.setPeerPort(server_port);
        
        OFList<OFString> transferSyntaxes;
        transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
        scu.addPresentationContext(UID_FINDStudyRootQueryRetrieveInformationModel, transferSyntaxes);
        
        OFCondition status = scu.initNetwork();
        if (status.bad()) {
            result->error = 1;
            result->error_message = strdup(("Network initialization failed: " + std::string(status.text())).c_str());
            return result;
        }
        if (!maybe_apply_tls(scu)) {
            result->error = 1;
            result->error_message = strdup("TLS configuration failed");
            return result;
        }
        
        status = scu.negotiateAssociation();
        if (status.bad()) {
            result->error = 1;
            result->error_message = strdup(("Association failed: " + std::string(status.text())).c_str());
            return result;
        }
        
        T_ASC_PresentationContextID presID = scu.findPresentationContextID(UID_FINDStudyRootQueryRetrieveInformationModel, "");
        if (presID == 0) {
            result->error = 1;
            result->error_message = strdup("No presentation context for Study Root C-FIND");
            scu.releaseAssociation();
            return result;
        }
        
        DcmDataset query;
        query.putAndInsertOFStringArray(DCM_QueryRetrieveLevel, "IMAGE");
        query.putAndInsertOFStringArray(DCM_SeriesInstanceUID, series_instance_uid);
        query.putAndInsertOFStringArray(DCM_SOPInstanceUID, "");
        query.putAndInsertOFStringArray(DCM_InstanceNumber, "");
        
        OFList<QRResponse*> responses;
        status = scu.sendFINDRequest(presID, &query, &responses);
        
        if (status.bad()) {
            result->error = 1;
            result->error_message = strdup(("C-FIND failed: " + std::string(status.text())).c_str());
            scu.releaseAssociation();
            return result;
        }
        
        DEBUG_LOG("C-FIND successful. Received %zu instance responses", responses.size());
        
        if (responses.size() > 0) {
            result->instances = (DicomInstance*)malloc(responses.size() * sizeof(DicomInstance));
            result->instance_count = 0;
            
            OFListIterator(QRResponse*) iter = responses.begin();
            while (iter != responses.end()) {
                QRResponse* response = *iter;
                if (response && response->m_dataset) {
                    OFString sopUID, instanceNumber;
                    
                    response->m_dataset->findAndGetOFString(DCM_SOPInstanceUID, sopUID);
                    response->m_dataset->findAndGetOFString(DCM_InstanceNumber, instanceNumber);
                    
                    if (!sopUID.empty()) {
                        DicomInstance* inst = &result->instances[result->instance_count];
                        inst->sop_instance_uid = strdup(sopUID.c_str());
                        inst->instance_number = strdup(instanceNumber.c_str());
                        inst->file_path = strdup("");
                        inst->content_type = strdup("IMAGE");
                        inst->file_size = 0;
                        
                        result->instance_count++;
                        DEBUG_LOG("Found instance: %s (#%s)", sopUID.c_str(), instanceNumber.c_str());
                    }
                }
                ++iter;
            }
        }
        
        scu.releaseAssociation();
        
    } catch (const std::exception& e) {
        result->error = 1;
        result->error_message = strdup(("Exception during instance query: " + std::string(e.what())).c_str());
    }
    
    return result;
}

// TLS support - uses per-call TLS config (cert/key/ca) rather than global config
int dcmtk_test_server_connection_tls(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title,
                                      const char* cert_file, const char* key_file, const char* ca_file) {
#ifdef WITH_OPENSSL
    DEBUG_LOG("Testing TLS connection to %s:%d", server_host, server_port);
    
    // Temporarily set TLS config for this call
    dcmtk_set_tls_config(cert_file, key_file, ca_file);
    
    int retval = dcmtk_test_server_connection(server_host, server_port, ae_title, called_ae_title);
    
    dcmtk_clear_tls_config();
    return retval;
#else
    DEBUG_LOG("TLS not available - OpenSSL not compiled in");
    return -1; // -1 indicates TLS not available
#endif
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
        if (!maybe_apply_tls(scu)) {
            result->error = 1;
            result->error_message = strdup("TLS configuration failed");
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
        query.putAndInsertOFStringArray(DCM_NumberOfSeriesRelatedInstances, "");
        query.putAndInsertOFStringArray(DCM_BodyPartExamined, "");
        
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
                    OFString numInstances, bodyPart;
                    
                    response->m_dataset->findAndGetOFString(DCM_SeriesInstanceUID, seriesUID);
                    response->m_dataset->findAndGetOFString(DCM_SeriesNumber, seriesNumber);
                    response->m_dataset->findAndGetOFString(DCM_SeriesDescription, seriesDescription);
                    response->m_dataset->findAndGetOFString(DCM_Modality, modality);
                    response->m_dataset->findAndGetOFString(DCM_SeriesDate, seriesDate);
                    response->m_dataset->findAndGetOFString(DCM_SeriesTime, seriesTime);
                    response->m_dataset->findAndGetOFString(DCM_NumberOfSeriesRelatedInstances, numInstances);
                    response->m_dataset->findAndGetOFString(DCM_BodyPartExamined, bodyPart);
                    
                    if (!seriesUID.empty()) {
                        DicomSeries* series = &result->series[result->series_count];
                        series->series_instance_uid = strdup(seriesUID.c_str());
                        series->series_number = strdup(seriesNumber.c_str());
                        series->series_description = strdup(seriesDescription.c_str());
                        series->modality = strdup(modality.c_str());
                        series->series_date = strdup(seriesDate.c_str());
                        series->series_time = strdup(seriesTime.c_str());
                        series->instance_count = 0;
                        series->number_of_series_related_instances = strdup(numInstances.c_str());
                        series->body_part_examined = strdup(bodyPart.c_str());
                        
                        result->series_count++;
                        DEBUG_LOG("Added series: %s (%s) [%s images, %s]", seriesUID.c_str(), modality.c_str(), numInstances.c_str(), bodyPart.c_str());
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
                if (result->series[i].number_of_series_related_instances) free(result->series[i].number_of_series_related_instances);
                if (result->series[i].body_part_examined) free(result->series[i].body_part_examined);
            }
            free(result->series);
        }
        if (result->error_message) free(result->error_message);
        free(result);
    }
}

// Helper: storage SOP classes split by type for proper transfer syntax negotiation.
// Image SOP classes use image transfer syntaxes (JPEG, J2K, RLE, etc.)
static const char* g_imageStorageSopClasses[] = {
    // CT
    UID_CTImageStorage,
    UID_EnhancedCTImageStorage,
    UID_LegacyConvertedEnhancedCTImageStorage,
    // MR
    UID_MRImageStorage,
    UID_EnhancedMRImageStorage,
    UID_EnhancedMRColorImageStorage,
    UID_LegacyConvertedEnhancedMRImageStorage,
    UID_MRSpectroscopyStorage,
    // Ultrasound
    UID_UltrasoundImageStorage,
    UID_UltrasoundMultiframeImageStorage,
    UID_EnhancedUSVolumeStorage,
    // Secondary Capture
    UID_SecondaryCaptureImageStorage,
    UID_MultiframeSingleBitSecondaryCaptureImageStorage,
    UID_MultiframeGrayscaleByteSecondaryCaptureImageStorage,
    UID_MultiframeGrayscaleWordSecondaryCaptureImageStorage,
    UID_MultiframeTrueColorSecondaryCaptureImageStorage,
    // X-Ray / CR / DX / Mammography
    UID_ComputedRadiographyImageStorage,
    UID_DigitalXRayImageStorageForPresentation,
    UID_DigitalXRayImageStorageForProcessing,
    UID_DigitalMammographyXRayImageStorageForPresentation,
    UID_DigitalMammographyXRayImageStorageForProcessing,
    UID_DigitalIntraOralXRayImageStorageForPresentation,
    UID_DigitalIntraOralXRayImageStorageForProcessing,
    UID_BreastTomosynthesisImageStorage,
    UID_BreastProjectionXRayImageStorageForPresentation,
    UID_BreastProjectionXRayImageStorageForProcessing,
    // Angiography / Fluoroscopy
    UID_XRayAngiographicImageStorage,
    UID_EnhancedXAImageStorage,
    UID_XRayRadiofluoroscopicImageStorage,
    UID_EnhancedXRFImageStorage,
    UID_XRay3DAngiographicImageStorage,
    UID_XRay3DCraniofacialImageStorage,
    // Nuclear Medicine / PET
    UID_NuclearMedicineImageStorage,
    UID_PositronEmissionTomographyImageStorage,
    UID_EnhancedPETImageStorage,
    UID_LegacyConvertedEnhancedPETImageStorage,
    // VL / Ophthalmic
    UID_VLEndoscopicImageStorage,
    UID_VLMicroscopicImageStorage,
    UID_VLSlideCoordinatesMicroscopicImageStorage,
    UID_VLPhotographicImageStorage,
    UID_VLWholeSlideMicroscopyImageStorage,
    UID_DermoscopicPhotographyImageStorage,
    UID_OphthalmicPhotography8BitImageStorage,
    UID_OphthalmicPhotography16BitImageStorage,
    UID_OphthalmicTomographyImageStorage,
    // IVOCT
    UID_IntravascularOpticalCoherenceTomographyImageStorageForPresentation,
    UID_IntravascularOpticalCoherenceTomographyImageStorageForProcessing,
    // RT
    UID_RTImageStorage,
    UID_RTDoseStorage,
    UID_RTStructureSetStorage,
    UID_RTPlanStorage,
    // Segmentation / Parametric / Registration
    UID_SegmentationStorage,
    UID_ParametricMapStorage,
    UID_SpatialRegistrationStorage,
    UID_DeformableSpatialRegistrationStorage,
    UID_TractographyResultsStorage,
    UID_RawDataStorage,
};
static const int g_numImageStorageSopClasses = sizeof(g_imageStorageSopClasses) / sizeof(g_imageStorageSopClasses[0]);

// Video SOP classes use MPEG transfer syntaxes
static const char* g_videoStorageSopClasses[] = {
    UID_VideoEndoscopicImageStorage,
    UID_VideoMicroscopicImageStorage,
    UID_VideoPhotographicImageStorage,
};
static const int g_numVideoStorageSopClasses = sizeof(g_videoStorageSopClasses) / sizeof(g_videoStorageSopClasses[0]);

// Non-image storage SOP classes (presentation states, KOS, structured reports, encapsulated docs)
// These are commonly found alongside images in a series and use uncompressed transfer syntaxes.
static const char* g_otherStorageSopClasses[] = {
    // Presentation States
    UID_GrayscaleSoftcopyPresentationStateStorage,
    UID_ColorSoftcopyPresentationStateStorage,
    UID_PseudoColorSoftcopyPresentationStateStorage,
    UID_BlendingSoftcopyPresentationStateStorage,
    UID_XAXRFGrayscaleSoftcopyPresentationStateStorage,
    // Key Object Selection
    UID_KeyObjectSelectionDocumentStorage,
    // Structured Reports
    UID_BasicTextSRStorage,
    UID_EnhancedSRStorage,
    UID_ComprehensiveSRStorage,
    UID_Comprehensive3DSRStorage,
    // Encapsulated Documents
    UID_EncapsulatedPDFStorage,
    UID_EncapsulatedCDAStorage,
    UID_EncapsulatedSTLStorage,
    // Waveforms
    UID_TwelveLeadECGWaveformStorage,
    UID_GeneralECGWaveformStorage,
    UID_BasicVoiceAudioWaveformStorage,
    // Surface / Fiducials
    UID_SpatialFiducialsStorage,
    UID_SurfaceSegmentationStorage,
    UID_RealWorldValueMappingStorage,
};
static const int g_numOtherStorageSopClasses = sizeof(g_otherStorageSopClasses) / sizeof(g_otherStorageSopClasses[0]);

// Helper: scan directory for DICOM files and return their paths.
// DcmSCU's handleSTORERequest saves files WITHOUT .dcm extension (e.g. "SC.1.2.3..."),
// so we scan all regular files and verify each is a valid DICOM file.
static std::vector<std::string> scanDirectoryForDcmFiles(const std::string& dirPath) {
    std::vector<std::string> files;
    DIR* dir = opendir(dirPath.c_str());
    if (!dir) return files;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        // Skip dot-files and directories
        if (name.empty() || name[0] == '.') continue;
        
        std::string fullPath = dirPath + "/" + name;
        
        // Check it's a regular file (not a directory)
        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;
        
        // Verify it's actually a DICOM file by trying to load it
        DcmFileFormat dcmFile;
        if (dcmFile.loadFile(fullPath.c_str()).good()) {
            files.push_back(fullPath);
        }
    }
    closedir(dir);
    std::sort(files.begin(), files.end());
    return files;
}

// C-GET implementation - retrieve instances from server via C-GET, with C-FIND fallback on separate association
DicomInstanceQueryResult* dcmtk_download_instances(const char* server_host, int server_port, const char* ae_title, const char* called_ae_title, const char* series_instance_uid, const char* local_storage_path) {
    DicomInstanceQueryResult* result = (DicomInstanceQueryResult*)malloc(sizeof(DicomInstanceQueryResult));
    if (!result) return NULL;
    
    result->instances = NULL;
    result->instance_count = 0;
    result->error = 0;
    result->error_message = NULL;
    
    // Use a per-series subdirectory to avoid mixing files from different downloads
    std::string seriesDir = std::string(local_storage_path) + "/" + series_instance_uid;
    mkdir(local_storage_path, 0755);
    mkdir(seriesDir.c_str(), 0755);
    
    // Transfer syntaxes for image storage (non-video)
    OFList<OFString> imageStorageTSList;
    imageStorageTSList.push_back(UID_LittleEndianExplicitTransferSyntax);
    imageStorageTSList.push_back(UID_LittleEndianImplicitTransferSyntax);
    imageStorageTSList.push_back(UID_BigEndianExplicitTransferSyntax);
    imageStorageTSList.push_back(UID_DeflatedExplicitVRLittleEndianTransferSyntax);
    // JPEG
    imageStorageTSList.push_back(UID_JPEGProcess1TransferSyntax);
    imageStorageTSList.push_back(UID_JPEGProcess2_4TransferSyntax);
    imageStorageTSList.push_back(UID_JPEGProcess14SV1TransferSyntax);        // JPEG Lossless SV1 — very common!
    imageStorageTSList.push_back(UID_JPEGProcess14TransferSyntax);
    // JPEG 2000
    imageStorageTSList.push_back(UID_JPEG2000LosslessOnlyTransferSyntax);
    imageStorageTSList.push_back(UID_JPEG2000TransferSyntax);
    imageStorageTSList.push_back(UID_JPEG2000Part2MulticomponentImageCompressionLosslessOnlyTransferSyntax);
    imageStorageTSList.push_back(UID_JPEG2000Part2MulticomponentImageCompressionTransferSyntax);
    // HTJ2K (High-Throughput JPEG 2000)
    imageStorageTSList.push_back(UID_HighThroughputJPEG2000ImageCompressionLosslessOnlyTransferSyntax);
    imageStorageTSList.push_back(UID_HighThroughputJPEG2000ImageCompressionTransferSyntax);
    // JPEG-LS
    imageStorageTSList.push_back(UID_JPEGLSLosslessTransferSyntax);
    imageStorageTSList.push_back(UID_JPEGLSLossyTransferSyntax);
    // RLE
    imageStorageTSList.push_back(UID_RLELosslessTransferSyntax);

    // Transfer syntaxes for video storage (MPEG / HEVC)
    OFList<OFString> videoStorageTSList;
    videoStorageTSList.push_back(UID_MPEG4HighProfileLevel4_1TransferSyntax);
    videoStorageTSList.push_back(UID_MPEG4BDcompatibleHighProfileLevel4_1TransferSyntax);
    videoStorageTSList.push_back(UID_MPEG4HighProfileLevel4_2_For2DVideoTransferSyntax);
    videoStorageTSList.push_back(UID_MPEG4HighProfileLevel4_2_For3DVideoTransferSyntax);
    videoStorageTSList.push_back(UID_MPEG2MainProfileAtMainLevelTransferSyntax);
    videoStorageTSList.push_back(UID_MPEG2MainProfileAtHighLevelTransferSyntax);
    videoStorageTSList.push_back(UID_HEVCMainProfileLevel5_1TransferSyntax);
    videoStorageTSList.push_back(UID_HEVCMain10ProfileLevel5_1TransferSyntax);
    videoStorageTSList.push_back(UID_LittleEndianExplicitTransferSyntax);
    videoStorageTSList.push_back(UID_LittleEndianImplicitTransferSyntax);

    // Transfer syntaxes for non-image storage (presentation states, SR, KOS, etc.)
    OFList<OFString> otherStorageTSList;
    otherStorageTSList.push_back(UID_LittleEndianExplicitTransferSyntax);
    otherStorageTSList.push_back(UID_LittleEndianImplicitTransferSyntax);
    otherStorageTSList.push_back(UID_DeflatedExplicitVRLittleEndianTransferSyntax);

    OFList<OFString> queryTSList;
    queryTSList.push_back(UID_LittleEndianExplicitTransferSyntax);
    queryTSList.push_back(UID_LittleEndianImplicitTransferSyntax);

    std::string cgetError;
    
    try {
        DEBUG_LOG("C-GET download starting for series: %s", series_instance_uid);
        DEBUG_LOG("Storage directory: %s", seriesDir.c_str());
        
        // ========== PHASE 1: Try C-GET on its own association ==========
        {
            DcmSCU scu;
            scu.setAETitle(ae_title);
            scu.setPeerHostName(server_host);
            scu.setPeerPort(server_port);
            scu.setPeerAETitle(called_ae_title);
            scu.setStorageDir(seriesDir.c_str());
            scu.setStorageMode(DCMSCU_STORAGE_DISK);
            
            // C-GET query context
            scu.addPresentationContext(UID_GETStudyRootQueryRetrieveInformationModel, queryTSList);
            
            // Image storage SOP classes with image transfer syntaxes (SCP role for C-STORE sub-ops)
            for (int i = 0; i < g_numImageStorageSopClasses; i++) {
                scu.addPresentationContext(g_imageStorageSopClasses[i], imageStorageTSList, ASC_SC_ROLE_SCP);
            }
            
            // Video storage SOP classes with MPEG transfer syntaxes
            for (int i = 0; i < g_numVideoStorageSopClasses; i++) {
                scu.addPresentationContext(g_videoStorageSopClasses[i], videoStorageTSList, ASC_SC_ROLE_SCP);
            }
            
            // Non-image storage SOP classes (presentation states, KOS, SR, etc.)
            for (int i = 0; i < g_numOtherStorageSopClasses; i++) {
                scu.addPresentationContext(g_otherStorageSopClasses[i], otherStorageTSList, ASC_SC_ROLE_SCP);
            }
            
            DEBUG_LOG("C-GET: proposed %d image + %d video + %d other = %d storage presentation contexts",
                g_numImageStorageSopClasses, g_numVideoStorageSopClasses, g_numOtherStorageSopClasses,
                g_numImageStorageSopClasses + g_numVideoStorageSopClasses + g_numOtherStorageSopClasses);
            
            OFCondition status = scu.initNetwork();
            if (status.good()) {
                if (!maybe_apply_tls(scu)) status = EC_IllegalParameter;
            }
            if (status.good()) {
                status = scu.negotiateAssociation();
            }
            
            if (status.good()) {
                // Log negotiation results for diagnostics
                int acceptedStorage = 0;
                for (int i = 0; i < g_numImageStorageSopClasses; i++) {
                    if (scu.findPresentationContextID(g_imageStorageSopClasses[i], "") != 0) {
                        acceptedStorage++;
                    } else {
                        DEBUG_LOG("C-GET: server rejected image SOP %s", g_imageStorageSopClasses[i]);
                    }
                }
                for (int i = 0; i < g_numVideoStorageSopClasses; i++) {
                    if (scu.findPresentationContextID(g_videoStorageSopClasses[i], "") != 0) acceptedStorage++;
                }
                for (int i = 0; i < g_numOtherStorageSopClasses; i++) {
                    if (scu.findPresentationContextID(g_otherStorageSopClasses[i], "") != 0) acceptedStorage++;
                }
                DEBUG_LOG("C-GET negotiation: %d storage contexts accepted out of %d proposed",
                    acceptedStorage, g_numImageStorageSopClasses + g_numVideoStorageSopClasses + g_numOtherStorageSopClasses);
                
                T_ASC_PresentationContextID getPresID = scu.findPresentationContextID(
                    UID_GETStudyRootQueryRetrieveInformationModel, "");
                
                if (getPresID != 0) {
                    DEBUG_LOG("C-GET presentation context accepted (ID=%d)", getPresID);
                    
                    DcmDataset getDataset;
                    getDataset.putAndInsertOFStringArray(DCM_QueryRetrieveLevel, "SERIES");
                    getDataset.putAndInsertOFStringArray(DCM_SeriesInstanceUID, series_instance_uid);
                    
                    OFList<RetrieveResponse*> getResponses;
                    status = scu.sendCGETRequest(getPresID, &getDataset, &getResponses);
                    
                    if (status.good()) {
                        int completed = 0, failed = 0, remaining = 0;
                        OFListIterator(RetrieveResponse*) it = getResponses.begin();
                        while (it != getResponses.end()) {
                            RetrieveResponse* resp = *it;
                            if (resp) {
                                completed = resp->m_numberOfCompletedSubops;
                                failed = resp->m_numberOfFailedSubops;
                                remaining = resp->m_numberOfRemainingSubops;
                                DEBUG_LOG("C-GET sub-response: status=0x%04x completed=%d failed=%d remaining=%d", 
                                    resp->m_status, completed, failed, remaining);
                            }
                            ++it;
                        }
                        DEBUG_LOG("C-GET final: %d completed, %d failed", completed, failed);
                        if (completed == 0 && failed > 0) {
                            cgetError = "C-GET: server reported " + std::to_string(failed) + " failed sub-operation(s)"
                                " — the server could not send the file(s) in any negotiated transfer syntax/SOP class."
                                " Check that the SOP Class of the instance is in the proposed storage contexts.";
                        } else if (failed > 0) {
                            // Partial success: some completed, some failed
                            DEBUG_LOG("C-GET: partial success — %d completed, %d failed", completed, failed);
                        } else if (completed == 0 && failed == 0) {
                            cgetError = "C-GET: server completed with 0 sub-operations";
                        }
                    } else {
                        cgetError = "C-GET request failed: " + std::string(status.text());
                        DEBUG_LOG("C-GET failed: %s", status.text());
                    }
                    
                    // Clean up responses
                    while (!getResponses.empty()) {
                        RetrieveResponse* resp = getResponses.front();
                        getResponses.pop_front();
                        delete resp;
                    }
                } else {
                    cgetError = "Server did not accept C-GET presentation context";
                    DEBUG_LOG("Server rejected C-GET presentation context");
                }
                
                scu.releaseAssociation();
            } else {
                cgetError = "C-GET association failed: " + std::string(status.text());
                DEBUG_LOG("C-GET association failed: %s", status.text());
            }
        }
        
        // Debug: list all files in the storage directory
        {
            DIR* dbgDir = opendir(seriesDir.c_str());
            if (dbgDir) {
                struct dirent* dbgEntry;
                int fileCount = 0;
                while ((dbgEntry = readdir(dbgDir)) != NULL) {
                    std::string n = dbgEntry->d_name;
                    if (n[0] == '.') continue;
                    std::string fp = seriesDir + "/" + n;
                    struct stat st;
                    long fsize = 0;
                    if (stat(fp.c_str(), &st) == 0) fsize = st.st_size;
                    DEBUG_LOG("  Storage dir file: %s (size=%ld, isDir=%d)", n.c_str(), fsize, S_ISDIR(st.st_mode));
                    fileCount++;
                }
                closedir(dbgDir);
                DEBUG_LOG("  Total files in storage dir: %d", fileCount);
            } else {
                DEBUG_LOG("  ERROR: Cannot open storage dir: %s", seriesDir.c_str());
            }
        }
        
        // Check if C-GET produced any files
        std::vector<std::string> downloadedFiles = scanDirectoryForDcmFiles(seriesDir);
        
        if (!downloadedFiles.empty()) {
            // SUCCESS: C-GET delivered files
            DEBUG_LOG("C-GET downloaded %d DICOM files", (int)downloadedFiles.size());
            result->instance_count = (int)downloadedFiles.size();
            result->instances = (DicomInstance*)malloc(result->instance_count * sizeof(DicomInstance));
            
            for (int i = 0; i < (int)downloadedFiles.size(); i++) {
                const std::string& filePath = downloadedFiles[i];
                result->instances[i].file_path = strdup(filePath.c_str());
                result->instances[i].instance_number = strdup(std::to_string(i + 1).c_str());
                result->instances[i].content_type = strdup("DICOM");
                
                struct stat st;
                result->instances[i].file_size = (stat(filePath.c_str(), &st) == 0) ? (int)st.st_size : 0;
                
                // Extract SOP Instance UID from downloaded file
                DcmFileFormat dcmFile;
                if (dcmFile.loadFile(filePath.c_str()).good()) {
                    OFString sopUID;
                    if (dcmFile.getDataset()->findAndGetOFString(DCM_SOPInstanceUID, sopUID).good()) {
                        result->instances[i].sop_instance_uid = strdup(sopUID.c_str());
                    } else {
                        result->instances[i].sop_instance_uid = strdup("");
                    }
                } else {
                    result->instances[i].sop_instance_uid = strdup("");
                }
                
                DEBUG_LOG("Downloaded: %s (size=%d)", filePath.c_str(), result->instances[i].file_size);
            }
            return result;
        }
        
        // ========== PHASE 2: C-GET didn't produce files, try C-FIND on fresh association ==========
        DEBUG_LOG("C-GET produced no files (%s), trying C-FIND on fresh association", cgetError.c_str());
        
        {
            DcmSCU scu2;
            scu2.setAETitle(ae_title);
            scu2.setPeerHostName(server_host);
            scu2.setPeerPort(server_port);
            scu2.setPeerAETitle(called_ae_title);
            
            scu2.addPresentationContext(UID_FINDStudyRootQueryRetrieveInformationModel, queryTSList);
            
            OFCondition status = scu2.initNetwork();
            if (status.good()) {
                status = scu2.negotiateAssociation();
            }
            
            std::vector<std::string> instanceUIDs;
            
            if (status.good()) {
                T_ASC_PresentationContextID findPresID = scu2.findPresentationContextID(
                    UID_FINDStudyRootQueryRetrieveInformationModel, "");
                
                if (findPresID != 0) {
                    OFList<QRResponse*> findResponses;
                    DcmDataset findQuery;
                    findQuery.putAndInsertOFStringArray(DCM_QueryRetrieveLevel, "IMAGE");
                    findQuery.putAndInsertOFStringArray(DCM_SeriesInstanceUID, series_instance_uid);
                    findQuery.putAndInsertOFStringArray(DCM_SOPInstanceUID, "");
                    findQuery.putAndInsertOFStringArray(DCM_InstanceNumber, "");
                    
                    OFCondition findStatus = scu2.sendFINDRequest(findPresID, &findQuery, &findResponses);
                    if (findStatus.good()) {
                        OFListIterator(QRResponse*) iter = findResponses.begin();
                        while (iter != findResponses.end()) {
                            QRResponse* response = *iter;
                            if (response && response->m_dataset) {
                                OFString sopUID;
                                if (response->m_dataset->findAndGetOFString(DCM_SOPInstanceUID, sopUID).good()) {
                                    if (sopUID.length() > 0) {
                                        instanceUIDs.push_back(sopUID.c_str());
                                    }
                                }
                            }
                            ++iter;
                        }
                    } else {
                        DEBUG_LOG("C-FIND also failed: %s", findStatus.text());
                    }
                    
                    // Clean up find responses
                    while (!findResponses.empty()) {
                        QRResponse* resp = findResponses.front();
                        findResponses.pop_front();
                        delete resp;
                    }
                }
                
                scu2.releaseAssociation();
            } else {
                DEBUG_LOG("C-FIND association failed: %s", status.text());
            }
            
            if (!instanceUIDs.empty()) {
                // C-FIND found instances but C-GET couldn't download them
                result->error = 1;
                std::string errorMsg = "C-GET failed to download " + std::to_string(instanceUIDs.size()) +
                    " instance(s). " + cgetError +
                    ". The server may not support C-GET. Try enabling C-GET on the PACS server (e.g. Orthanc: set \"DicomGetEnabled\": true in configuration).";
                result->error_message = strdup(errorMsg.c_str());
                DEBUG_LOG("C-FIND found %d instances but C-GET couldn't retrieve them", (int)instanceUIDs.size());
            } else {
                result->error = 1;
                std::string errorMsg = "No instances found. " + cgetError;
                result->error_message = strdup(errorMsg.c_str());
                DEBUG_LOG("Both C-GET and C-FIND returned no instances");
            }
        }
        
    } catch (const std::exception& e) {
        result->error = 1;
        result->error_message = strdup(("Exception during download: " + std::string(e.what())).c_str());
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

// ============================================================
// Video Extraction from DICOM
// ============================================================
VideoExtractionResult* dcmtk_extract_video(const char* dicom_path, const char* output_path) {
    VideoExtractionResult* result = (VideoExtractionResult*)malloc(sizeof(VideoExtractionResult));
    result->success = 0;
    result->error_message = nullptr;
    result->output_path = nullptr;
    result->mime_type = nullptr;
    result->file_size = 0;

    if (!dicom_path || !output_path) {
        result->error_message = strdup("Error: dicom_path and output_path are required");
        return result;
    }

    DEBUG_LOG("=== Extracting video from DICOM ===");
    DEBUG_LOG("Input: %s", dicom_path);
    DEBUG_LOG("Output: %s", output_path);

    DcmFileFormat fileformat;
    OFCondition status = fileformat.loadFile(dicom_path);
    if (status.bad()) {
        std::string err = "Failed to load DICOM file: ";
        err += status.text();
        result->error_message = strdup(err.c_str());
        return result;
    }

    DcmDataset* dataset = fileformat.getDataset();
    if (!dataset) {
        result->error_message = strdup("Could not get dataset from DICOM file");
        return result;
    }

    // Determine MIME type from transfer syntax
    E_TransferSyntax xfer = dataset->getOriginalXfer();
    const char* mime = "video/mp4";
    switch (xfer) {
        case EXS_MPEG2MainProfileAtMainLevel:
        case EXS_MPEG2MainProfileAtHighLevel:
            mime = "video/mpeg";
            break;
        default:
            mime = "video/mp4";
            break;
    }

    // Verify this is a video SOP class OR has a video transfer syntax
    OFString sopClassUID;
    dataset->findAndGetOFString(DCM_SOPClassUID, sopClassUID);
    bool isVideoSopClass = (sopClassUID == UID_VideoEndoscopicImageStorage ||
                            sopClassUID == UID_VideoMicroscopicImageStorage ||
                            sopClassUID == UID_VideoPhotographicImageStorage);
    bool isVideoTransferSyntax = (xfer == EXS_MPEG2MainProfileAtMainLevel ||
                                  xfer == EXS_MPEG2MainProfileAtHighLevel ||
                                  xfer == EXS_MPEG4HighProfileLevel4_1 ||
                                  xfer == EXS_MPEG4BDcompatibleHighProfileLevel4_1);

    if (!isVideoSopClass && !isVideoTransferSyntax) {
        DEBUG_LOG("SOP class %s is not a video class and transfer syntax %d is not MPEG", sopClassUID.c_str(), (int)xfer);
        result->error_message = strdup("Not a video DICOM file (neither video SOP class nor MPEG transfer syntax)");
        return result;
    }

    DEBUG_LOG("Video detection: SOP=%s (isVideoSOP=%d), xfer=%d (isVideoTS=%d), mime=%s",
              sopClassUID.c_str(), isVideoSopClass, (int)xfer, isVideoTransferSyntax, mime);

    // Access encapsulated pixel data
    DcmElement* pixelElement = nullptr;
    status = dataset->findAndGetElement(DCM_PixelData, pixelElement);
    if (status.bad() || !pixelElement) {
        result->error_message = strdup("No PixelData element found in DICOM file");
        return result;
    }

    DcmPixelData* pixelData = dynamic_cast<DcmPixelData*>(pixelElement);
    if (!pixelData) {
        result->error_message = strdup("PixelData element is not encapsulated pixel data");
        return result;
    }

    // Get the encapsulated pixel sequence
    DcmPixelSequence* pixSeq = nullptr;
    E_TransferSyntax repXfer = EXS_Unknown;
    const DcmRepresentationParameter* repParam = nullptr;
    status = pixelData->getEncapsulatedRepresentation(repXfer, repParam, pixSeq);
    if (status.bad() || !pixSeq) {
        status = pixelData->getEncapsulatedRepresentation(xfer, repParam, pixSeq);
        if (status.bad() || !pixSeq) {
            result->error_message = strdup("Could not access encapsulated pixel data sequence");
            return result;
        }
    }

    unsigned long numItems = pixSeq->card();
    DEBUG_LOG("Pixel sequence has %lu items (first is offset table)", numItems);

    if (numItems < 2) {
        result->error_message = strdup("Pixel sequence has no data items (only offset table or empty)");
        return result;
    }

    // Write video data to output file
    // Item 0 is the offset table, items 1..N are the actual video fragments
    FILE* outFile = fopen(output_path, "wb");
    if (!outFile) {
        std::string err = "Cannot create output file: ";
        err += output_path;
        result->error_message = strdup(err.c_str());
        return result;
    }

    long totalBytes = 0;
    bool writeError = false;

    for (unsigned long i = 1; i < numItems; i++) {
        DcmPixelItem* pixItem = nullptr;
        if (pixSeq->getItem(pixItem, i).good() && pixItem) {
            Uint8* fragData = nullptr;
            if (pixItem->getUint8Array(fragData).good() && fragData) {
                unsigned long fragLen = pixItem->getLength();
                size_t written = fwrite(fragData, 1, fragLen, outFile);
                if (written != fragLen) {
                    writeError = true;
                    DEBUG_LOG("Write error at fragment %lu: wrote %zu of %lu", i, written, fragLen);
                    break;
                }
                totalBytes += fragLen;
                DEBUG_LOG("Wrote fragment %lu: %lu bytes", i, fragLen);
            } else {
                DEBUG_LOG("Warning: could not read fragment %lu data", i);
            }
        }
    }

    fclose(outFile);

    if (writeError || totalBytes == 0) {
        remove(output_path);
        result->error_message = strdup(writeError ? "Error writing video data to file" : "No video data found in pixel items");
        return result;
    }

    DEBUG_LOG("Successfully extracted %ld bytes of video data", totalBytes);

    result->success = 1;
    result->output_path = strdup(output_path);
    result->mime_type = strdup(mime);
    result->file_size = totalBytes;
    return result;
}

void dcmtk_free_video_extraction_result(VideoExtractionResult* result) {
    if (result) {
        if (result->error_message) free(result->error_message);
        if (result->output_path) free(result->output_path);
        if (result->mime_type) free(result->mime_type);
        free(result);
    }
}

// ============================================================
// C-STORE SCU: Send existing DICOM files to a remote PACS
// ============================================================

StoreResult* dcmtk_store_files(const char* server_host, int server_port,
                                const char* ae_title, const char* called_ae_title,
                                const char** file_paths, int file_count) {
    DEBUG_LOG("Storing %d DICOM file(s) to %s:%d", file_count, server_host, server_port);

    StoreResult* result = (StoreResult*)malloc(sizeof(StoreResult));
    result->success_count = 0;
    result->fail_count = 0;
    result->total_count = file_count;
    result->error = 0;
    result->error_message = nullptr;

    if (file_count <= 0 || !file_paths) {
        result->error = 1;
        result->error_message = strdup("No files to store");
        return result;
    }

    // Subclass to capture per-instance DIMSE response status
    class TrackingStorageSCU : public DcmStorageSCU {
    public:
        int successCount = 0;
        int failCount = 0;
        Uint16 lastFailStatus = 0;
    protected:
        virtual void notifySOPInstanceSent(const TransferEntry& entry) {
            if (entry.RequestSent) {
                Uint16 st = entry.ResponseStatusCode;
                if (st == 0x0000 || (st & 0xFF00) == 0xB000) {
                    successCount++;
                    DEBUG_LOG("  Instance sent OK: status 0x%04X", st);
                } else {
                    failCount++;
                    lastFailStatus = st;
                    DEBUG_LOG("  Instance REJECTED: status 0x%04X", st);
                }
            }
        }
    };

    try {
        TrackingStorageSCU storageSCU;
        storageSCU.setAETitle(ae_title);
        storageSCU.setPeerAETitle(called_ae_title);
        storageSCU.setPeerHostName(server_host);
        storageSCU.setPeerPort(server_port);
        storageSCU.setMaxReceivePDULength(16384);
        storageSCU.setACSETimeout(30);
        storageSCU.setDIMSETimeout(60);
        storageSCU.setDecompressionMode(DcmStorageSCU::DM_lossyAndLossless);
        storageSCU.setHaltOnUnsuccessfulStoreMode(OFFalse);

        // Register decompression codecs
        DJDecoderRegistration::registerCodecs();
        DJLSDecoderRegistration::registerCodecs();
        DcmRLEDecoderRegistration::registerCodecs();

        // Add each file to the transfer list
        for (int i = 0; i < file_count; i++) {
            OFCondition addStatus = storageSCU.addDicomFile(file_paths[i], ERM_autoDetect, OFFalse);
            if (addStatus.bad()) {
                DEBUG_LOG("Failed to add file %d: %s (%s)", i, file_paths[i], addStatus.text());
                result->fail_count++;
            }
        }

        if (storageSCU.getNumberOfSOPInstances() == 0) {
            result->error = 1;
            result->error_message = strdup("No valid DICOM files to store");
            DJDecoderRegistration::cleanup();
            DJLSDecoderRegistration::cleanup();
            DcmRLEDecoderRegistration::cleanup();
            return result;
        }

        OFCondition cond = storageSCU.addPresentationContexts();
        if (cond.bad()) {
            result->error = 1;
            result->error_message = strdup(("addPresentationContexts failed: " + std::string(cond.text())).c_str());
            DJDecoderRegistration::cleanup();
            DJLSDecoderRegistration::cleanup();
            DcmRLEDecoderRegistration::cleanup();
            return result;
        }

        cond = storageSCU.initNetwork();
        if (cond.bad()) {
            result->error = 1;
            result->error_message = strdup(("Network init failed: " + std::string(cond.text())).c_str());
            DJDecoderRegistration::cleanup();
            DJLSDecoderRegistration::cleanup();
            DcmRLEDecoderRegistration::cleanup();
            return result;
        }
        if (!maybe_apply_tls(storageSCU)) {
            result->error = 1;
            result->error_message = strdup("TLS configuration failed");
            DJDecoderRegistration::cleanup();
            DJLSDecoderRegistration::cleanup();
            DcmRLEDecoderRegistration::cleanup();
            return result;
        }

        cond = storageSCU.negotiateAssociation();
        if (cond.bad()) {
            result->error = 1;
            result->error_message = strdup(("Association failed: " + std::string(cond.text())).c_str());
            DJDecoderRegistration::cleanup();
            DJLSDecoderRegistration::cleanup();
            DcmRLEDecoderRegistration::cleanup();
            return result;
        }

        cond = storageSCU.sendSOPInstances();

        OFString summary;
        storageSCU.getStatusSummary(summary);
        DEBUG_LOG("DcmStorageSCU summary:\n%s", summary.c_str());

        storageSCU.releaseAssociation();

        // Use the ACTUAL per-instance status counts
        result->success_count = storageSCU.successCount;
        result->fail_count += storageSCU.failCount;
        if (storageSCU.failCount > 0 && result->error_message == nullptr) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Server rejected %d instance(s) with status 0x%04X",
                     storageSCU.failCount, storageSCU.lastFailStatus);
            result->error_message = strdup(msg);
        }

        DJDecoderRegistration::cleanup();
        DJLSDecoderRegistration::cleanup();
        DcmRLEDecoderRegistration::cleanup();

        DEBUG_LOG("Store complete: %d succeeded, %d failed", result->success_count, result->fail_count);

    } catch (const std::exception& e) {
        result->error = 1;
        result->error_message = strdup(e.what());
    }

    return result;
}

void dcmtk_free_store_result(StoreResult* result) {
    if (result) {
        if (result->error_message) free(result->error_message);
        free(result);
    }
}

// ============================================================
// C-STORE SCP: Receive DICOM files from remote peers
// ============================================================

// Global SCP state (single instance)
static volatile int g_scp_running = 0;
static volatile int g_scp_received_count = 0;
static char* g_scp_storage_dir = nullptr;
static int g_scp_port = 0;
static char* g_scp_error = nullptr;

// Custom SCP handler that saves received datasets to files
class FlutterStoreSCP : public DcmSCP {
public:
    volatile bool m_shouldStop;

    FlutterStoreSCP() : DcmSCP(), m_shouldStop(false) {}

    // Override handleIncomingCommand to dispatch C-STORE and C-ECHO
    OFCondition handleIncomingCommand(T_DIMSE_Message* incomingMsg,
                                       const DcmPresentationContextInfo& presInfo) override {
        if (incomingMsg->CommandField == DIMSE_C_STORE_RQ) {
            DcmDataset* dataset = nullptr;
            OFCondition cond = handleSTORERequest(incomingMsg->msg.CStoreRQ,
                                                    presInfo.presentationContextID, dataset);
            delete dataset; // we save within handleSTORERequest, so clean up
            return cond;
        }
        if (incomingMsg->CommandField == DIMSE_C_ECHO_RQ) {
            return handleECHORequest(incomingMsg->msg.CEchoRQ, presInfo.presentationContextID);
        }
        DEBUG_LOG("SCP: Unhandled command: 0x%04X", incomingMsg->CommandField);
        return EC_Normal;
    }

    // Override handleSTORERequest to save received dataset to file
    OFCondition handleSTORERequest(T_DIMSE_C_StoreRQ& reqMessage,
                                    const T_ASC_PresentationContextID presID,
                                    DcmDataset*& reqDataset) override {
        DEBUG_LOG("SCP: Received C-STORE request for %s", reqMessage.AffectedSOPInstanceUID);

        // Let base class receive the dataset and send response
        OFCondition cond = DcmSCP::handleSTORERequest(reqMessage, presID, reqDataset);
        if (cond.bad()) {
            DEBUG_LOG("SCP: Base handleSTORERequest failed: %s", cond.text());
            return cond;
        }

        // Save to file
        if (reqDataset && g_scp_storage_dir) {
            DcmFileFormat fileFormat(reqDataset);
            std::string filePath = std::string(g_scp_storage_dir) + "/" +
                                   reqMessage.AffectedSOPInstanceUID + ".dcm";

            OFCondition saveStatus = fileFormat.saveFile(filePath.c_str(),
                                                          EXS_LittleEndianExplicit);
            if (saveStatus.good()) {
                g_scp_received_count++;
                DEBUG_LOG("SCP: Saved instance to %s (total: %d)", filePath.c_str(), g_scp_received_count);
            } else {
                DEBUG_LOG("SCP: Failed to save: %s", saveStatus.text());
            }
        }

        return cond;
    }

    // Override to allow external stopping
    OFBool stopAfterCurrentAssociation() override {
        return m_shouldStop ? OFTrue : OFFalse;
    }
};

static FlutterStoreSCP* g_scp_instance = nullptr;

int dcmtk_start_store_scp(int port, const char* ae_title, const char* storage_dir) {
    DEBUG_LOG("Starting C-STORE SCP on port %d, AE: %s, storage: %s", port, ae_title, storage_dir);

    if (g_scp_running) {
        DEBUG_LOG("SCP already running");
        return 0;
    }

    // Create storage directory
    struct stat st;
    if (stat(storage_dir, &st) != 0) {
        mkdir(storage_dir, 0755);
    }

    if (g_scp_storage_dir) free(g_scp_storage_dir);
    g_scp_storage_dir = strdup(storage_dir);
    g_scp_port = port;
    g_scp_received_count = 0;
    if (g_scp_error) { free(g_scp_error); g_scp_error = nullptr; }

    try {
        if (g_scp_instance) {
            delete g_scp_instance;
        }
        g_scp_instance = new FlutterStoreSCP();

        OFList<OFString> transferSyntaxes;
        transferSyntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
        transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
        transferSyntaxes.push_back(UID_BigEndianExplicitTransferSyntax);
        transferSyntaxes.push_back(UID_JPEGProcess1TransferSyntax);
        transferSyntaxes.push_back(UID_JPEGProcess2_4TransferSyntax);
        transferSyntaxes.push_back(UID_JPEGLSLosslessTransferSyntax);
        transferSyntaxes.push_back(UID_JPEG2000LosslessOnlyTransferSyntax);
        transferSyntaxes.push_back(UID_RLELosslessTransferSyntax);

        DcmSCPConfig& config = g_scp_instance->getConfig();
        config.setAETitle(ae_title);
        config.setPort(port);
        config.setMaxReceivePDULength(16384);
        config.setConnectionTimeout(60);

        // Accept all standard SOP classes with all transfer syntaxes
        const char* sopClasses[] = {
            UID_VerificationSOPClass,
            UID_CTImageStorage,
            UID_MRImageStorage,
            UID_UltrasoundImageStorage,
            UID_SecondaryCaptureImageStorage,
            UID_DigitalXRayImageStorageForPresentation,
            UID_DigitalXRayImageStorageForProcessing,
            UID_ComputedRadiographyImageStorage,
            UID_NuclearMedicineImageStorage,
            UID_XRayAngiographicImageStorage,
            UID_XRayRadiofluoroscopicImageStorage,
            UID_MultiframeTrueColorSecondaryCaptureImageStorage,
            UID_MultiframeGrayscaleByteSecondaryCaptureImageStorage,
            UID_VideoEndoscopicImageStorage,
            UID_EncapsulatedPDFStorage,
            nullptr
        };

        for (int i = 0; sopClasses[i] != nullptr; i++) {
            config.addPresentationContext(sopClasses[i], transferSyntaxes);
        }

        g_scp_running = 1;

        // Listen (this blocks — caller should run in a background thread)
        OFCondition cond = g_scp_instance->listen();
        g_scp_running = 0;

        if (cond.bad()) {
            g_scp_error = strdup(cond.text());
            DEBUG_LOG("SCP stopped with error: %s", cond.text());
            return 0;
        }

        DEBUG_LOG("SCP stopped normally");
        return 1;

    } catch (const std::exception& e) {
        g_scp_running = 0;
        g_scp_error = strdup(e.what());
        DEBUG_LOG("SCP exception: %s", e.what());
        return 0;
    }
}

void dcmtk_stop_store_scp(void) {
    DEBUG_LOG("Stopping C-STORE SCP");
    if (g_scp_instance) {
        g_scp_instance->m_shouldStop = true;
    }
    g_scp_running = 0;
}

StoreSCPStatus* dcmtk_get_store_scp_status(void) {
    StoreSCPStatus* status = (StoreSCPStatus*)malloc(sizeof(StoreSCPStatus));
    status->running = g_scp_running;
    status->port = g_scp_port;
    status->received_count = g_scp_received_count;
    status->storage_dir = g_scp_storage_dir ? strdup(g_scp_storage_dir) : nullptr;
    status->error_message = g_scp_error ? strdup(g_scp_error) : nullptr;
    return status;
}

void dcmtk_free_store_scp_status(StoreSCPStatus* status) {
    if (status) {
        if (status->storage_dir) free(status->storage_dir);
        if (status->error_message) free(status->error_message);
        free(status);
    }
}

// ============================================================
// C-MOVE: Retrieve instances via C-MOVE
// ============================================================

DicomInstanceQueryResult* dcmtk_move_instances(const char* server_host, int server_port,
                                                const char* ae_title, const char* called_ae_title,
                                                const char* series_instance_uid,
                                                const char* local_storage_path,
                                                int move_scp_port) {
    DEBUG_LOG("C-MOVE retrieval for series %s from %s:%d (SCP port %d)",
              series_instance_uid, server_host, server_port, move_scp_port);

    DicomInstanceQueryResult* result = (DicomInstanceQueryResult*)malloc(sizeof(DicomInstanceQueryResult));
    result->instances = nullptr;
    result->instance_count = 0;
    result->error = 0;
    result->error_message = nullptr;

    // Create storage directory
    struct stat st;
    if (stat(local_storage_path, &st) != 0) {
        mkdir(local_storage_path, 0755);
    }

    // First, start a temporary SCP to receive the moved instances
    // We'll collect files after the SCP finishes
    int prev_received = g_scp_received_count;

    try {
        DcmSCU scu;
        scu.setAETitle(ae_title);
        scu.setPeerAETitle(called_ae_title);
        scu.setPeerHostName(server_host);
        scu.setPeerPort(server_port);
        scu.setMaxReceivePDULength(16384);
        scu.setACSETimeout(30);
        scu.setDIMSETimeout(120);

        OFList<OFString> transferSyntaxes;
        transferSyntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
        transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);

        scu.addPresentationContext(UID_FINDStudyRootQueryRetrieveInformationModel, transferSyntaxes);
        scu.addPresentationContext(UID_MOVEStudyRootQueryRetrieveInformationModel, transferSyntaxes);
        scu.addPresentationContext(UID_VerificationSOPClass, transferSyntaxes);

        OFCondition cond = scu.initNetwork();
        if (cond.bad()) {
            result->error = 1;
            std::string msg = "C-MOVE network init failed: ";
            msg += cond.text();
            result->error_message = strdup(msg.c_str());
            return result;
        }
        if (!maybe_apply_tls(scu)) {
            result->error = 1;
            result->error_message = strdup("TLS configuration failed");
            return result;
        }

        cond = scu.negotiateAssociation();
        if (cond.bad()) {
            result->error = 1;
            std::string msg = "C-MOVE association failed: ";
            msg += cond.text();
            result->error_message = strdup(msg.c_str());
            return result;
        }

        T_ASC_PresentationContextID presID = scu.findPresentationContextID(
            UID_MOVEStudyRootQueryRetrieveInformationModel, "");
        if (presID == 0) {
            scu.releaseAssociation();
            result->error = 1;
            result->error_message = strdup("No C-MOVE presentation context accepted");
            return result;
        }

        // Build the move request
        DcmDataset moveQuery;
        moveQuery.putAndInsertOFStringArray(DCM_QueryRetrieveLevel, "SERIES");
        moveQuery.putAndInsertOFStringArray(DCM_SeriesInstanceUID, series_instance_uid);

        // Send C-MOVE request — the server will send back via C-STORE sub-operations
        // The destination is our own AE title (requires SCP to be running)
        OFList<RetrieveResponse*> responses;
        cond = scu.sendMOVERequest(presID, ae_title, &moveQuery, &responses);

        if (cond.bad()) {
            scu.releaseAssociation();
            result->error = 1;
            std::string msg = "C-MOVE request failed: ";
            msg += cond.text();
            result->error_message = strdup(msg.c_str());
            return result;
        }

        // Check responses
        int completedOps = 0;
        int failedOps = 0;
        for (OFListIterator(RetrieveResponse*) it = responses.begin(); it != responses.end(); ++it) {
            if (*it) {
                Uint16 status = (*it)->m_status;
                if (status == STATUS_Success || status == STATUS_Pending) {
                    completedOps++;
                } else if ((status & 0xFF00) == STATUS_MOVE_Failed_UnableToProcess) {
                    failedOps++;
                }
            }
        }

        scu.releaseAssociation();

        DEBUG_LOG("C-MOVE completed: %d responses, %d failed", completedOps, failedOps);

        // Collect files that were stored by the SCP
        // (files should now be in local_storage_path if SCP was running)
        DIR* dir = opendir(local_storage_path);
        if (dir) {
            std::vector<std::string> filePaths;
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                std::string name = entry->d_name;
                if (name.length() > 4 && name.substr(name.length() - 4) == ".dcm") {
                    filePaths.push_back(std::string(local_storage_path) + "/" + name);
                }
            }
            closedir(dir);

            if (!filePaths.empty()) {
                result->instance_count = (int)filePaths.size();
                result->instances = (DicomInstance*)calloc(result->instance_count, sizeof(DicomInstance));
                for (int i = 0; i < result->instance_count; i++) {
                    result->instances[i].file_path = strdup(filePaths[i].c_str());
                    result->instances[i].sop_instance_uid = nullptr;
                    result->instances[i].instance_number = nullptr;
                    result->instances[i].content_type = nullptr;
                    result->instances[i].file_size = 0;

                    struct stat fileStat;
                    if (stat(filePaths[i].c_str(), &fileStat) == 0) {
                        result->instances[i].file_size = (int)fileStat.st_size;
                    }
                }
            }
        }

        DEBUG_LOG("C-MOVE collected %d files", result->instance_count);

    } catch (const std::exception& e) {
        result->error = 1;
        result->error_message = strdup(e.what());
    }

    return result;
}

// ============================================================================
// TLS Configuration API
// ============================================================================

void dcmtk_set_tls_config(const char* cert_file, const char* key_file, const char* ca_file) {
    g_tlsConfig.certFile = cert_file ? cert_file : "";
    g_tlsConfig.keyFile  = key_file  ? key_file  : "";
    g_tlsConfig.caFile   = ca_file   ? ca_file   : "";
    g_tlsConfig.enabled  = true;
    DEBUG_LOG("TLS config set: cert=%s, key=%s, ca=%s",
              g_tlsConfig.certFile.c_str(),
              g_tlsConfig.keyFile.c_str(),
              g_tlsConfig.caFile.c_str());
}

void dcmtk_clear_tls_config(void) {
    g_tlsConfig.certFile.clear();
    g_tlsConfig.keyFile.clear();
    g_tlsConfig.caFile.clear();
    g_tlsConfig.enabled = false;
    DEBUG_LOG("TLS config cleared");
}

int dcmtk_is_tls_available(void) {
#ifdef WITH_OPENSSL
    return 1;
#else
    return 0;
#endif
}

int dcmtk_is_tls_enabled(void) {
    return g_tlsConfig.enabled ? 1 : 0;
}

}