cmake_minimum_required(VERSION 3.15)
set(PROJECT_NAME "dcmtk_flutter")
project(${PROJECT_NAME} LANGUAGES CXX)

# This value is used when generating builds using this plugin, so it must
# not be changed
set(PLUGIN_NAME "dcmtk_flutter_plugin")

add_library(${PLUGIN_NAME} SHARED
  "dcmtk_flutter_plugin.cpp"
  "dcmtk_flutter_plugin.h"
  "dcmtk_flutter_plugin_c_api.cpp"
  "dcmtk_flutter_plugin_c_api.h"
  "../macos/native/dcmtk_flutter_wrapper.cpp"
  "../macos/native/dcmtk_flutter_wrapper.h"
)

# Apply a standard set of build settings that are configured in the
# application-level CMakeLists.txt. This can be removed for plugins
# that want to apply fine-grained settings.
apply_standard_settings(${PLUGIN_NAME})

# Symbols are hidden by default to reduce the chance of accidental conflicts
# between plugins. This should not be removed; any symbols that should be
# exported should be explicitly exported with the FLUTTER_PLUGIN_EXPORT macro.
set_target_properties(${PLUGIN_NAME} PROPERTIES
  CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)

# Source include directories and library dependencies.
target_include_directories(${PLUGIN_NAME} INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(${PLUGIN_NAME} PRIVATE flutter flutter_wrapper_plugin)

# === DCMTK Configuration ===
# Pre-built DCMTK headers and libraries for Windows
set(DCMTK_HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Headers")
set(DCMTK_LIBS_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Libs")

# Include DCMTK headers
target_include_directories(${PLUGIN_NAME} PRIVATE
  "${DCMTK_HEADERS_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../macos/native"
)

# Suppress some warnings from DCMTK code
target_compile_options(${PLUGIN_NAME} PRIVATE
  /wd4267  # conversion from 'size_t' to 'int', possible loss of data
  /wd4244  # conversion from 'type1' to 'type2', possible loss of data
  /wd4996  # deprecated functions
)

# Windows-specific defines for DCMTK
target_compile_definitions(${PLUGIN_NAME} PRIVATE
  _WIN32
  NOMINMAX
  WIN32_LEAN_AND_MEAN
  DCMTK_INSIDE_LOG4CPLUS=1
)

# Link pre-built DCMTK static libraries (config-specific)
# Glob from Release dir to get the list of lib names, then link using generator expression
file(GLOB DCMTK_LIB_NAMES RELATIVE "${DCMTK_LIBS_BASE_DIR}/Release" "${DCMTK_LIBS_BASE_DIR}/Release/*.lib")

if(DCMTK_LIB_NAMES)
  foreach(LIB_NAME ${DCMTK_LIB_NAMES})
    target_link_libraries(${PLUGIN_NAME} PRIVATE
      "$<IF:$<CONFIG:Debug>,${DCMTK_LIBS_BASE_DIR}/Debug/${LIB_NAME},${DCMTK_LIBS_BASE_DIR}/Release/${LIB_NAME}>"
    )
  endforeach()
else()
  message(WARNING "No DCMTK libraries found in ${DCMTK_LIBS_BASE_DIR}/Release. "
    "Build DCMTK first: see build_mobile/scripts/build_windows.ps1")
endif()

# Windows system libraries needed by DCMTK
target_link_libraries(${PLUGIN_NAME} PRIVATE
  ws2_32      # Winsock (networking)
  iphlpapi    # IP Helper (network interface enumeration)
  netapi32    # Network management
  advapi32    # Advanced Windows API
)

# OpenSSL (optional, for TLS support)
set(OPENSSL_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Libs/openssl")
if(EXISTS "${OPENSSL_LIBS_DIR}")
  file(GLOB OPENSSL_LIBRARIES "${OPENSSL_LIBS_DIR}/*.lib")
  if(OPENSSL_LIBRARIES)
    target_link_libraries(${PLUGIN_NAME} PRIVATE ${OPENSSL_LIBRARIES})
    target_compile_definitions(${PLUGIN_NAME} PRIVATE WITH_OPENSSL)
    target_include_directories(${PLUGIN_NAME} PRIVATE "${OPENSSL_LIBS_DIR}/include")
    # OpenSSL on Windows needs crypt32
    target_link_libraries(${PLUGIN_NAME} PRIVATE crypt32)
  endif()
endif()

# zlib (usually bundled with DCMTK on Windows)
# If using system zlib, link it:
# target_link_libraries(${PLUGIN_NAME} PRIVATE zlibstatic)

# List of absolute paths to libraries that should be bundled with the plugin.
# This is used by the Flutter tool to copy the DLL into the output directory.
set(dcmtk_flutter_bundled_libraries
  ""
  PARENT_SCOPE
)
