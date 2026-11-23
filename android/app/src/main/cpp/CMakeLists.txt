# Android NDK (Vulkan) — klient tylko-do-odczytu.
# Budowanie: Android Studio (otwórz katalog android/) albo Gradle z cmdline-tools.
# Wymaga na komputerze deweloperskim: Android NDK + Vulkan SDK (glslangValidator do shaderów).
cmake_minimum_required(VERSION 3.22)
project(autoservis_android)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Katalog główny repo (android/app/src/main/cpp -> 5 poziomów w górę)
get_filename_component(APP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
set(CLIENTS_DIR "${APP_ROOT}/clients")

if(NOT DEFINED AUTOSERVIS_SERVER_URL OR AUTOSERVIS_SERVER_URL STREQUAL "")
    set(AUTOSERVIS_SERVER_URL "http://10.0.2.2:8080")
endif()
configure_file(client_config.h.in "${CMAKE_CURRENT_BINARY_DIR}/client_config.h" @ONLY)

# native_app_glue z NDK
add_library(native_app_glue STATIC
    "${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c")
target_include_directories(native_app_glue PUBLIC
    "${ANDROID_NDK}/sources/android/native_app_glue")

add_library(autoservis-android SHARED
    main_android.cpp
    "${CLIENTS_DIR}/shared/client_app.cpp"
    "${CLIENTS_DIR}/shared/client_http.cpp"
    "${CLIENTS_DIR}/shared/vulkan_ui.cpp"
)

target_include_directories(autoservis-android PRIVATE
    "${CLIENTS_DIR}/shared"
    "${APP_ROOT}"
    "${CMAKE_CURRENT_BINARY_DIR}"
)

target_compile_definitions(autoservis-android PRIVATE VK_USE_PLATFORM_ANDROID_KHR)

# Vulkan w NDK: nagłówki z sysroot, loader libvulkan.so (API 24+)
target_link_libraries(autoservis-android PRIVATE
    native_app_glue
    vulkan
    android
    log
)

include("${CLIENTS_DIR}/cmake/CompileShaders.cmake")
autoservis_add_ui_shaders(autoservis-android "${CLIENTS_DIR}/shaders")
