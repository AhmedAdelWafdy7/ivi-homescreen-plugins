#
# Copyright 2024 Toyota Connected North America
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

include_guard()

if (NOT EXISTS ${CEF_ROOT})
    message(FATAL_ERROR "CEF_ROOT: \"${CEF_ROOT}\" does not exist")
endif ()

message(STATUS "  CEF_ROOT: ${CEF_ROOT}")

if (NOT CEF_BUILD_TYPE)
    set(CEF_BUILD_TYPE "Release")
endif ()

message("CEF_BUILD_TYPE: ${CEF_BUILD_TYPE}")

add_library(plugin_webview_flutter_view STATIC
        webview_flutter_view_plugin_c_api.cc
        webview_flutter_view_plugin.cc
        messages.g.cc
)

add_compile_definitions(CEF_ROOT=\"${CEF_ROOT}\")

set(WEBVIEW_SUBPROCESS_PROJECT_NAME "webview_flutter_subprocess")

# CEF_ROOT is a Yocto-staged sysroot path (recipe-sysroot/usr/share/cef), out
# of tree relative to this plugin's own source dir, so CMake requires an
# explicit binary dir. The reference native build this integration follows
# keeps CEF_ROOT in-tree under third_party/ (depot_tools-fetched), where the
# default binary dir is implicit -- harmless for that layout too.
add_subdirectory(${CEF_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/cef)

# libcef_dll_wrapper is CEF's own generated/vendored code, built here in-tree
# via the add_subdirectory() above -- it inherits this project's strict
# -Wconversion -Wsign-conversion -Werror (set globally by the shell's own
# cmake/compiler.cmake), which CEF's own build was never written against.
# Disable just for this target rather than loosening the shell's warning
# flags project-wide or patching CEF's own CMakeLists.txt.
#
# -stdlib=libc++: CEF's vendored cmake/cef_variables.cmake resets CXX flags
# for its own subtree and never sets -stdlib on Linux at all (its only
# stdlib knob, CEF_RUNTIME_LIBRARY_FLAG, defaults to "/MT" and is
# Windows/MSVC-only) -- so without this, libcef_dll_wrapper silently falls
# back to Clang's default (libstdc++) while the rest of this project is
# libc++ (LIBCPLUSPLUS=-stdlib=libc++), and the final link fails with
# undefined std::_Rb_tree_*/std::__throw_length_error symbols (an ABI mix,
# not a missing dependency). libcef_dll_wrapper is a static archive, so only
# its compile-time stdlib choice matters here -- no link flag needed on it.
if (TARGET libcef_dll_wrapper)
    target_compile_options(libcef_dll_wrapper PRIVATE -Wno-error -Wno-sign-conversion -Wno-conversion -stdlib=libc++)
endif ()

target_compile_definitions(plugin_webview_flutter_view PUBLIC
        MESA_EGL_NO_X11_HEADERS
        WL_EGL_PLATFORM
        EGL_NO_X11
)

target_include_directories(plugin_webview_flutter_view PRIVATE
        include
)

target_compile_options(plugin_webview_flutter_view PRIVATE
        -isystem${CEF_ROOT}
        -isystem${CEF_ROOT}/include
)

target_link_directories(plugin_webview_flutter_view PUBLIC
        ${CEF_ROOT}/${CEF_BUILD_TYPE}
)

add_dependencies(plugin_webview_flutter_view libcef_dll_wrapper)

target_link_libraries(plugin_webview_flutter_view PRIVATE
        flutter
        platform_homescreen
        libcef_dll_wrapper
        GLESv2
        EGL
)
