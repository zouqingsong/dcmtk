Pod::Spec.new do |s|
  s.name             = 'dcmtk_flutter'
  s.version          = '1.0.0'
  s.summary          = 'DCMTK Flutter plugin for macOS.'
  s.description      = <<-DESC
DCMTK (DICOM toolkit) Flutter plugin providing access to DICOM file processing capabilities on macOS.
                       DESC
  s.homepage         = 'https://github.com/zouqingsong/dcmtk'
  s.license          = { :file => '../LICENSE' }
  s.author           = { 'DOI Technology' => 'info@doitech.com' }
  s.source           = { :path => '.' }

  s.source_files = 'Classes/**/*', 'native/dcmtk_flutter_wrapper.h', 'native/dcmtk_flutter_wrapper.cpp'
  s.public_header_files = 'Classes/**/*.h', 'native/dcmtk_flutter_wrapper.h'

  s.dependency 'FlutterMacOS'
  s.platform = :osx, '10.15'
  s.osx.deployment_target = '10.15'

  s.static_framework = true

  # Link the pre-built DCMTK static library
  s.vendored_libraries = 'Libs/libdcmtk_all.a', 'Libs/libssl.a', 'Libs/libcrypto.a'

  s.libraries = 'c++', 'z', 'iconv'

  s.pod_target_xcconfig = {
    'DEFINES_MODULE' => 'YES',
    'CLANG_CXX_LANGUAGE_STANDARD' => 'c++11',
    'CLANG_CXX_LIBRARY' => 'libc++',
    'HEADER_SEARCH_PATHS' => '$(inherited) "${PODS_TARGET_SRCROOT}/Headers"',
    'OTHER_LDFLAGS' => '$(inherited) -ObjC',
    'OTHER_CFLAGS' => '$(inherited) -Wno-documentation -Wno-documentation-html'
  }
  s.swift_version = '5.0'
end
