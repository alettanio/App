# Serwer AutoSerwis Pro (Windows / Linux) — dane zleceń + PDF + aplikacja przeglądarkowa.
cmake_minimum_required(VERSION 3.16)

add_executable(autoservis-server
    src/main.cpp
    src/http_server.cpp
    src/api.cpp
    src/store.cpp
    src/pdf.cpp
)

target_include_directories(autoservis-server PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

if(WIN32)
    target_link_libraries(autoservis-server PRIVATE ws2_32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(autoservis-server PRIVATE Threads::Threads)
endif()

# Po zbudowaniu skopiuj pliki web obok exe, żeby serwer działał z dowolnego katalogu roboczego
# (można też przekazać --web przy starcie).
add_custom_command(TARGET autoservis-server POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/web" "$<TARGET_FILE_DIR:autoservis-server>/web"
    COMMENT "Kopiowanie aplikacji przegladarkowej obok autoservis-server")
