#include <jni.h>
#include "dcmtk_flutter_wrapper.h"
#include <string>
#include <cstring>
#include <android/log.h>

#define LOG_TAG "DcmtkFlutter"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Helper: convert jstring to const char* (caller must ReleaseStringUTFChars)
static const char* jstringToChar(JNIEnv* env, jstring js) {
    if (js == nullptr) return nullptr;
    return env->GetStringUTFChars(js, nullptr);
}

static void releaseString(JNIEnv* env, jstring js, const char* cs) {
    if (js != nullptr && cs != nullptr) {
        env->ReleaseStringUTFChars(js, cs);
    }
}

static jstring safeNewStringUTF(JNIEnv* env, const char* s) {
    return env->NewStringUTF(s ? s : "");
}

// ==================== Basic DICOM operations ====================

extern "C" JNIEXPORT jstring JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeLoadDicomFile(JNIEnv *env, jobject, jstring filePath) {
    const char *path = jstringToChar(env, filePath);
    char* result = dcmtk_load_dicom_file(path);
    releaseString(env, filePath, path);
    if (result) {
        jstring jResult = env->NewStringUTF(result);
        dcmtk_free_string(result);
        return jResult;
    }
    return env->NewStringUTF("{\"error\":\"Failed to load DICOM file\"}");
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeGetVersion(JNIEnv *env, jobject) {
    char* result = dcmtk_get_version();
    if (result) {
        jstring jResult = env->NewStringUTF(result);
        dcmtk_free_string(result);
        return jResult;
    }
    return env->NewStringUTF("Unknown version");
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeValidateDicomFile(JNIEnv *env, jobject, jstring filePath) {
    const char *path = jstringToChar(env, filePath);
    int result = dcmtk_validate_dicom_file(path);
    releaseString(env, filePath, path);
    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeGetDicomTag(JNIEnv *env, jobject, jstring filePath, jstring tagName) {
    const char *path = jstringToChar(env, filePath);
    const char *tag = jstringToChar(env, tagName);
    char* result = dcmtk_get_dicom_tag(path, tag);
    releaseString(env, filePath, path);
    releaseString(env, tagName, tag);
    if (result) {
        jstring jResult = env->NewStringUTF(result);
        dcmtk_free_string(result);
        return jResult;
    }
    return env->NewStringUTF("");
}

// ==================== Image extraction ====================

extern "C" JNIEXPORT jobject JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeExtractImage(JNIEnv *env, jobject,
        jstring filePath, jint frameIndex, jdouble windowCenter, jdouble windowWidth) {
    const char *path = jstringToChar(env, filePath);
    DicomImageData* imgData = dcmtk_extract_image(path, frameIndex, windowCenter, windowWidth);
    releaseString(env, filePath, path);

    // Build result HashMap
    jclass hashMapClass = env->FindClass("java/util/HashMap");
    jmethodID hashMapInit = env->GetMethodID(hashMapClass, "<init>", "()V");
    jmethodID hashMapPut = env->GetMethodID(hashMapClass, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject map = env->NewObject(hashMapClass, hashMapInit);

    if (imgData->error) {
        env->CallObjectMethod(map, hashMapPut,
                env->NewStringUTF("error"),
                env->NewStringUTF(imgData->error_message ? imgData->error_message : "Unknown error"));
        dcmtk_free_image_data(imgData);
        return map;
    }

    int dataSize = imgData->width * imgData->height * 4;
    jbyteArray pixelData = env->NewByteArray(dataSize);
    env->SetByteArrayRegion(pixelData, 0, dataSize, (jbyte*)imgData->data);

    jclass integerClass = env->FindClass("java/lang/Integer");
    jmethodID intValueOf = env->GetStaticMethodID(integerClass, "valueOf", "(I)Ljava/lang/Integer;");

    env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("width"),
            env->CallStaticObjectMethod(integerClass, intValueOf, imgData->width));
    env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("height"),
            env->CallStaticObjectMethod(integerClass, intValueOf, imgData->height));
    env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("data"), pixelData);
    env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("samplesPerPixel"),
            env->CallStaticObjectMethod(integerClass, intValueOf, imgData->samples_per_pixel));
    env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("bitsStored"),
            env->CallStaticObjectMethod(integerClass, intValueOf, imgData->bits_stored));
    env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("totalFrames"),
            env->CallStaticObjectMethod(integerClass, intValueOf, imgData->total_frames));

    dcmtk_free_image_data(imgData);
    return map;
}

// ==================== Server connection ====================

