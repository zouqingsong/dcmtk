#import "DcmtkFlutterPlugin.h"

// Import C functions from the DCMTK wrapper
#ifdef __cplusplus
extern "C" {
#endif
    const char* dcmtk_load_dicom_file(const char* filename);
    void dcmtk_free_string(const char* str);
#ifdef __cplusplus
}
#endif

@implementation DcmtkFlutterPlugin
+ (void)registerWithRegistrar:(NSObject<FlutterPluginRegistrar>*)registrar {
  FlutterMethodChannel* channel = [FlutterMethodChannel
      methodChannelWithName:@"dcmtk_flutter"
            binaryMessenger:[registrar messenger]];
  DcmtkFlutterPlugin* instance = [[DcmtkFlutterPlugin alloc] init];
  [registrar addMethodCallDelegate:instance channel:channel];
}

- (void)handleMethodCall:(FlutterMethodCall*)call result:(FlutterResult)result {
  if ([@"loadDicomFile" isEqualToString:call.method]) {
    NSString* filePath = call.arguments[@"filePath"];
    if (filePath == nil || filePath.length == 0) {
      result([FlutterError errorWithCode:@"INVALID_ARGUMENT"
                                 message:@"File path is required"
                                 details:nil]);
      return;
    }
    
    const char* cFilePath = [filePath UTF8String];
    const char* resultStr = dcmtk_load_dicom_file(cFilePath);
    
    NSString* resultNSString = [NSString stringWithUTF8String:resultStr];
    dcmtk_free_string(resultStr);
    
    result(resultNSString);
  } else {
    result(FlutterMethodNotImplemented);
  }
}

@end
