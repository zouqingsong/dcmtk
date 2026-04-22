cmake_minimum_required(VERSION 3.22.1)
project("dcmtk_flutter")

# Find the Flutter library
find_library(log-lib log)

# Paths
set(DCMTK_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}")
set(DCMTK_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(WRAPPER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../ios/native")

# Create the shared library
add_library(dcmtk_flutter SHARED
    dcmtk_flutter_jni.cpp
    ${WRAPPER_DIR}/dcmtk_flutter_wrapper.cpp)

# Include paths: DCMTK headers + wrapper header
target_include_directories(dcmtk_flutter PRIVATE
    ${DCMTK_INCLUDE_DIR}
    ${WRAPPER_DIR})

# Link all required DCMTK static libraries (order matters for static linking)
target_link_libraries(dcmtk_flutter
    ${log-lib}
    ${DCMTK_LIB_DIR}/libdcmnet.a
    ${DCMTK_LIB_DIR}/libdcmtls.a
    ${DCMTK_LIB_DIR}/libdcmdata.a
    ${DCMTK_LIB_DIR}/libi2d.a
    ${DCMTK_LIB_DIR}/libdcmjpeg.a
    ${DCMTK_LIB_DIR}/libdcmjpls.a
    ${DCMTK_LIB_DIR}/libdcmtkcharls.a
    ${DCMTK_LIB_DIR}/libijg8.a
    ${DCMTK_LIB_DIR}/libijg12.a
    ${DCMTK_LIB_DIR}/libijg16.a
    ${DCMTK_LIB_DIR}/libdcmimage.a
    ${DCMTK_LIB_DIR}/libdcmimgle.a
    ${DCMTK_LIB_DIR}/libdcmiod.a
    ${DCMTK_LIB_DIR}/libdcmsr.a
    ${DCMTK_LIB_DIR}/libdcmfg.a
    ${DCMTK_LIB_DIR}/libdcmpstat.a
    ${DCMTK_LIB_DIR}/libdcmqrdb.a
    ${DCMTK_LIB_DIR}/libdcmseg.a
    ${DCMTK_LIB_DIR}/libdcmpmap.a
    ${DCMTK_LIB_DIR}/libdcmect.a
    ${DCMTK_LIB_DIR}/libdcmtract.a
    ${DCMTK_LIB_DIR}/libdcmrt.a
    ${DCMTK_LIB_DIR}/libdcmwlm.a
    ${DCMTK_LIB_DIR}/libdcmxml.a
    ${DCMTK_LIB_DIR}/libdcmdsig.a
    ${DCMTK_LIB_DIR}/libcmr.a
    ${DCMTK_LIB_DIR}/liboficonv.a
    ${DCMTK_LIB_DIR}/liboflog.a
    ${DCMTK_LIB_DIR}/libofstd.a
    z)

# Link OpenSSL if available
if(EXISTS "${DCMTK_LIB_DIR}/libssl.a")
    target_link_libraries(dcmtk_flutter
        ${DCMTK_LIB_DIR}/libssl.a
        ${DCMTK_LIB_DIR}/libcrypto.a)
    target_include_directories(dcmtk_flutter PRIVATE
        ${DCMTK_INCLUDE_DIR})
    message(STATUS "OpenSSL found - TLS support enabled")
else()
    message(STATUS "OpenSSL not found - TLS support disabled")
endif()
