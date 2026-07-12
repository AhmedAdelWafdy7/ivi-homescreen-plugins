// C-API entry points for the chromium_dart_view platform-view bridge.

#include <string>
#include <vector>

#include <flutter/plugin_registrar.h>

#include "chromium_dart_view/chromium_dart_view_plugin_c_api.h"
#include "flutter_homescreen.h"
#include "platform_views/platform_view.h"

namespace plugin_chromium_dart_view {
class ChromiumDartViewPlugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrar* registrar);
  static void PlatformViewCreate(int32_t id,
                                 std::string viewType,
                                 int32_t direction,
                                 double top,
                                 double left,
                                 double width,
                                 double height,
                                 const std::vector<uint8_t>& params,
                                 std::string assetDirectory,
                                 FlutterDesktopEngineRef engine,
                                 PlatformViewAddListener addListener,
                                 PlatformViewRemoveListener removeListener,
                                 void* platform_view_context);
};
}  // namespace plugin_chromium_dart_view

void ChromiumDartViewPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrar* registrar) {
  plugin_chromium_dart_view::ChromiumDartViewPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrar>(registrar));
}

void ChromiumDartViewPluginCApiPlatformViewCreate(
    FlutterDesktopPluginRegistrar* /* registrar */,
    const int32_t id,
    std::string viewType,
    const int32_t direction,
    const double top,
    const double left,
    const double width,
    const double height,
    const std::vector<uint8_t>& params,
    std::string assetDirectory,
    FlutterDesktopEngineRef engine,
    const PlatformViewAddListener add_listener,
    const PlatformViewRemoveListener remove_listener,
    void* platform_views_context) {
  plugin_chromium_dart_view::ChromiumDartViewPlugin::PlatformViewCreate(
      id, std::move(viewType), direction, top, left, width, height, params,
      std::move(assetDirectory), engine, add_listener, remove_listener,
      platform_views_context);
}