extern "C" JNIEXPORT jboolean JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeTestServerConnection(JNIEnv *env, jobject,
        jstring serverHost, jint serverPort, jstring aeTitle, jstring calledAeTitle) {
    const char *host = jstringToChar(env, serverHost);
    const char *ae = jstringToChar(env, aeTitle);
    const char *called = jstringToChar(env, calledAeTitle);
    int result = dcmtk_test_server_connection(host, serverPort, ae, called);
    releaseString(env, serverHost, host);
    releaseString(env, aeTitle, ae);
    releaseString(env, calledAeTitle, called);
    return result == 1 ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeTestServerConnectionTls(JNIEnv *env, jobject,
        jstring serverHost, jint serverPort, jstring aeTitle, jstring calledAeTitle,
        jstring certFile, jstring keyFile, jstring caFile) {
    const char *host = jstringToChar(env, serverHost);
    const char *ae = jstringToChar(env, aeTitle);
    const char *called = jstringToChar(env, calledAeTitle);
    const char *cert = jstringToChar(env, certFile);
    const char *key = jstringToChar(env, keyFile);
    const char *ca = jstringToChar(env, caFile);
    int result = dcmtk_test_server_connection_tls(host, serverPort, ae, called,
            cert ? cert : "", key ? key : "", ca ? ca : "");
    releaseString(env, serverHost, host);
    releaseString(env, aeTitle, ae);
    releaseString(env, calledAeTitle, called);
    releaseString(env, certFile, cert);
    releaseString(env, keyFile, key);
    releaseString(env, caFile, ca);
    return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeInitDictionary(JNIEnv *env, jobject,
                jstring dictionaryPath) {
        const char *path = jstringToChar(env, dictionaryPath);
        int result = dcmtk_init_dictionary(path);
        releaseString(env, dictionaryPath, path);
        return result == 1 ? JNI_TRUE : JNI_FALSE;
}

// ==================== Query operations ====================

static jobject createPatientList(JNIEnv* env, DicomQueryResult* queryResult) {
    jclass arrayListClass = env->FindClass("java/util/ArrayList");
    jmethodID arrayListInit = env->GetMethodID(arrayListClass, "<init>", "()V");
    jmethodID arrayListAdd = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");
    jobject list = env->NewObject(arrayListClass, arrayListInit);

    jclass hashMapClass = env->FindClass("java/util/HashMap");
    jmethodID hashMapInit = env->GetMethodID(hashMapClass, "<init>", "()V");
    jmethodID hashMapPut = env->GetMethodID(hashMapClass, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");

    for (int i = 0; i < queryResult->patient_count; i++) {
        DicomPatient* p = &queryResult->patients[i];
        jobject map = env->NewObject(hashMapClass, hashMapInit);
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("patientId"), safeNewStringUTF(env, p->patient_id));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("patientName"), safeNewStringUTF(env, p->patient_name));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("patientBirthDate"), safeNewStringUTF(env, p->patient_birth_date));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("patientSex"), safeNewStringUTF(env, p->patient_sex));

        jclass integerClass = env->FindClass("java/lang/Integer");
        jmethodID intValueOf = env->GetStaticMethodID(integerClass, "valueOf", "(I)Ljava/lang/Integer;");
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("studyCount"),
                env->CallStaticObjectMethod(integerClass, intValueOf, p->study_count));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("numberOfPatientRelatedStudies"),
                safeNewStringUTF(env, p->number_of_patient_related_studies));

        env->CallBooleanMethod(list, arrayListAdd, map);
    }
    return list;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeQueryPatients(JNIEnv *env, jobject,
        jstring serverHost, jint serverPort, jstring aeTitle, jstring calledAeTitle, jstring patientNameFilter) {
    const char *host = jstringToChar(env, serverHost);
    const char *ae = jstringToChar(env, aeTitle);
    const char *called = jstringToChar(env, calledAeTitle);
    const char *filter = jstringToChar(env, patientNameFilter);

    DicomQueryResult* queryResult = dcmtk_query_patients(host, serverPort, ae, called, filter);
    releaseString(env, serverHost, host);
    releaseString(env, aeTitle, ae);
    releaseString(env, calledAeTitle, called);
    releaseString(env, patientNameFilter, filter);

    // Build result map
    jclass hashMapClass = env->FindClass("java/util/HashMap");
    jmethodID hashMapInit = env->GetMethodID(hashMapClass, "<init>", "()V");
    jmethodID hashMapPut = env->GetMethodID(hashMapClass, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject resultMap = env->NewObject(hashMapClass, hashMapInit);

    if (queryResult->error) {
        env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("error"),
                env->NewStringUTF(queryResult->error_message ? queryResult->error_message : "Unknown error"));
        dcmtk_free_query_result(queryResult);
        return resultMap;
    }

    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("patients"),
            createPatientList(env, queryResult));
    dcmtk_free_query_result(queryResult);
    return resultMap;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeQueryStudiesForPatient(JNIEnv *env, jobject,
        jstring serverHost, jint serverPort, jstring aeTitle, jstring calledAeTitle, jstring patientId) {
    const char *host = jstringToChar(env, serverHost);
    const char *ae = jstringToChar(env, aeTitle);
    const char *called = jstringToChar(env, calledAeTitle);
    const char *pid = jstringToChar(env, patientId);

    DicomStudyQueryResult* queryResult = dcmtk_query_studies_for_patient(host, serverPort, ae, called, pid);
    releaseString(env, serverHost, host);
    releaseString(env, aeTitle, ae);
    releaseString(env, calledAeTitle, called);
    releaseString(env, patientId, pid);

    jclass hashMapClass = env->FindClass("java/util/HashMap");
    jmethodID hashMapInit = env->GetMethodID(hashMapClass, "<init>", "()V");
    jmethodID hashMapPut = env->GetMethodID(hashMapClass, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject resultMap = env->NewObject(hashMapClass, hashMapInit);

    if (queryResult->error) {
        env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("error"),
                env->NewStringUTF(queryResult->error_message ? queryResult->error_message : "Unknown error"));
        dcmtk_free_study_query_result(queryResult);
        return resultMap;
    }

    // Build studies list
    jclass arrayListClass = env->FindClass("java/util/ArrayList");
    jmethodID arrayListInit = env->GetMethodID(arrayListClass, "<init>", "()V");
    jmethodID arrayListAdd = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");
    jobject studiesList = env->NewObject(arrayListClass, arrayListInit);

    jclass integerClass = env->FindClass("java/lang/Integer");
    jmethodID intValueOf = env->GetStaticMethodID(integerClass, "valueOf", "(I)Ljava/lang/Integer;");

    for (int i = 0; i < queryResult->study_count; i++) {
        DicomStudy* s = &queryResult->studies[i];
        jobject map = env->NewObject(hashMapClass, hashMapInit);
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("studyInstanceUID"), safeNewStringUTF(env, s->study_instance_uid));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("studyDate"), safeNewStringUTF(env, s->study_date));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("studyTime"), safeNewStringUTF(env, s->study_time));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("studyDescription"), safeNewStringUTF(env, s->study_description));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("accessionNumber"), safeNewStringUTF(env, s->accession_number));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("seriesCount"),
                env->CallStaticObjectMethod(integerClass, intValueOf, s->series_count));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("modalitiesInStudy"), safeNewStringUTF(env, s->modalities_in_study));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("numberOfStudyRelatedSeries"), safeNewStringUTF(env, s->number_of_study_related_series));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("numberOfStudyRelatedInstances"), safeNewStringUTF(env, s->number_of_study_related_instances));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("referringPhysicianName"), safeNewStringUTF(env, s->referring_physician_name));
        env->CallBooleanMethod(studiesList, arrayListAdd, map);
    }

    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("studies"), studiesList);
    dcmtk_free_study_query_result(queryResult);
    return resultMap;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeQuerySeriesForStudy(JNIEnv *env, jobject,
        jstring serverHost, jint serverPort, jstring aeTitle, jstring calledAeTitle, jstring studyInstanceUID) {
    const char *host = jstringToChar(env, serverHost);
    const char *ae = jstringToChar(env, aeTitle);
    const char *called = jstringToChar(env, calledAeTitle);
    const char *uid = jstringToChar(env, studyInstanceUID);

    DicomSeriesQueryResult* seriesResult = dcmtk_query_series_for_study(host, serverPort, ae, called, uid);
    releaseString(env, serverHost, host);
    releaseString(env, aeTitle, ae);
    releaseString(env, calledAeTitle, called);
    releaseString(env, studyInstanceUID, uid);

    jclass hashMapClass = env->FindClass("java/util/HashMap");
    jmethodID hashMapInit = env->GetMethodID(hashMapClass, "<init>", "()V");
    jmethodID hashMapPut = env->GetMethodID(hashMapClass, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject resultMap = env->NewObject(hashMapClass, hashMapInit);

    if (seriesResult->error) {
        env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("error"),
                env->NewStringUTF(seriesResult->error_message ? seriesResult->error_message : "Unknown error"));
        dcmtk_free_series_query_result(seriesResult);
        return resultMap;
    }

    jclass arrayListClass = env->FindClass("java/util/ArrayList");
    jmethodID arrayListInit = env->GetMethodID(arrayListClass, "<init>", "()V");
    jmethodID arrayListAdd = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");
    jobject seriesList = env->NewObject(arrayListClass, arrayListInit);

    jclass integerClass = env->FindClass("java/lang/Integer");
    jmethodID intValueOf = env->GetStaticMethodID(integerClass, "valueOf", "(I)Ljava/lang/Integer;");

    for (int i = 0; i < seriesResult->series_count; i++) {
        DicomSeries* s = &seriesResult->series[i];
        jobject map = env->NewObject(hashMapClass, hashMapInit);
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("seriesInstanceUID"), safeNewStringUTF(env, s->series_instance_uid));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("seriesNumber"), safeNewStringUTF(env, s->series_number));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("seriesDescription"), safeNewStringUTF(env, s->series_description));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("modality"), safeNewStringUTF(env, s->modality));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("seriesDate"), safeNewStringUTF(env, s->series_date));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("seriesTime"), safeNewStringUTF(env, s->series_time));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("instanceCount"),
                env->CallStaticObjectMethod(integerClass, intValueOf, s->instance_count));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("numberOfSeriesRelatedInstances"),
                safeNewStringUTF(env, s->number_of_series_related_instances));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("bodyPartExamined"), safeNewStringUTF(env, s->body_part_examined));
        env->CallBooleanMethod(seriesList, arrayListAdd, map);
    }

    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("series"), seriesList);
    dcmtk_free_series_query_result(seriesResult);
    return resultMap;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeQueryInstancesForSeries(JNIEnv *env, jobject,
        jstring serverHost, jint serverPort, jstring aeTitle, jstring calledAeTitle, jstring seriesInstanceUID) {
    const char *host = jstringToChar(env, serverHost);
    const char *ae = jstringToChar(env, aeTitle);
    const char *called = jstringToChar(env, calledAeTitle);
    const char *uid = jstringToChar(env, seriesInstanceUID);

    DicomInstanceQueryResult* instResult = dcmtk_query_instances_for_series(host, serverPort, ae, called, uid);
    releaseString(env, serverHost, host);
    releaseString(env, aeTitle, ae);
    releaseString(env, calledAeTitle, called);
    releaseString(env, seriesInstanceUID, uid);

    jclass hashMapClass = env->FindClass("java/util/HashMap");
    jmethodID hashMapInit = env->GetMethodID(hashMapClass, "<init>", "()V");
    jmethodID hashMapPut = env->GetMethodID(hashMapClass, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject resultMap = env->NewObject(hashMapClass, hashMapInit);

    if (instResult->error) {
        env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("error"),
                env->NewStringUTF(instResult->error_message ? instResult->error_message : "Unknown error"));
        dcmtk_free_instance_query_result(instResult);
        return resultMap;
    }

    jclass arrayListClass = env->FindClass("java/util/ArrayList");
    jmethodID arrayListInit = env->GetMethodID(arrayListClass, "<init>", "()V");
    jmethodID arrayListAdd = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");
    jobject instanceList = env->NewObject(arrayListClass, arrayListInit);

    jclass integerClass = env->FindClass("java/lang/Integer");
    jmethodID intValueOf = env->GetStaticMethodID(integerClass, "valueOf", "(I)Ljava/lang/Integer;");

    for (int i = 0; i < instResult->instance_count; i++) {
        DicomInstance* inst = &instResult->instances[i];
        jobject map = env->NewObject(hashMapClass, hashMapInit);
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("sopInstanceUID"), safeNewStringUTF(env, inst->sop_instance_uid));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("instanceNumber"), safeNewStringUTF(env, inst->instance_number));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("filePath"), safeNewStringUTF(env, inst->file_path));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("contentType"), safeNewStringUTF(env, inst->content_type ? inst->content_type : "IMAGE"));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("fileSize"),
                env->CallStaticObjectMethod(integerClass, intValueOf, inst->file_size));
        env->CallBooleanMethod(instanceList, arrayListAdd, map);
    }

    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("instances"), instanceList);
    dcmtk_free_instance_query_result(instResult);
    return resultMap;
}

