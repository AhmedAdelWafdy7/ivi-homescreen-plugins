set(TESTCASE_NAME "FlatpakPluginTest")
set(TESTCASE_CC test_cache_manager.cc)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FLATPAK_DEPS REQUIRED IMPORTED_TARGET flatpak libxml-2.0 zlib)
find_package(GTest REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(CURL REQUIRED)
find_package(fmt REQUIRED)

set(MISSING_SHELL_SOURCES
        ${CMAKE_SOURCE_DIR}/shell/libflutter_engine.cc
        ${CMAKE_SOURCE_DIR}/shell/engine.cc
        ${CMAKE_SOURCE_DIR}/shell/task_runner.cc
        ${CMAKE_SOURCE_DIR}/shell/timer.cc
        ${CMAKE_SOURCE_DIR}/shell/wayland/window.cc
        ${CMAKE_SOURCE_DIR}/shell/wayland/display.cc
)

set(PLATFORM_SOURCES
        ${CMAKE_SOURCE_DIR}/shell/platform/homescreen/platform_views/platform_views_handler.cc
        ${CMAKE_SOURCE_DIR}/shell/platform/homescreen/platform_views/platform_view_touch.cc
)

file(GLOB CACHE_SOURCES "../cache/*.cc")
file(GLOB CACHE_STORAGE_SOURCES "../cache/storage/*.cc")
file(GLOB CACHE_NETWORK_SOURCES "../cache/network/*.cc")
file(GLOB CACHE_OBSERVER_SOURCES "../cache/observer/*.cc")

set(ALL_SOURCES
        ${CACHE_SOURCES}
        ${CACHE_STORAGE_SOURCES}
        ${CACHE_NETWORK_SOURCES}
        ${CACHE_OBSERVER_SOURCES}
        ${MISSING_SHELL_SOURCES}
        ${PLATFORM_SOURCES}
)

set(EXISTING_SOURCES)
foreach(SOURCE ${ALL_SOURCES})
    if(EXISTS ${SOURCE})
        list(APPEND EXISTING_SOURCES ${SOURCE})
        message(STATUS "Including source: ${SOURCE}")
    else()
        message(WARNING "Source not found: ${SOURCE}")
    endif()
endforeach()

add_executable(
        ${TESTCASE_NAME}
        ${TESTCASE_CC}
        ${EXISTING_SOURCES}
)

target_include_directories(${TESTCASE_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../cache
        ${CMAKE_CURRENT_SOURCE_DIR}/../cache/storage
        ${CMAKE_CURRENT_SOURCE_DIR}/../cache/network
        ${CMAKE_CURRENT_SOURCE_DIR}/../cache/observer
        ${CMAKE_CURRENT_SOURCE_DIR}/../cache/interfaces
        ${CMAKE_CURRENT_SOURCE_DIR}/../cache/operations
        ${CMAKE_SOURCE_DIR}/shell
        ${CMAKE_SOURCE_DIR}/shell/platform/homescreen
        ${CMAKE_SOURCE_DIR}/shell/wayland
        ${CMAKE_SOURCE_DIR}/third_party/flutter/include
        ${CMAKE_SOURCE_DIR}/third_party/waypp/include
)

target_link_libraries(${TESTCASE_NAME} PRIVATE
        flutter
        plugin_flatpak
        wayland-gen
        PkgConfig::FLATPAK_DEPS
        SQLite::SQLite3
        CURL::libcurl
        wayland-cursor
        xkbcommon
        wayland-egl
        wayland-client
        GTest::GTest
        GTest::Main
        Threads::Threads
)

add_test(
        NAME ${TESTCASE_NAME}
        COMMAND ${TESTCASE_NAME}
)