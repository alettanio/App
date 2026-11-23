# Desktop Windows (Vulkan + GLFW) — klient tylko-do-odczytu.
# Wymaga: Vulkan SDK (glslangValidator + nagłówki + loader) oraz GLFW.
# GLFW: zainstalowane (find_package) albo pobierane przez FetchContent z GitHuba.
cmake_minimum_required(VERSION 3.16)

find_package(Vulkan REQUIRED)

find_package(glfw3 QUIET)
if(NOT glfw3_FOUND)
    include(FetchContent)
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4)
    FetchContent_MakeAvailable(glfw)
endif()

set(CLIENTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../clients")

add_executable(autoservis-desktop
    main.cpp
    "${CLIENTS_DIR}/shared/client_app.cpp"
    "${CLIENTS_DIR}/shared/client_http.cpp"
    "${CLIENTS_DIR}/shared/vulkan_ui.cpp"
)

target_include_directories(autoservis-desktop PRIVATE
    "${CLIENTS_DIR}/shared"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

if(WIN32)
    target_compile_definitions(autoservis-desktop PRIVATE VK_USE_PLATFORM_WIN32_KHR)
    target_link_libraries(autoservis-desktop PRIVATE ws2_32)
elseif(UNIX AND NOT APPLE)
    # Linux (opcjonalnie, np. do testów): X11. Na Windows/Android ten blok jest pomijany.
    target_compile_definitions(autoservis-desktop PRIVATE VK_USE_PLATFORM_XLIB_KHR)
endif()

target_link_libraries(autoservis-desktop PRIVATE Vulkan::Vulkan glfw)

include("${CLIENTS_DIR}/cmake/CompileShaders.cmake")
autoservis_add_ui_shaders(autoservis-desktop "${CLIENTS_DIR}/shaders")

if(WIN32)
    # Konsola zostaje widoczna (logi); zmień na WIN32_EXECUTABLE, jeśli chcesz bez konsoli.
    # set_target_properties(autoservis-desktop PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