// ==================== Patient creation ====================

extern "C" JNIEXPORT jobject JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeCreatePatient(JNIEnv *env, jobject,
        jstring serverHost, jint serverPort, jstring aeTitle, jstring calledAeTitle,
        jstring patientId, jstring patientName, jstring birthDate, jstring sex, jstring comments) {
    const char *host = jstringToChar(env, serverHost);
    const char *ae = jstringToChar(env, aeTitle);
    const char *called = jstringToChar(env, calledAeTitle);
    const char *pid = jstringToChar(env, patientId);
    const char *pname = jstringToChar(env, patientName);
    const char *dob = jstringToChar(env, birthDate);
    const char *psex = jstringToChar(env, sex);
    const char *pcomments = jstringToChar(env, comments);

    PatientInfo patientInfo;
    patientInfo.patient_id = (char*)(pid ? pid : "");
    patientInfo.patient_name = (char*)(pname ? pname : "");
    patientInfo.patient_birth_date = (char*)(dob ? dob : "");
    patientInfo.patient_sex = (char*)(psex ? psex : "");
    patientInfo.patient_comments = (char*)(pcomments ? pcomments : "");

    PatientCreationResult* creationResult = dcmtk_create_patient(host, serverPort, ae, called, &patientInfo);

    releaseString(env, serverHost, host);
    releaseString(env, aeTitle, ae);
    releaseString(env, calledAeTitle, called);
    releaseString(env, patientId, pid);
    releaseString(env, patientName, pname);
    releaseString(env, birthDate, dob);
    releaseString(env, sex, psex);
    releaseString(env, comments, pcomments);

    jclass hashMapClass = env->FindClass("java/util/HashMap");
    jmethodID hashMapInit = env->GetMethodID(hashMapClass, "<init>", "()V");
    jmethodID hashMapPut = env->GetMethodID(hashMapClass, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject resultMap = env->NewObject(hashMapClass, hashMapInit);

    if (!creationResult->success) {
        env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("error"),
                env->NewStringUTF(creationResult->error_message ? creationResult->error_message : "Unknown error"));
        dcmtk_free_patient_creation_result(creationResult);
        return resultMap;
    }

    jclass integerClass = env->FindClass("java/lang/Integer");
    jmethodID intValueOf = env->GetStaticMethodID(integerClass, "valueOf", "(I)Ljava/lang/Integer;");
    jclass booleanClass = env->FindClass("java/lang/Boolean");
    jmethodID boolValueOf = env->GetStaticMethodID(booleanClass, "valueOf", "(Z)Ljava/lang/Boolean;");

    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("success"),
            env->CallStaticObjectMethod(booleanClass, boolValueOf, JNI_TRUE));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("patientId"),
            safeNewStringUTF(env, creationResult->generated_patient_id));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("rspStatusCode"),
            env->CallStaticObjectMethod(integerClass, intValueOf, creationResult->rsp_status_code));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("warning"),
            safeNewStringUTF(env, creationResult->warning_message));

    dcmtk_free_patient_creation_result(creationResult);
    return resultMap;
}

// ==================== Upload operations ====================

