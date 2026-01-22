# iOS toolchain file for DCMTK
set(CMAKE_SYSTEM_NAME iOS)
set(CMAKE_SYSTEM_VERSION 11.0)

# Set the target architecture
if(NOT IOS_PLATFORM)
    set(IOS_PLATFORM "OS") # Can be OS, SIMULATOR, or WATCHOS
endif()

if(IOS_PLATFORM STREQUAL "OS")
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(CMAKE_SYSTEM_PROCESSOR "arm64")
elseif(IOS_PLATFORM STREQUAL "SIMULATOR")
    set(CMAKE_OSX_ARCHITECTURES "arm64")  # arm64 for Apple Silicon Macs
    set(CMAKE_SYSTEM_PROCESSOR "arm64")
endif()

set(CMAKE_OSX_DEPLOYMENT_TARGET ${CMAKE_SYSTEM_VERSION})

# Find the iOS SDK
execute_process(
    COMMAND xcrun --sdk iphoneos --show-sdk-path
    OUTPUT_VARIABLE CMAKE_OSX_SYSROOT_OS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND xcrun --sdk iphonesimulator --show-sdk-path
    OUTPUT_VARIABLE CMAKE_OSX_SYSROOT_SIMULATOR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(IOS_PLATFORM STREQUAL "OS")
    set(CMAKE_OSX_SYSROOT ${CMAKE_OSX_SYSROOT_OS})
elseif(IOS_PLATFORM STREQUAL "SIMULATOR")
    set(CMAKE_OSX_SYSROOT ${CMAKE_OSX_SYSROOT_SIMULATOR})
endif()

# Set up compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fembed-bitcode")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fembed-bitcode")

# Compiler configuration
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

# DCMTK specific options for iOS
set(DCMTK_WITH_TIFF OFF CACHE BOOL "Disable TIFF support for mobile" FORCE)
set(DCMTK_WITH_PNG OFF CACHE BOOL "Disable PNG support for mobile" FORCE)
set(DCMTK_WITH_XML OFF CACHE BOOL "Disable XML support for mobile" FORCE)
set(DCMTK_WITH_ZLIB ON CACHE BOOL "Enable ZLIB support" FORCE)
set(DCMTK_WITH_OPENSSL OFF CACHE BOOL "Disable OpenSSL for mobile" FORCE)
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