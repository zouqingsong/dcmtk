# Android toolchain file for DCMTK
set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION 28) # API level (required for getlogin_r)

# Set the target architecture
if(NOT ANDROID_ABI)
    set(ANDROID_ABI "arm64-v8a")
endif()

# Set up NDK path - adjust as needed
if(NOT ANDROID_NDK)
    set(ANDROID_NDK "$ENV{ANDROID_NDK_ROOT}")
    if(NOT ANDROID_NDK)
        message(FATAL_ERROR "ANDROID_NDK must be set to the Android NDK directory")
    endif()
endif()

# Calculate Android SDK root from NDK path for emulator support
get_filename_component(ANDROID_SDK_ROOT "${ANDROID_NDK}/../.." ABSOLUTE)
set(ANDROID_SDK_ROOT "${ANDROID_SDK_ROOT}" CACHE PATH "Android SDK root directory" FORCE)

set(CMAKE_ANDROID_NDK ${ANDROID_NDK})
set(CMAKE_ANDROID_API_MIN 28)
set(CMAKE_ANDROID_API ${CMAKE_SYSTEM_VERSION})
set(CMAKE_ANDROID_ARCH_ABI ${ANDROID_ABI})

# Set the STL to c++_shared
set(CMAKE_ANDROID_STL_TYPE c++_shared)

# Compiler configuration
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

# DCMTK specific options for Android
set(DCMTK_WITH_TIFF OFF CACHE BOOL "Disable TIFF support for mobile" FORCE)
set(DCMTK_WITH_PNG OFF CACHE BOOL "Disable PNG support for mobile" FORCE)
set(DCMTK_WITH_XML OFF CACHE BOOL "Disable XML support for mobile" FORCE)
set(DCMTK_WITH_ZLIB ON CACHE BOOL "Enable ZLIB support" FORCE)
# OpenSSL: controlled by build script via -D flags, do not force here
set(DCMTK_WITH_SNDFILE OFF CACHE BOOL "Disable SNDFILE support" FORCE)
set(DCMTK_WITH_ICONV OFF CACHE BOOL "Disable ICONV for mobile" FORCE)
set(DCMTK_WITH_WRAP OFF CACHE BOOL "Disable WRAP support" FORCE)
set(DCMTK_WITH_OPENJPEG OFF CACHE BOOL "Disable OpenJPEG for mobile" FORCE)
set(DCMTK_WITH_THREADS ON CACHE BOOL "Enable multi-threading" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
set(DCMTK_LINK_STATIC ON CACHE BOOL "Link statically" FORCE)

# Disable applications and tests for mobile
set(BUILD_APPS OFF CACHE BOOL "Disable applications for mobile" FORCE)
set(DCMTK_WITH_DOXYGEN OFF CACHE BOOL "Disable documentation" FORCE)

# Configure testing and emulator support for mobile
set(BUILD_TESTING OFF CACHE BOOL "Disable testing for mobile" FORCE)
# Note: Emulator startup is skipped when BUILD_TESTING is OFF