extern "C" JNIEXPORT jobject JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeUploadImage(JNIEnv *env, jobject,
        jstring serverHost, jint serverPort, jstring aeTitle, jstring calledAeTitle,
        jstring patientId, jstring imagePath, jstring patientName, jstring patientBirthDate,
        jstring studyDescription, jstring seriesDescription,
        jstring imageComments, jstring modality, jstring studyInstanceUID, jstring seriesInstanceUID,
        jint instanceNumber) {
    const char *host = jstringToChar(env, serverHost);
    const char *ae = jstringToChar(env, aeTitle);
    const char *called = jstringToChar(env, calledAeTitle);
    const char *pid = jstringToChar(env, patientId);
    const char *imgPath = jstringToChar(env, imagePath);
    const char *pName = jstringToChar(env, patientName);
    const char *pDob = jstringToChar(env, patientBirthDate);
    const char *studyDesc = jstringToChar(env, studyDescription);
    const char *seriesDesc = jstringToChar(env, seriesDescription);
    const char *imgComments = jstringToChar(env, imageComments);
    const char *mod = jstringToChar(env, modality);
    const char *studyUID = jstringToChar(env, studyInstanceUID);
    const char *seriesUID = jstringToChar(env, seriesInstanceUID);

    MediaUploadResult* uploadResult = dcmtk_upload_image(host, serverPort, ae, called, pid, imgPath,
            pName ? pName : "",
            pDob ? pDob : "",
            studyDesc ? studyDesc : "Uploaded Image",
            seriesDesc ? seriesDesc : "Uploaded Series",
            imgComments ? imgComments : "",
            (mod && strlen(mod) > 0) ? mod : nullptr,
            studyUID ? studyUID : "",
            seriesUID ? seriesUID : "",
            instanceNumber);

    releaseString(env, serverHost, host);
    releaseString(env, aeTitle, ae);
    releaseString(env, calledAeTitle, called);
    releaseString(env, patientId, pid);
    releaseString(env, imagePath, imgPath);
    releaseString(env, patientName, pName);
    releaseString(env, patientBirthDate, pDob);
    releaseString(env, studyDescription, studyDesc);
    releaseString(env, seriesDescription, seriesDesc);
    releaseString(env, imageComments, imgComments);
    releaseString(env, modality, mod);
    releaseString(env, studyInstanceUID, studyUID);
    releaseString(env, seriesInstanceUID, seriesUID);

    jclass hashMapClass = env->FindClass("java/util/HashMap");
    jmethodID hashMapInit = env->GetMethodID(hashMapClass, "<init>", "()V");
    jmethodID hashMapPut = env->GetMethodID(hashMapClass, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject resultMap = env->NewObject(hashMapClass, hashMapInit);

    if (!uploadResult->success) {
        env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("error"),
                env->NewStringUTF(uploadResult->error_message ? uploadResult->error_message : "Unknown error"));
        dcmtk_free_media_upload_result(uploadResult);
        return resultMap;
    }

    jclass integerClass = env->FindClass("java/lang/Integer");
    jmethodID intValueOf = env->GetStaticMethodID(integerClass, "valueOf", "(I)Ljava/lang/Integer;");
    jclass booleanClass = env->FindClass("java/lang/Boolean");
    jmethodID boolValueOf = env->GetStaticMethodID(booleanClass, "valueOf", "(Z)Ljava/lang/Boolean;");

    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("success"),
            env->CallStaticObjectMethod(booleanClass, boolValueOf, JNI_TRUE));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("studyInstanceUID"),
            safeNewStringUTF(env, uploadResult->study_instance_uid));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("seriesInstanceUID"),
            safeNewStringUTF(env, uploadResult->series_instance_uid));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("sopInstanceUID"),
            safeNewStringUTF(env, uploadResult->sop_instance_uid));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("rspStatusCode"),
            env->CallStaticObjectMethod(integerClass, intValueOf, uploadResult->rsp_status_code));

    dcmtk_free_media_upload_result(uploadResult);
    return resultMap;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeUploadMultiframe(JNIEnv *env, jobject,
        jstring serverHost, jint serverPort, jstring aeTitle, jstring calledAeTitle,
        jstring patientId, jobjectArray imagePaths, jstring patientName, jstring patientBirthDate,
        jstring studyDescription, jstring seriesDescription,
        jstring imageComments, jstring modality, jstring studyInstanceUID, jstring seriesInstanceUID) {
    const char *host = jstringToChar(env, serverHost);
    const char *ae = jstringToChar(env, aeTitle);
    const char *called = jstringToChar(env, calledAeTitle);
    const char *pid = jstringToChar(env, patientId);
    const char *pName = jstringToChar(env, patientName);
    const char *pDob = jstringToChar(env, patientBirthDate);
    const char *studyDesc = jstringToChar(env, studyDescription);
    const char *seriesDesc = jstringToChar(env, seriesDescription);
    const char *imgComments = jstringToChar(env, imageComments);
    const char *mod = jstringToChar(env, modality);
    const char *studyUID = jstringToChar(env, studyInstanceUID);
    const char *seriesUID = jstringToChar(env, seriesInstanceUID);

    int imageCount = env->GetArrayLength(imagePaths);
    const char** cPaths = new const char*[imageCount];
    jstring* jPaths = new jstring[imageCount];
    for (int i = 0; i < imageCount; i++) {
        jPaths[i] = (jstring)env->GetObjectArrayElement(imagePaths, i);
        cPaths[i] = env->GetStringUTFChars(jPaths[i], nullptr);
    }

    MediaUploadResult* uploadResult = dcmtk_upload_multiframe(host, serverPort, ae, called, pid,
            cPaths, imageCount,
            pName ? pName : "",
            pDob ? pDob : "",
            studyDesc ? studyDesc : "Uploaded Study",
            seriesDesc ? seriesDesc : "Multi-frame Series",
            imgComments ? imgComments : "",
            mod ? mod : "SC",
            studyUID ? studyUID : "",
            seriesUID ? seriesUID : "");

    for (int i = 0; i < imageCount; i++) {
        env->ReleaseStringUTFChars(jPaths[i], cPaths[i]);
    }
    delete[] cPaths;
    delete[] jPaths;

    releaseString(env, serverHost, host);
    releaseString(env, aeTitle, ae);
    releaseString(env, calledAeTitle, called);
    releaseString(env, patientId, pid);
    releaseString(env, patientName, pName);
    releaseString(env, patientBirthDate, pDob);
    releaseString(env, studyDescription, studyDesc);
    releaseString(env, seriesDescription, seriesDesc);
    releaseString(env, imageComments, imgComments);
    releaseString(env, modality, mod);
    releaseString(env, studyInstanceUID, studyUID);
    releaseString(env, seriesInstanceUID, seriesUID);

    jclass hashMapClass = env->FindClass("java/util/HashMap");
    jmethodID hashMapInit = env->GetMethodID(hashMapClass, "<init>", "()V");
    jmethodID hashMapPut = env->GetMethodID(hashMapClass, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject resultMap = env->NewObject(hashMapClass, hashMapInit);

    if (!uploadResult->success) {
        env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("error"),
                env->NewStringUTF(uploadResult->error_message ? uploadResult->error_message : "Unknown error"));
        dcmtk_free_media_upload_result(uploadResult);
        return resultMap;
    }

    jclass booleanClass = env->FindClass("java/lang/Boolean");
    jmethodID boolValueOf = env->GetStaticMethodID(booleanClass, "valueOf", "(Z)Ljava/lang/Boolean;");

    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("success"),
            env->CallStaticObjectMethod(booleanClass, boolValueOf, JNI_TRUE));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("studyInstanceUID"),
            safeNewStringUTF(env, uploadResult->study_instance_uid));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("seriesInstanceUID"),
            safeNewStringUTF(env, uploadResult->series_instance_uid));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("sopInstanceUID"),
            safeNewStringUTF(env, uploadResult->sop_instance_uid));

    dcmtk_free_media_upload_result(uploadResult);
    return resultMap;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeUploadVideo(JNIEnv *env, jobject,
        jstring serverHost, jint serverPort, jstring aeTitle, jstring calledAeTitle,
        jstring patientId, jstring videoPath, jstring patientName, jstring patientBirthDate,
        jstring studyDescription, jstring seriesDescription,
        jstring imageComments, jstring modality) {
    const char *host = jstringToChar(env, serverHost);
    const char *ae = jstringToChar(env, aeTitle);
    const char *called = jstringToChar(env, calledAeTitle);
    const char *pid = jstringToChar(env, patientId);
    const char *vidPath = jstringToChar(env, videoPath);
    const char *pName = jstringToChar(env, patientName);
    const char *pDob = jstringToChar(env, patientBirthDate);
    const char *studyDesc = jstringToChar(env, studyDescription);
    const char *seriesDesc = jstringToChar(env, seriesDescription);
    const char *imgComments = jstringToChar(env, imageComments);
    const char *mod = jstringToChar(env, modality);

    MediaUploadResult* uploadResult = dcmtk_upload_video(host, serverPort, ae, called, pid, vidPath,
            pName ? pName : "",
            pDob ? pDob : "",
            studyDesc ? studyDesc : "Uploaded Video",
            seriesDesc ? seriesDesc : "Uploaded Video Series",
            imgComments ? imgComments : "",
            mod ? mod : "SC");

    releaseString(env, serverHost, host);
    releaseString(env, aeTitle, ae);
    releaseString(env, calledAeTitle, called);
    releaseString(env, patientId, pid);
    releaseString(env, videoPath, vidPath);
    releaseString(env, patientName, pName);
    releaseString(env, patientBirthDate, pDob);
    releaseString(env, studyDescription, studyDesc);
    releaseString(env, seriesDescription, seriesDesc);
    releaseString(env, imageComments, imgComments);
    releaseString(env, modality, mod);

    jclass hashMapClass = env->FindClass("java/util/HashMap");
    jmethodID hashMapInit = env->GetMethodID(hashMapClass, "<init>", "()V");
    jmethodID hashMapPut = env->GetMethodID(hashMapClass, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject resultMap = env->NewObject(hashMapClass, hashMapInit);

    if (!uploadResult->success) {
        env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("error"),
                env->NewStringUTF(uploadResult->error_message ? uploadResult->error_message : "Unknown error"));
        dcmtk_free_media_upload_result(uploadResult);
        return resultMap;
    }

    jclass booleanClass = env->FindClass("java/lang/Boolean");
    jmethodID boolValueOf = env->GetStaticMethodID(booleanClass, "valueOf", "(Z)Ljava/lang/Boolean;");

    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("success"),
            env->CallStaticObjectMethod(booleanClass, boolValueOf, JNI_TRUE));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("studyInstanceUID"),
            safeNewStringUTF(env, uploadResult->study_instance_uid));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("seriesInstanceUID"),
            safeNewStringUTF(env, uploadResult->series_instance_uid));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("sopInstanceUID"),
            safeNewStringUTF(env, uploadResult->sop_instance_uid));

    dcmtk_free_media_upload_result(uploadResult);
    return resultMap;
}

