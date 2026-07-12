// chromium_dart_view — thin ivi-homescreen platform-view bridge for the
// chromium_dart FFI package.
//
// It does the ONE thing pure-Dart-FFI cannot: obtain the ivi-homescreen
// FlutterView's wl_display + base wl_surface + rect, and hand them to the
// chromium_dart .so's C ABI (chromium_browser_attach_surface). ALL CEF +
// wayland-cxx-scanner rendering lives in libchromium_nc.so; this file is glue.
//
// The .so is loaded into this process by the Dart FFI layer, so we resolve its
// symbols with dlsym(RTLD_DEFAULT). We correlate to the browser the Dart
// controller created via chromium_last_browser() (single active webview).

#include <dlfcn.h>

#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <flutter/plugin_registrar.h>

#include "plugins/common/common.h"

#include "flutter_desktop_engine_state.h"
#include "platform_views/platform_view.h"
#include "view/flutter_view.h"
#include "wayland/display.h"
#include "wayland/window.h"

namespace plugin_chromium_dart_view {

class ChromiumDartViewPlugin;  // owns the live views; erases on dispose

namespace {
using AttachFn = void (*)(void*, void*, void*, int32_t, int32_t, int32_t,
                          int32_t);
using DetachFn = void (*)(void*);
using LastBrowserFn = void* (*)();
using SetGeometryFn = void (*)(void*, int32_t, int32_t, int32_t, int32_t);

// Dart's DynamicLibrary.open loads libchromium_nc.so with RTLD_LOCAL, so its
// symbols are NOT in the global scope — dlsym(RTLD_DEFAULT) can't see them.
// dlopen the same file (RTLD_NOLOAD returns the already-loaded handle; it is the
// same instance, so shared globals like g_last_browser match) and resolve from
// that handle.
void* chromium_lib_handle() {
  static void* handle = [] {
    const char* p = std::getenv("CHROMIUM_DART_LIB");
    if (p == nullptr || *p == '\0') p = "libchromium_nc.so";
    void* h = dlopen(p, RTLD_NOW | RTLD_NOLOAD);
    if (h == nullptr) h = dlopen(p, RTLD_NOW);
    return h;
  }();
  return handle;
}

template <typename T>
T sym(const char* name) {
  void* const h = chromium_lib_handle();
  return reinterpret_cast<T>(h ? dlsym(h, name) : dlsym(RTLD_DEFAULT, name));
}
}  // namespace

class ChromiumDartView final : public PlatformView {
 public:
  ChromiumDartView(int32_t id,
                   std::string viewType,
                   int32_t direction,
                   double top,
                   double left,
                   double width,
                   double height,
                   const std::vector<uint8_t>& /* params */,
                   std::string /* assetDirectory */,
                   FlutterDesktopEngineState* state,
                   PlatformViewAddListener addListener,
                   PlatformViewRemoveListener removeListener,
                   void* platform_view_context)
      : PlatformView(id,
                     std::move(viewType),
                     direction,
                     top,
                     left,
                     width,
                     height),
        id_(id),
        platformViewsContext_(platform_view_context),
        removeListener_(removeListener),
        width_(static_cast<int32_t>(width)),
        height_(static_cast<int32_t>(height)) {
    const auto flutter_view = state->view_controller->view;
    wl_display* display = flutter_view->GetDisplay()->GetDisplay();
    wl_surface* parent = flutter_view->GetWindow()->GetBaseSurface();

    const auto attach = sym<AttachFn>("chromium_browser_attach_surface");
    const auto last = sym<LastBrowserFn>("chromium_last_browser");
    browser_ = last ? last() : nullptr;

    if (attach && browser_ && display && parent) {
      attach(browser_, display, parent, static_cast<int32_t>(left),
             static_cast<int32_t>(top), width_, height_);
      ihs::log::info("[chromium_dart_view] attached {}x{} at {},{}", width_,
                   height_, left, top);
    } else {
      ihs::log::error(
          "[chromium_dart_view] attach unavailable (browser={} attach={} "
          "display={} parent={}) — is libchromium_nc.so the CEF backend?",
          browser_, reinterpret_cast<void*>(attach),
          static_cast<void*>(display), static_cast<void*>(parent));
    }
    addListener(platformViewsContext_, id, &listener_, this);
  }

