#include <jni.h>
#include "../dcmtk_flutter_wrapper.h"
#include <string>

extern "C" JNIEXPORT jstring JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeLoadDicomFile(JNIEnv *env, jobject /* this */, jstring filePath) {
    const char *nativeFilePath = env->GetStringUTFChars(filePath, 0);
    
    char* result = dcmtk_load_dicom_file(nativeFilePath);
    
    env->ReleaseStringUTFChars(filePath, nativeFilePath);
    
    if (result) {
        jstring jResult = env->NewStringUTF(result);
        dcmtk_free_string(result);
        return jResult;
    } else {
        return env->NewStringUTF("{\"error\":\"Failed to load DICOM file\"}");
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeGetVersion(JNIEnv *env, jobject /* this */) {
    char* result = dcmtk_get_version();
    
    if (result) {
        jstring jResult = env->NewStringUTF(result);
        dcmtk_free_string(result);
        return jResult;
    } else {
        return env->NewStringUTF("Unknown version");
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeValidateDicomFile(JNIEnv *env, jobject /* this */, jstring filePath) {
    const char *nativeFilePath = env->GetStringUTFChars(filePath, 0);
    
    int result = dcmtk_validate_dicom_file(nativeFilePath);
    
    env->ReleaseStringUTFChars(filePath, nativeFilePath);
    
    return result ? JNI_TRUE : JNI_FALSE;
}