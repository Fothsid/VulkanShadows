#
# Vulkan Shadows
# Author: Fedor Vorobev
# Build dependencies:
#    - CMake
#    - git submodules in external/
#    - SDL2
#    - Vulkan headers + loader (part of Vulkan SDK)
#    - glslc from shaderc (part of Vulkan SDK)
#

cmake_minimum_required(VERSION 3.31)
project(VulkanShadows)

set(CMAKE_CXX_STANDARD 20)
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(CMAKE_DEBUG_POSTFIX d)
    add_definitions(-DDEBUG=1)
endif()

set(SOURCE_CXX
    src/main.cpp
    src/external_impl.cpp
    src/Renderer.cpp
    src/Texture.cpp
    src/Swapchain.cpp
    src/Shader.cpp
    src/RenderPassBuilder.cpp
    src/PipelineBuilder.cpp
    src/PipelineLayoutBuilder.cpp
    src/GpuBuffer.cpp
    src/BindlessSet.cpp
    src/CommonSamplers.cpp
    src/Scene.cpp
    src/ScenePipelines.cpp
    src/Camera.cpp
    src/VIBufferBuilder.cpp
    src/Animation.cpp
    src/Configuration.cpp
)

set(HEADER_CXX
    src/Renderer.hpp
    src/GfxSettings.hpp
    src/Renderer.hpp
    src/Texture.hpp
    src/Swapchain.hpp
    src/Shader.hpp
    src/PipelineBuilder.hpp
    src/PipelineLayoutBuilder.hpp
    src/GpuBuffer.hpp
    src/Vertex.hpp
    src/BindlessSet.hpp
    src/CommonSamplers.hpp
    src/Scene.hpp
    src/ScenePipelines.hpp
    src/Camera.hpp
    src/VIBufferBuilder.hpp
    src/Animation.hpp
    src/Configuration.hpp
)

set(IMGUI_SRC
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/backends/imgui_impl_sdl2.cpp
    external/imgui/backends/imgui_impl_vulkan.cpp
    external/ImGuizmo/ImGuizmo.cpp
)

find_package( SDL2 REQUIRED )
find_package( Vulkan REQUIRED )
add_subdirectory(external/glm)

find_package(Vulkan COMPONENTS glslc)
find_program(GLSLC NAMES glslc HINTS Vulkan::glslc)

include_directories(
    "${SDL2_INCLUDE_DIRS}"
    "${Vulkan_INCLUDE_DIRS}"
    external/glm
    external/tinygltf
    external/VulkanMemoryAllocator/include
    external/imgui
    external/imgui/backends
    external/ImGuizmo
    external/dds_image/include
)

set(PROJECT_LIBRARIES
    "${SDL2_LIBRARIES}"
    "${Vulkan_LIBRARIES}"
    glm::glm
)

add_executable(${CMAKE_PROJECT_NAME}
    ${SOURCE_CXX}
    ${SOURCE_C}
    ${HEADER_CXX}
    ${HEADER_C}
    ${IMGUI_SRC}
)

set(SHADER_OUT_DIR "${PROJECT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY ${SHADER_OUT_DIR})

function(compile_shaders target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES")
    foreach(source ${arg_SOURCES})
        set(_srcfile ${CMAKE_CURRENT_SOURCE_DIR}/${source})
        set(_depfile ${CMAKE_BINARY_DIR}/depfiles/${source}.d)
        get_filename_component(_deppath ${_depfile} DIRECTORY)
        get_filename_component(_dstfile ${_srcfile} NAME)
        set(_dstfile ${SHADER_OUT_DIR}/${_dstfile}.spirv)
        file(MAKE_DIRECTORY ${_deppath})
        add_custom_command(
            OUTPUT  ${_dstfile}
            DEPENDS ${_srcfile}
            DEPFILE ${_depfile}
            COMMAND
                ${GLSLC} -O --target-env=vulkan1.2
                -MD -MF ${_depfile}
                -o ${_dstfile}
                ${_srcfile}
        )
        target_sources(${target} PRIVATE ${_dstfile})
    endforeach()
endfunction()

function(compile_shader_with_defs target source dstname)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "DEFINES")
    set(_srcfile ${CMAKE_CURRENT_SOURCE_DIR}/${source})
    set(_depfile ${CMAKE_BINARY_DIR}/depfiles/${source}.d)
    get_filename_component(_deppath ${_depfile} DIRECTORY)
    set(_dstfile ${SHADER_OUT_DIR}/${dstname}.spirv)
    file(MAKE_DIRECTORY ${_deppath})
    add_custom_command(
        OUTPUT  ${_dstfile}
        DEPENDS ${_srcfile}
        DEPFILE ${_depfile}
        COMMAND
            ${GLSLC} --target-env=vulkan1.2
            -MD -MF ${_depfile}
            ${arg_DEFINES}
            -o ${_dstfile}
            ${_srcfile}
    )
    target_sources(${target} PRIVATE ${_dstfile})
endfunction()

compile_shaders(${CMAKE_PROJECT_NAME} SOURCES
    src/shaders/scene.vert
    src/shaders/scene.frag
    src/shaders/shadowmap.vert
    src/shaders/shadowmap.frag
    src/shaders/shadowvolumes.vert
    src/shaders/shadowvolumes.geom
    src/shaders/svsilhouette.geom
    src/shaders/debug.frag
)

compile_shader_with_defs(${CMAKE_PROJECT_NAME}
    src/shaders/svsilhouette.geom silhouettedebug.geom
    DEFINES -DDEBUG=1
)

target_link_libraries(${CMAKE_PROJECT_NAME} ${PROJECT_LIBRARIES})

if (MSVC)
    # Copy all the .DLL files the executable depends on to the final folder.
    add_custom_command(
        TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${CMAKE_PROJECT_NAME}> $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>
        COMMAND_EXPAND_LISTS
    )
endif()