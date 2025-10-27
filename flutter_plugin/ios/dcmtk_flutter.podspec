Pod::Spec.new do |s|
  s.name             = 'dcmtk_flutter'
  s.version          = '1.0.0'
  s.summary          = 'DCMTK Flutter plugin for iOS and Android.'
  s.description      = <<-DESC
DCMTK (DICOM toolkit) Flutter plugin providing access to DICOM file processing capabilities.
                       DESC
  s.homepage         = 'https://github.com/zouqingsong/dcmtk'
  s.license          = { :file => '../LICENSE' }
  s.author           = { 'Your Company' => 'email@example.com' }
  s.source           = { :path => '.' }
  s.source_files = 'Classes/**/*'
  s.dependency 'Flutter'
  s.platform = :ios, '11.0'
  s.ios.deployment_target = '11.0'

  # Flutter.framework does not contain a i386 slice.
  s.pod_target_xcconfig = { 'DEFINES_MODULE' => 'YES', 'EXCLUDED_ARCHS[sdk=iphonesimulator*]' => 'i386' }
  s.swift_version = '5.0'

  # Add the DCMTK static libraries
  s.vendored_libraries = 'Frameworks/libdcmtk.a'
  s.libraries = 'c++', 'z', 'iconv'
  
  # Required for C++ support
  s.xcconfig = {
    'CLANG_CXX_LANGUAGE_STANDARD' => 'c++11',
    'CLANG_CXX_LIBRARY' => 'libc++'
  }
end