# AutoSerwis Pro — platforma zleceń samochodowych
#  - serwer C++ (komputer-serwer: dane + PDF + www)   -> server/
#  - desktop Windows Vulkan (klient tylko-do-odczytu) -> desktop/   (opcja)
#  - Android Vulkan budowany osobno przez Gradle/NDK  -> android/   (patrz README)
cmake_minimum_required(VERSION 3.20)
project(AutoSerwisPro LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra)
endif()

option(AUTOSERVIS_BUILD_SERVER "Buduj serwer (Windows/Linux)" ON)
option(AUTOSERVIS_BUILD_DESKTOP "Buduj aplikacje desktop Vulkan (Windows)" OFF)

if(AUTOSERVIS_BUILD_SERVER)
    add_subdirectory(server)
endif()

if(AUTOSERVIS_BUILD_DESKTOP)
    add_subdirectory(desktop)
endif()
