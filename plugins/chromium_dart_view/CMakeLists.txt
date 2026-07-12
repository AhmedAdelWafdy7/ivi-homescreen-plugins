#
# chromium_dart_view — thin platform-view bridge to the chromium_dart FFI .so.
# No CEF/Wayland deps here: it only reads the FlutterView surface and calls the
# chromium_dart C ABI (resolved at runtime via dlsym). All rendering lives in
# libchromium_nc.so.
#

include_guard()

add_library(plugin_chromium_dart_view STATIC
        chromium_dart_view_plugin_c_api.cc
        chromium_dart_view_plugin.cc
)

target_include_directories(plugin_chromium_dart_view PRIVATE
        include
        ${CMAKE_BINARY_DIR}/shell/wayland-protocols
)

target_link_libraries(plugin_chromium_dart_view PRIVATE
        flutter
        platform_homescreen
        ${CMAKE_DL_LIBS}
)

# The shell's wayland/display.h transitively includes the wayland-cxx-scanner
# framework headers.
if (TARGET wayland-cxx::wayland-cxx)
    target_link_libraries(plugin_chromium_dart_view PRIVATE wayland-cxx::wayland-cxx)
endif ()
