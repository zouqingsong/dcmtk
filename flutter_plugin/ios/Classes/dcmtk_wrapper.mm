#import "dcmtk_wrapper.h"
#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <string>
#include <sstream>

extern "C" {

const char* dcmtk_load_dicom_file(const char* filename) {
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
    
    return strdup(oss.str().c_str());
}

void dcmtk_free_string(const char* str) {
    if (str) {
        free((void*)str);
    }
}

} // extern "C"
