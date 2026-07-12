#ifndef FLUTTER_PLUGIN_CHROMIUM_DART_VIEW_PLUGIN_C_API_H
#define FLUTTER_PLUGIN_CHROMIUM_DART_VIEW_PLUGIN_C_API_H

#include <flutter_plugin_registrar.h>
#include "flutter_homescreen.h"
#include "platform_views/platform_view_listener.h"

#include <string>
#include <vector>

#define FLUTTER_PLUGIN_EXPORT __attribute__((visibility("default")))

#if defined(__cplusplus)
extern "C" {
#endif

FLUTTER_PLUGIN_EXPORT void ChromiumDartViewPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrar* registrar);

FLUTTER_PLUGIN_EXPORT void ChromiumDartViewPluginCApiPlatformViewCreate(
    FlutterDesktopPluginRegistrar* registrar,
    int32_t id,
    std::string viewType,
    int32_t direction,
    double top,
    double left,
    double width,
    double height,
    const std::vector<uint8_t>& params,
    std::string assetDirectory,
    FlutterDesktopEngineRef engine,
    PlatformViewAddListener add_listener,
    PlatformViewRemoveListener remove_listener,
    void* platform_views_context);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif  // FLUTTER_PLUGIN_CHROMIUM_DART_VIEW_PLUGIN_C_API_H
