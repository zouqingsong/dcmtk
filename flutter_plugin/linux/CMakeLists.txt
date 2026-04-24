cmake_minimum_required(VERSION 3.15)
set(PROJECT_NAME "dcmtk_flutter")
project(${PROJECT_NAME} LANGUAGES CXX)

set(PLUGIN_NAME "dcmtk_flutter_plugin")

add_library(${PLUGIN_NAME} SHARED
  "dcmtk_flutter_plugin.cc"
  "../macos/native/dcmtk_flutter_wrapper.cpp"
  "../macos/native/dcmtk_flutter_wrapper.h"
)

apply_standard_settings(${PLUGIN_NAME})

set_target_properties(${PLUGIN_NAME} PROPERTIES
  CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)

target_include_directories(${PLUGIN_NAME} PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(${PLUGIN_NAME} PRIVATE flutter)
target_link_libraries(${PLUGIN_NAME} PRIVATE PkgConfig::GTK)

# === DCMTK Configuration ===
set(DCMTK_HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Headers")
set(DCMTK_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Libs")

target_include_directories(${PLUGIN_NAME} PRIVATE
  "${DCMTK_HEADERS_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../macos/native"
)

target_compile_options(${PLUGIN_NAME} PRIVATE
  -Wno-deprecated-declarations
  -Wno-unused-variable
)

target_compile_definitions(${PLUGIN_NAME} PRIVATE
  DCMTK_INSIDE_LOG4CPLUS=1
)

# Link combined DCMTK static library
set(DCMTK_COMBINED_LIB "${DCMTK_LIBS_DIR}/libdcmtk_all.a")
if(EXISTS "${DCMTK_COMBINED_LIB}")
  target_link_libraries(${PLUGIN_NAME} PRIVATE "${DCMTK_COMBINED_LIB}")
else()
  # Fall back to individual libraries
  file(GLOB DCMTK_LIBRARIES "${DCMTK_LIBS_DIR}/lib*.a")
  if(DCMTK_LIBRARIES)
    # Filter out OpenSSL libs to link them separately
    set(DCMTK_ONLY_LIBS "")
    foreach(LIB ${DCMTK_LIBRARIES})
      get_filename_component(LIB_NAME ${LIB} NAME)
      if(NOT LIB_NAME MATCHES "^lib(ssl|crypto)")
        list(APPEND DCMTK_ONLY_LIBS ${LIB})
      endif()
    endforeach()
    target_link_libraries(${PLUGIN_NAME} PRIVATE ${DCMTK_ONLY_LIBS})
  else()
    message(WARNING "No DCMTK libraries found in ${DCMTK_LIBS_DIR}. "
      "Build DCMTK first: see build_mobile/scripts/build_linux.sh")
  endif()
endif()

# System libraries required by DCMTK on Linux
target_link_libraries(${PLUGIN_NAME} PRIVATE
  z         # zlib
  pthread   # threading
)

# OpenSSL support
set(OPENSSL_LIBS_DIR "${DCMTK_LIBS_DIR}")
if(EXISTS "${OPENSSL_LIBS_DIR}/libssl.a")
  target_link_libraries(${PLUGIN_NAME} PRIVATE
    "${OPENSSL_LIBS_DIR}/libssl.a"
    "${OPENSSL_LIBS_DIR}/libcrypto.a"
  )
  target_compile_definitions(${PLUGIN_NAME} PRIVATE WITH_OPENSSL)
  if(EXISTS "${DCMTK_HEADERS_DIR}/openssl")
    target_include_directories(${PLUGIN_NAME} PRIVATE "${DCMTK_HEADERS_DIR}/openssl/..")
  endif()
elseif(PKG_CONFIG_FOUND)
  pkg_check_modules(OPENSSL openssl)
  if(OPENSSL_FOUND)
    target_link_libraries(${PLUGIN_NAME} PRIVATE ${OPENSSL_LIBRARIES})
    target_include_directories(${PLUGIN_NAME} PRIVATE ${OPENSSL_INCLUDE_DIRS})
    target_compile_definitions(${PLUGIN_NAME} PRIVATE WITH_OPENSSL)
  endif()
endif()

set(dcmtk_flutter_bundled_libraries
  ""
  PARENT_SCOPE
)