// ==================== Download / C-MOVE ====================

static jobject buildInstanceResultMap(JNIEnv* env, DicomInstanceQueryResult* instResult) {
    jclass hashMapClass = env->FindClass("java/util/HashMap");
    jmethodID hashMapInit = env->GetMethodID(hashMapClass, "<init>", "()V");
    jmethodID hashMapPut = env->GetMethodID(hashMapClass, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject resultMap = env->NewObject(hashMapClass, hashMapInit);

    if (instResult->error) {
        env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("error"),
                env->NewStringUTF(instResult->error_message ? instResult->error_message : "Unknown error"));
        return resultMap;
    }

    jclass arrayListClass = env->FindClass("java/util/ArrayList");
    jmethodID arrayListInit = env->GetMethodID(arrayListClass, "<init>", "()V");
    jmethodID arrayListAdd = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");
    jobject instanceList = env->NewObject(arrayListClass, arrayListInit);

    jclass integerClass = env->FindClass("java/lang/Integer");
    jmethodID intValueOf = env->GetStaticMethodID(integerClass, "valueOf", "(I)Ljava/lang/Integer;");

    for (int i = 0; i < instResult->instance_count; i++) {
        DicomInstance* inst = &instResult->instances[i];
        jobject map = env->NewObject(hashMapClass, hashMapInit);
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("sopInstanceUID"), safeNewStringUTF(env, inst->sop_instance_uid));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("instanceNumber"), safeNewStringUTF(env, inst->instance_number));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("filePath"), safeNewStringUTF(env, inst->file_path));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("contentType"),
                safeNewStringUTF(env, inst->content_type ? inst->content_type : "IMAGE"));
        env->CallObjectMethod(map, hashMapPut, env->NewStringUTF("fileSize"),
                env->CallStaticObjectMethod(integerClass, intValueOf, inst->file_size));
        env->CallBooleanMethod(instanceList, arrayListAdd, map);
    }

    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("instances"), instanceList);
    return resultMap;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeDownloadInstances(JNIEnv *env, jobject,
        jstring serverHost, jint serverPort, jstring aeTitle, jstring calledAeTitle,
        jstring seriesInstanceUID, jstring localStoragePath) {
    const char *host = jstringToChar(env, serverHost);
    const char *ae = jstringToChar(env, aeTitle);
    const char *called = jstringToChar(env, calledAeTitle);
    const char *uid = jstringToChar(env, seriesInstanceUID);
    const char *storagePath = jstringToChar(env, localStoragePath);

    DicomInstanceQueryResult* instResult = dcmtk_download_instances(host, serverPort, ae, called, uid, storagePath);
    releaseString(env, serverHost, host);
    releaseString(env, aeTitle, ae);
    releaseString(env, calledAeTitle, called);
    releaseString(env, seriesInstanceUID, uid);
    releaseString(env, localStoragePath, storagePath);

    jobject resultMap = buildInstanceResultMap(env, instResult);
    dcmtk_free_instance_query_result(instResult);
    return resultMap;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeMoveInstances(JNIEnv *env, jobject,
        jstring serverHost, jint serverPort, jstring aeTitle, jstring calledAeTitle,
        jstring seriesInstanceUID, jstring localStoragePath, jint moveSCPPort) {
    const char *host = jstringToChar(env, serverHost);
    const char *ae = jstringToChar(env, aeTitle);
    const char *called = jstringToChar(env, calledAeTitle);
    const char *uid = jstringToChar(env, seriesInstanceUID);
    const char *storagePath = jstringToChar(env, localStoragePath);

    DicomInstanceQueryResult* instResult = dcmtk_move_instances(host, serverPort, ae, called, uid, storagePath, moveSCPPort);
    releaseString(env, serverHost, host);
    releaseString(env, aeTitle, ae);
    releaseString(env, calledAeTitle, called);
    releaseString(env, seriesInstanceUID, uid);
    releaseString(env, localStoragePath, storagePath);

    jobject resultMap = buildInstanceResultMap(env, instResult);
    dcmtk_free_instance_query_result(instResult);
    return resultMap;
}

// ==================== Video extraction ====================

extern "C" JNIEXPORT jobject JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeExtractVideo(JNIEnv *env, jobject,
        jstring dicomPath, jstring outputPath) {
    const char *dcmPath = jstringToChar(env, dicomPath);
    const char *outPath = jstringToChar(env, outputPath);

    VideoExtractionResult* vidResult = dcmtk_extract_video(dcmPath, outPath);
    releaseString(env, dicomPath, dcmPath);
    releaseString(env, outputPath, outPath);

    jclass hashMapClass = env->FindClass("java/util/HashMap");
    jmethodID hashMapInit = env->GetMethodID(hashMapClass, "<init>", "()V");
    jmethodID hashMapPut = env->GetMethodID(hashMapClass, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject resultMap = env->NewObject(hashMapClass, hashMapInit);

    if (!vidResult->success) {
        env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("error"),
                env->NewStringUTF(vidResult->error_message ? vidResult->error_message : "Unknown error"));
        dcmtk_free_video_extraction_result(vidResult);
        return resultMap;
    }

    jclass longClass = env->FindClass("java/lang/Long");
    jmethodID longValueOf = env->GetStaticMethodID(longClass, "valueOf", "(J)Ljava/lang/Long;");

    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("outputPath"),
            safeNewStringUTF(env, vidResult->output_path));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("mimeType"),
            safeNewStringUTF(env, vidResult->mime_type));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("fileSize"),
            env->CallStaticObjectMethod(longClass, longValueOf, (jlong)vidResult->file_size));

    dcmtk_free_video_extraction_result(vidResult);
    return resultMap;
}