  ~ChromiumDartView() override {
    // Tear down the CEF-owned subsurface so the webview does not linger in the
    // scene after its Flutter widget is disposed (e.g. navigating away). Runs on
    // the platform-view thread, which owns the Wayland event loop.
    if (browser_) {
      if (const auto detach = sym<DetachFn>("chromium_browser_detach_surface"))
        detach(browser_);
    }
    removeListener_(platformViewsContext_, id_);
  }

 private:
  int32_t id_;
  void* platformViewsContext_;
  PlatformViewRemoveListener removeListener_;
  void* browser_ = nullptr;
  int32_t width_, height_;

  static void on_resize(double width, double height, void* data) {
    auto* self = static_cast<ChromiumDartView*>(data);
    self->width_ = static_cast<int32_t>(width);
    self->height_ = static_cast<int32_t>(height);
    if (const auto setgeo = sym<SetGeometryFn>("chromium_browser_set_geometry");
        setgeo && self->browser_)
      setgeo(self->browser_, 0, 0, self->width_, self->height_);
  }
  static void on_set_direction(int32_t /*direction*/, void* /*data*/) {}
  static void on_set_offset(double /*left*/, double /*top*/, void* /*data*/) {
    // Subsurface reposition on scroll is a follow-up (needs a set_position ABI).
  }
  static void on_touch(int32_t /*action*/,
                       int32_t /*point_count*/,
                       size_t /*point_data_size*/,
                       const double* /*point_data*/,
                       void* /*data*/) {
    // Input forwarding is a follow-up (chromium_browser_send_* over the ABI).
  }
  // Flutter disposed the platform view: destroy the ChromiumDartView (its
  // destructor detaches the CEF subsurface). Defined out-of-line below because it
  // reaches into ChromiumDartViewPlugin's live-view map.
  static void on_dispose(bool hybrid, void* data);

  static const platform_view_listener listener_;
};

const platform_view_listener ChromiumDartView::listener_ = {
    .resize = on_resize,
    .set_direction = on_set_direction,
    .set_offset = on_set_offset,
    .on_touch = on_touch,
    .dispose = on_dispose,
    .accept_gesture = nullptr,
    .reject_gesture = nullptr,
};

class ChromiumDartViewPlugin final : public flutter::Plugin {
 public:
  // Defined out-of-line (below) so they are emitted for the C-API TU to link.
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

  // Destroy the view with this id (runs ~ChromiumDartView → detach). Safe if the
  // id is unknown. Called from ChromiumDartView::on_dispose.
  static void RemoveView(int32_t id);

 private:
  static std::map<int32_t, std::unique_ptr<ChromiumDartView>> m_views;
};

std::map<int32_t, std::unique_ptr<ChromiumDartView>>
    ChromiumDartViewPlugin::m_views;

void ChromiumDartViewPlugin::RemoveView(int32_t id) { m_views.erase(id); }

// data is the ChromiumDartView*; erasing it from the map destroys it (and thus
// detaches the subsurface + removes its listener). We must not touch the object
// after the erase — this static frame is all that remains valid.
void ChromiumDartView::on_dispose(bool /*hybrid*/, void* data) {
  if (auto* self = static_cast<ChromiumDartView*>(data)) {
    ihs::log::info("[chromium_dart_view] dispose id={} — detaching subsurface",
                 self->id_);
    ChromiumDartViewPlugin::RemoveView(self->id_);
  }
}

void ChromiumDartViewPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrar* /*registrar*/) {}

void ChromiumDartViewPlugin::PlatformViewCreate(
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
    PlatformViewAddListener addListener,
    PlatformViewRemoveListener removeListener,
    void* platform_view_context) {
  m_views[id] = std::make_unique<ChromiumDartView>(
      id, std::move(viewType), direction, top, left, width, height, params,
      std::move(assetDirectory), engine, addListener, removeListener,
      platform_view_context);
}

}  // namespace plugin_chromium_dart_view
