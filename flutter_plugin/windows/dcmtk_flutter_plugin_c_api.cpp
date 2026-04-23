#include "include/dcmtk_flutter/dcmtk_flutter_plugin_c_api.h"

#include <flutter/plugin_registrar_windows.h>

#include "dcmtk_flutter_plugin.h"

void DcmtkFlutterPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  dcmtk_flutter::DcmtkFlutterPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