// ==================== C-STORE SCU ====================

extern "C" JNIEXPORT jobject JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeStoreFiles(JNIEnv *env, jobject,
        jstring serverHost, jint serverPort, jstring aeTitle, jstring calledAeTitle,
        jobjectArray filePaths) {
    const char *host = jstringToChar(env, serverHost);
    const char *ae = jstringToChar(env, aeTitle);
    const char *called = jstringToChar(env, calledAeTitle);

    int count = env->GetArrayLength(filePaths);
    const char** cPaths = new const char*[count];
    jstring* jPaths = new jstring[count];
    for (int i = 0; i < count; i++) {
        jPaths[i] = (jstring)env->GetObjectArrayElement(filePaths, i);
        cPaths[i] = env->GetStringUTFChars(jPaths[i], nullptr);
    }

    StoreResult* storeRes = dcmtk_store_files(host, serverPort, ae, called, cPaths, count);

    for (int i = 0; i < count; i++) {
        env->ReleaseStringUTFChars(jPaths[i], cPaths[i]);
    }
    delete[] cPaths;
    delete[] jPaths;

    releaseString(env, serverHost, host);
    releaseString(env, aeTitle, ae);
    releaseString(env, calledAeTitle, called);

    jclass hashMapClass = env->FindClass("java/util/HashMap");
    jmethodID hashMapInit = env->GetMethodID(hashMapClass, "<init>", "()V");
    jmethodID hashMapPut = env->GetMethodID(hashMapClass, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject resultMap = env->NewObject(hashMapClass, hashMapInit);

    jclass integerClass = env->FindClass("java/lang/Integer");
    jmethodID intValueOf = env->GetStaticMethodID(integerClass, "valueOf", "(I)Ljava/lang/Integer;");

    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("successCount"),
            env->CallStaticObjectMethod(integerClass, intValueOf, storeRes->success_count));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("failCount"),
            env->CallStaticObjectMethod(integerClass, intValueOf, storeRes->fail_count));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("totalCount"),
            env->CallStaticObjectMethod(integerClass, intValueOf, storeRes->total_count));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("error"),
            env->CallStaticObjectMethod(integerClass, intValueOf, storeRes->error));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("errorMessage"),
            safeNewStringUTF(env, storeRes->error_message));

    dcmtk_free_store_result(storeRes);
    return resultMap;
}

// ==================== C-STORE SCP ====================

extern "C" JNIEXPORT jboolean JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeStartStoreSCP(JNIEnv *env, jobject,
        jint port, jstring aeTitle, jstring storageDir) {
    const char *ae = jstringToChar(env, aeTitle);
    const char *dir = jstringToChar(env, storageDir);
    int result = dcmtk_start_store_scp(port, ae, dir);
    releaseString(env, aeTitle, ae);
    releaseString(env, storageDir, dir);
    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeStopStoreSCP(JNIEnv *env, jobject) {
    dcmtk_stop_store_scp();
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeGetStoreSCPStatus(JNIEnv *env, jobject) {
    StoreSCPStatus* status = dcmtk_get_store_scp_status();

    jclass hashMapClass = env->FindClass("java/util/HashMap");
    jmethodID hashMapInit = env->GetMethodID(hashMapClass, "<init>", "()V");
    jmethodID hashMapPut = env->GetMethodID(hashMapClass, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject resultMap = env->NewObject(hashMapClass, hashMapInit);

    jclass integerClass = env->FindClass("java/lang/Integer");
    jmethodID intValueOf = env->GetStaticMethodID(integerClass, "valueOf", "(I)Ljava/lang/Integer;");
    jclass booleanClass = env->FindClass("java/lang/Boolean");
    jmethodID boolValueOf = env->GetStaticMethodID(booleanClass, "valueOf", "(Z)Ljava/lang/Boolean;");

    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("running"),
            env->CallStaticObjectMethod(booleanClass, boolValueOf, status->running ? JNI_TRUE : JNI_FALSE));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("port"),
            env->CallStaticObjectMethod(integerClass, intValueOf, status->port));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("receivedCount"),
            env->CallStaticObjectMethod(integerClass, intValueOf, status->received_count));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("storageDir"),
            safeNewStringUTF(env, status->storage_dir));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("errorMessage"),
            safeNewStringUTF(env, status->error_message));

    dcmtk_free_store_scp_status(status);
    return resultMap;
}

// ==================== TLS Configuration ====================

extern "C" JNIEXPORT void JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeSetTlsConfig(JNIEnv *env, jobject,
        jstring certFile, jstring keyFile, jstring caFile) {
    const char *cert = jstringToChar(env, certFile);
    const char *key = jstringToChar(env, keyFile);
    const char *ca = jstringToChar(env, caFile);
    dcmtk_set_tls_config(cert, key, ca);
    releaseString(env, certFile, cert);
    releaseString(env, keyFile, key);
    releaseString(env, caFile, ca);
}

