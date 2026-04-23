#ifndef FLUTTER_PLUGIN_DCMTK_FLUTTER_PLUGIN_H_
#define FLUTTER_PLUGIN_DCMTK_FLUTTER_PLUGIN_H_

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <memory>

namespace dcmtk_flutter {

class DcmtkFlutterPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

  DcmtkFlutterPlugin();
  virtual ~DcmtkFlutterPlugin();

  // Disallow copy and assign.
  DcmtkFlutterPlugin(const DcmtkFlutterPlugin&) = delete;
  DcmtkFlutterPlugin& operator=(const DcmtkFlutterPlugin&) = delete;

 private:
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
};

}  // namespace dcmtk_flutter

#endif  // FLUTTER_PLUGIN_DCMTK_FLUTTER_PLUGIN_H_
