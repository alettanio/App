# CompileShaders.cmake — kompiluje clients/shaders/*.glsl do nagłówków C++ (SPIR-V)
# Wymaga glslangValidator z Vulkan SDK (Windows: %VULKAN_SDK%\Bin, Linux/Android host: $VULKAN_SDK/bin).
# Użycie: autoservis_add_ui_shaders(<target>)
find_program(AUTOSERVIS_GLSLANG glslangValidator
    HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin" "$ENV{VULKAN_SDK}/Bin32")
if(NOT AUTOSERVIS_GLSLANG)
    message(FATAL_ERROR "Nie znaleziono glslangValidator. Zainstaluj Vulkan SDK (https://vulkan.lunarg.com) "
                        "i upewnij sie, ze VULKAN_SDK wskazuje katalog SDK.")
endif()

function(autoservis_add_ui_shaders target_name shaders_dir)
    set(gen_dir "${CMAKE_CURRENT_BINARY_DIR}/generated_shaders")
    file(MAKE_DIRECTORY "${gen_dir}")
    set(generated_headers "")
    foreach(stage vert frag)
        set(src "${shaders_dir}/ui.${stage}")
        set(spv "${gen_dir}/ui.${stage}.spv")
        set(hdr "${gen_dir}/ui_${stage}_spv.h")
        add_custom_command(
            OUTPUT "${spv}"
            COMMAND "${AUTOSERVIS_GLSLANG}" -V "${src}" -o "${spv}"
            DEPENDS "${src}"
            COMMENT "Kompilacja shadera ui.${stage} -> SPIR-V"
            VERBATIM)
        add_custom_command(
            OUTPUT "${hdr}"
            COMMAND "${CMAKE_COMMAND}"
                    "-DINPUT=${spv}"
                    "-DOUTPUT=${hdr}"
                    "-DVARIABLE=ui_${stage}_spv"
                    -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/GenerateShaderHeader.cmake"
            DEPENDS "${spv}" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/GenerateShaderHeader.cmake"
            COMMENT "Generowanie naglowka ui_${stage}_spv.h"
            VERBATIM)
        list(APPEND generated_headers "${hdr}")
    endforeach()
    target_sources(${target_name} PRIVATE ${generated_headers})
    target_include_directories(${target_name} PRIVATE "${gen_dir}")
endfunction()