extern "C" JNIEXPORT void JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeClearTlsConfig(JNIEnv *env, jobject) {
    dcmtk_clear_tls_config();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeIsTlsAvailable(JNIEnv *env, jobject) {
    return dcmtk_is_tls_available() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeIsTlsEnabled(JNIEnv *env, jobject) {
    return dcmtk_is_tls_enabled() ? JNI_TRUE : JNI_FALSE;
}

// ==================== MPR / MIP ====================

extern "C" JNIEXPORT jobject JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeBuildMprVolume(JNIEnv *env, jobject,
        jobjectArray filePaths) {
    int fileCount = env->GetArrayLength(filePaths);
    const char** cPaths = new const char*[fileCount];
    jstring* jPaths = new jstring[fileCount];
    for (int i = 0; i < fileCount; i++) {
        jPaths[i] = (jstring) env->GetObjectArrayElement(filePaths, i);
        cPaths[i] = env->GetStringUTFChars(jPaths[i], nullptr);
    }

    MprVolumeInfo* info = dcmtk_build_mpr_volume(cPaths, fileCount);

    for (int i = 0; i < fileCount; i++) {
        env->ReleaseStringUTFChars(jPaths[i], cPaths[i]);
    }
    delete[] cPaths;
    delete[] jPaths;

    jclass hashMapClass = env->FindClass("java/util/HashMap");
    jmethodID hashMapInit = env->GetMethodID(hashMapClass, "<init>", "()V");
    jmethodID hashMapPut = env->GetMethodID(hashMapClass, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject resultMap = env->NewObject(hashMapClass, hashMapInit);

    if (info->error) {
        env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("error"),
                safeNewStringUTF(env, info->error_message));
        dcmtk_free_mpr_volume_info(info);
        return resultMap;
    }

    jclass integerClass = env->FindClass("java/lang/Integer");
    jmethodID intValueOf = env->GetStaticMethodID(integerClass, "valueOf", "(I)Ljava/lang/Integer;");
    jclass doubleClass = env->FindClass("java/lang/Double");
    jmethodID doubleValueOf = env->GetStaticMethodID(doubleClass, "valueOf", "(D)Ljava/lang/Double;");

    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("volumeId"),
            env->CallStaticObjectMethod(integerClass, intValueOf, info->volume_id));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("width"),
            env->CallStaticObjectMethod(integerClass, intValueOf, info->width));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("height"),
            env->CallStaticObjectMethod(integerClass, intValueOf, info->height));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("depth"),
            env->CallStaticObjectMethod(integerClass, intValueOf, info->depth));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("pixelSpacingX"),
            env->CallStaticObjectMethod(doubleClass, doubleValueOf, info->pixel_spacing_x));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("pixelSpacingY"),
            env->CallStaticObjectMethod(doubleClass, doubleValueOf, info->pixel_spacing_y));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("sliceSpacing"),
            env->CallStaticObjectMethod(doubleClass, doubleValueOf, info->slice_spacing));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("windowCenter"),
            env->CallStaticObjectMethod(doubleClass, doubleValueOf, info->window_center));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("windowWidth"),
            env->CallStaticObjectMethod(doubleClass, doubleValueOf, info->window_width));

    dcmtk_free_mpr_volume_info(info);
    return resultMap;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeGetMprSlice(JNIEnv *env, jobject,
        jint volumeId, jint plane, jint sliceIndex, jdouble windowCenter, jdouble windowWidth) {

    MprSliceData* slice = dcmtk_get_mpr_slice(volumeId, plane, sliceIndex, windowCenter, windowWidth);

    jclass hashMapClass = env->FindClass("java/util/HashMap");
    jmethodID hashMapInit = env->GetMethodID(hashMapClass, "<init>", "()V");
    jmethodID hashMapPut = env->GetMethodID(hashMapClass, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject resultMap = env->NewObject(hashMapClass, hashMapInit);

    if (slice->error) {
        env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("error"),
                safeNewStringUTF(env, slice->error_message));
        dcmtk_free_mpr_slice_data(slice);
        return resultMap;
    }

    jclass integerClass = env->FindClass("java/lang/Integer");
    jmethodID intValueOf = env->GetStaticMethodID(integerClass, "valueOf", "(I)Ljava/lang/Integer;");

    int dataLen = slice->width * slice->height * 4;
    jbyteArray byteArray = env->NewByteArray(dataLen);
    env->SetByteArrayRegion(byteArray, 0, dataLen, (jbyte*)slice->data);

    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("data"), byteArray);
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("width"),
            env->CallStaticObjectMethod(integerClass, intValueOf, slice->width));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("height"),
            env->CallStaticObjectMethod(integerClass, intValueOf, slice->height));

    dcmtk_free_mpr_slice_data(slice);
    return resultMap;
}

extern "C" JNIEXPORT void JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeFreeMprVolume(JNIEnv *env, jobject, jint volumeId) {
    dcmtk_free_mpr_volume(volumeId);
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeRenderMip(JNIEnv *env, jobject,
        jint volumeId, jdouble rotationX, jdouble rotationY, jdouble windowCenter, jdouble windowWidth) {

    MprSliceData* mip = dcmtk_render_mip(volumeId, rotationX, rotationY, windowCenter, windowWidth);

    jclass hashMapClass = env->FindClass("java/util/HashMap");
    jmethodID hashMapInit = env->GetMethodID(hashMapClass, "<init>", "()V");
    jmethodID hashMapPut = env->GetMethodID(hashMapClass, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject resultMap = env->NewObject(hashMapClass, hashMapInit);

    if (mip->error) {
        env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("error"),
                safeNewStringUTF(env, mip->error_message));
        dcmtk_free_mpr_slice_data(mip);
        return resultMap;
    }

    jclass integerClass = env->FindClass("java/lang/Integer");
    jmethodID intValueOf = env->GetStaticMethodID(integerClass, "valueOf", "(I)Ljava/lang/Integer;");

    int dataLen = mip->width * mip->height * 4;
    jbyteArray byteArray = env->NewByteArray(dataLen);
    env->SetByteArrayRegion(byteArray, 0, dataLen, (jbyte*)mip->data);

    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("data"), byteArray);
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("width"),
            env->CallStaticObjectMethod(integerClass, intValueOf, mip->width));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("height"),
            env->CallStaticObjectMethod(integerClass, intValueOf, mip->height));

    dcmtk_free_mpr_slice_data(mip);
    return resultMap;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeRenderVolume(JNIEnv *env, jobject,
        jint volumeId, jdouble rotationX, jdouble rotationY, jdouble windowCenter, jdouble windowWidth,
        jstring preset, jboolean preview) {

    const char* presetName = jstringToChar(env, preset);
    MprSliceData* volume = dcmtk_render_volume(
            volumeId,
            rotationX,
            rotationY,
            windowCenter,
            windowWidth,
            presetName ? presetName : "Muscle",
            preview ? 1 : 0);
    releaseString(env, preset, presetName);

    jclass hashMapClass = env->FindClass("java/util/HashMap");
    jmethodID hashMapInit = env->GetMethodID(hashMapClass, "<init>", "()V");
    jmethodID hashMapPut = env->GetMethodID(hashMapClass, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject resultMap = env->NewObject(hashMapClass, hashMapInit);

    if (volume->error) {
        env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("error"),
                safeNewStringUTF(env, volume->error_message));
        dcmtk_free_mpr_slice_data(volume);
        return resultMap;
    }

    jclass integerClass = env->FindClass("java/lang/Integer");
    jmethodID intValueOf = env->GetStaticMethodID(integerClass, "valueOf", "(I)Ljava/lang/Integer;");

    int dataLen = volume->width * volume->height * 4;
    jbyteArray byteArray = env->NewByteArray(dataLen);
    env->SetByteArrayRegion(byteArray, 0, dataLen, (jbyte*)volume->data);

    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("data"), byteArray);
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("width"),
            env->CallStaticObjectMethod(integerClass, intValueOf, volume->width));
    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("height"),
            env->CallStaticObjectMethod(integerClass, intValueOf, volume->height));

    dcmtk_free_mpr_slice_data(volume);
    return resultMap;
}

