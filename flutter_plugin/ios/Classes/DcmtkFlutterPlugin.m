#import "DcmtkFlutterPlugin.h"

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
    // Native DCMTK method call will be implemented here
    result(@"Method not implemented yet");
  } else {
    result(FlutterMethodNotImplemented);
  }
}

@end