cmake_minimum_required(VERSION 3.10.2)
project("dcmtk_flutter")

# Find the Flutter library
find_library(log-lib log)

# Add the DCMTK libraries
set(DCMTK_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}")

# Create the shared library
add_library(dcmtk_flutter SHARED
    dcmtk_flutter_jni.cpp)

# Link the libraries
target_link_libraries(dcmtk_flutter
    ${log-lib}
    ${DCMTK_LIB_DIR}/libdcmdata.a
    ${DCMTK_LIB_DIR}/libofstd.a
    ${DCMTK_LIB_DIR}/liboflog.a
    z)