// ==================== GSPS ====================

extern "C" JNIEXPORT jobject JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeCreateGsps(JNIEnv *env, jobject,
        jstring sourceDicomPath, jstring annotationsJson, jstring outputPath) {
    const char *src = jstringToChar(env, sourceDicomPath);
    const char *json = jstringToChar(env, annotationsJson);
    const char *out = jstringToChar(env, outputPath);

    MediaUploadResult* gspsResult = dcmtk_create_gsps(src, json, out);

    releaseString(env, sourceDicomPath, src);
    releaseString(env, annotationsJson, json);
    releaseString(env, outputPath, out);

    jclass hashMapClass = env->FindClass("java/util/HashMap");
    jmethodID hashMapInit = env->GetMethodID(hashMapClass, "<init>", "()V");
    jmethodID hashMapPut = env->GetMethodID(hashMapClass, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject resultMap = env->NewObject(hashMapClass, hashMapInit);

    jclass booleanClass = env->FindClass("java/lang/Boolean");
    jmethodID boolValueOf = env->GetStaticMethodID(booleanClass, "valueOf", "(Z)Ljava/lang/Boolean;");

    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("success"),
            env->CallStaticObjectMethod(booleanClass, boolValueOf, gspsResult->success ? JNI_TRUE : JNI_FALSE));

    if (!gspsResult->success) {
        env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("error"),
                safeNewStringUTF(env, gspsResult->error_message));
    } else {
        env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("studyInstanceUID"),
                safeNewStringUTF(env, gspsResult->study_instance_uid));
        env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("seriesInstanceUID"),
                safeNewStringUTF(env, gspsResult->series_instance_uid));
        env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("sopInstanceUID"),
                safeNewStringUTF(env, gspsResult->sop_instance_uid));
    }

    dcmtk_free_media_upload_result(gspsResult);
    return resultMap;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeParseGsps(JNIEnv *env, jobject, jstring filePath) {
    const char *path = jstringToChar(env, filePath);
    char* jsonStr = dcmtk_parse_gsps(path);
    releaseString(env, filePath, path);
    if (jsonStr) {
        jstring jResult = env->NewStringUTF(jsonStr);
        free(jsonStr);
        return jResult;
    }
    return nullptr;
}

// ==================== Convert Image to DICOM ====================

extern "C" JNIEXPORT jobject JNICALL
Java_com_dcmtk_flutter_DcmtkFlutterPlugin_nativeConvertImageToDicom(JNIEnv *env, jobject,
        jstring imagePath, jstring outputPath, jstring patientId, jstring patientName,
        jstring patientBirthDate, jstring studyDescription, jstring seriesDescription,
        jstring imageComments, jstring modality, jstring studyInstanceUID,
        jstring seriesInstanceUID, jint instanceNumber) {

    const char *cImagePath = jstringToChar(env, imagePath);
    const char *cOutputPath = jstringToChar(env, outputPath);
    const char *cPatientId = jstringToChar(env, patientId);
    const char *cPatientName = jstringToChar(env, patientName);
    const char *cBirthDate = jstringToChar(env, patientBirthDate);
    const char *cStudyDesc = jstringToChar(env, studyDescription);
    const char *cSeriesDesc = jstringToChar(env, seriesDescription);
    const char *cComments = jstringToChar(env, imageComments);
    const char *cModality = jstringToChar(env, modality);
    const char *cStudyUID = jstringToChar(env, studyInstanceUID);
    const char *cSeriesUID = jstringToChar(env, seriesInstanceUID);

    MediaUploadResult* convResult = dcmtk_convert_image_to_dicom(
        cImagePath, cOutputPath,
        cPatientId ? cPatientId : "",
        cPatientName ? cPatientName : "",
        cBirthDate ? cBirthDate : "",
        cStudyDesc ? cStudyDesc : "Exported Image",
        cSeriesDesc ? cSeriesDesc : "Exported Series",
        cComments ? cComments : "",
        cModality ? cModality : "SC",
        cStudyUID ? cStudyUID : "",
        cSeriesUID ? cSeriesUID : "",
        instanceNumber
    );

    releaseString(env, imagePath, cImagePath);
    releaseString(env, outputPath, cOutputPath);
    releaseString(env, patientId, cPatientId);
    releaseString(env, patientName, cPatientName);
    releaseString(env, patientBirthDate, cBirthDate);
    releaseString(env, studyDescription, cStudyDesc);
    releaseString(env, seriesDescription, cSeriesDesc);
    releaseString(env, imageComments, cComments);
    releaseString(env, modality, cModality);
    releaseString(env, studyInstanceUID, cStudyUID);
    releaseString(env, seriesInstanceUID, cSeriesUID);

    jclass hashMapClass = env->FindClass("java/util/HashMap");
    jmethodID hashMapInit = env->GetMethodID(hashMapClass, "<init>", "()V");
    jmethodID hashMapPut = env->GetMethodID(hashMapClass, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject resultMap = env->NewObject(hashMapClass, hashMapInit);

    jclass booleanClass = env->FindClass("java/lang/Boolean");
    jmethodID boolValueOf = env->GetStaticMethodID(booleanClass, "valueOf", "(Z)Ljava/lang/Boolean;");

    env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("success"),
            env->CallStaticObjectMethod(booleanClass, boolValueOf, convResult->success ? JNI_TRUE : JNI_FALSE));

    if (!convResult->success) {
        env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("error"),
                safeNewStringUTF(env, convResult->error_message));
    } else {
        env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("studyInstanceUID"),
                safeNewStringUTF(env, convResult->study_instance_uid));
        env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("seriesInstanceUID"),
                safeNewStringUTF(env, convResult->series_instance_uid));
        env->CallObjectMethod(resultMap, hashMapPut, env->NewStringUTF("sopInstanceUID"),
                safeNewStringUTF(env, convResult->sop_instance_uid));
    }

    dcmtk_free_media_upload_result(convResult);
    return resultMap;
}