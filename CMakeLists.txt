cmake_minimum_required(VERSION 3.28)
project(WillEngine)

set(CMAKE_CXX_STANDARD 20)

add_definitions(-DVK_NO_PROTOTYPES)
add_definitions(-DGLM_FORCE_DEPTH_ZERO_TO_ONE)
add_definitions(-DGLM_ENABLE_EXPERIMENTAL)
add_definitions(-DJPH_DEBUG_RENDERER)
#add_definitions(-DJPH_PROFILE_ENABLED=0)
#add_definitions(-DJPH_DEBUG_RENDERER=0)
add_definitions(-DJPH_ENABLE_ASSERTS=1)
add_definitions(-DJPH_ENABLE_ASSERT_MESSAGES=1)

# Vulkan
find_package(Vulkan REQUIRED)

set(VULKAN_PATH $ENV{VULKAN_SDK})

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    find_library(SHADERC_LIB
            NAMES shaderc_combinedd
            PATHS $ENV{VULKAN_SDK}/Lib
            REQUIRED
    )

    find_library(SDL2MAIN_LIB
            NAMES SDL2maind
            PATHS $ENV{VULKAN_SDK}/Lib
            REQUIRED
    )

    find_library(SDL2_LIB
            NAMES SDL2d
            PATHS $ENV{VULKAN_SDK}/Lib
            REQUIRED
    )

    find_library(VOLK_LIB
            NAMES volkd
            PATHS $ENV{VULKAN_SDK}/Lib
            REQUIRED
    )
else ()
    find_library(SHADERC_LIB
            NAMES shaderc_combined
            PATHS $ENV{VULKAN_SDK}/Lib
            REQUIRED
    )

    find_library(SDL2MAIN_LIB
            NAMES SDL2main
            PATHS $ENV{VULKAN_SDK}/Lib
            REQUIRED
    )

    find_library(SDL2_LIB
            NAMES SDL2
            PATHS $ENV{VULKAN_SDK}/Lib
            REQUIRED
    )
    find_library(VOLK_LIB
            NAMES volk
            PATHS $ENV{VULKAN_SDK}/Lib
            REQUIRED
    )
endif ()

# Jolt Physics
set(PHYSICS_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/extern/JoltPhysics")

# Nlohmann JSON
set(JSON_BuildTests OFF CACHE INTERNAL "")



add_subdirectory(extern/vk-bootstrap)

add_subdirectory(extern/fmt)
add_subdirectory(extern/fastgltf)
add_subdirectory(extern/fastgltf/deps) # simdjson
add_subdirectory(extern/json)

set(BUILD_SHARED_LIBS ON)
add_definitions(-DJPH_SHARED_LIBRARY)
include(extern/JoltPhysics/Jolt/Jolt.cmake)
set(CMAKE_CXX_STANDARD 20) # Jolt sets it to 17

include_directories(${CMAKE_SOURCE_DIR})

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui)
set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
        ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)
set(FASTGLTF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extern/fastgltf)

set(FASTGLTF_SOURCES
        ${FASTGLTF_DIR}/src/base64.cpp
        ${FASTGLTF_DIR}/src/fastgltf.cpp
        ${FASTGLTF_DIR}/src/io.cpp
)

set(HALF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extern/half)
set(HALF_SOURCES
        ${HALF_DIR}/half/dummy.cpp
        ${HALF_DIR}/half/half.hpp
)

set(ENGINE_SOURCES
        src/core/engine.cpp
        src/core/engine.h
        src/renderer/imgui_wrapper.h
        src/renderer/imgui_wrapper.cpp
        src/core/scene/scene_serializer.cpp
        src/core/scene/scene_serializer.h
)
set(CORE_SOURCES
        src/core/input.cpp
        src/core/input.h
        src/core/time.cpp
        src/core/time.h
        src/core/engine_types.cpp
        src/core/engine_types.h
        src/core/transform.h
        src/core/transform.cpp
        src/core/scene/scene.cpp
        src/core/scene/scene.h
        src/core/identifier/identifier_manager.cpp
        src/core/identifier/identifier_manager.h
        src/core/game_object/game_object.h
        src/core/game_object/game_object.cpp
        src/core/game_object/transformable.h
        src/core/game_object/hierarchical.h
        src/core/game_object/renderable.h
        src/core/identifier/identifiable.h
        src/core/camera/camera.cpp
        src/core/camera/camera.h
        src/core/camera/camera_types.h
        src/core/camera/free_camera.cpp
        src/core/camera/free_camera.h
        src/core/camera/orbit_camera.cpp
        src/core/camera/orbit_camera.h

)


set(PHYSICS_SOURCES
        src/physics/physics.cpp
        src/physics/physics.h
        src/physics/physics_filters.cpp
        src/physics/physics_filters.h
        src/physics/physics_types.h
        src/physics/physics_utils.h
        src/physics/physics_utils.cpp
        src/physics/physics_constants.h
        src/physics/debug/debug_renderer.cpp
        src/physics/debug/debug_renderer.h
        src/physics/physics_body.h
)

set(RENDERER_SOURCES
        src/renderer/vulkan_context.h
        src/renderer/vulkan_context.cpp
        src/renderer/resource_manager.h
        src/renderer/resource_manager.cpp
        src/renderer/immediate_submitter.h
        src/renderer/immediate_submitter.cpp
        src/renderer/vk_descriptors.cpp
        src/renderer/vk_descriptors.h
        src/renderer/vk_helpers.h
        src/renderer/vk_pipelines.cpp
        src/renderer/vk_helpers.cpp
        src/renderer/vk_pipelines.h
        src/renderer/vk_types.cpp
        src/renderer/vk_types.h
        src/renderer/renderer_constants.h
        src/renderer/render_object/render_object.h
        src/renderer/render_object/render_object.cpp
        src/renderer/render_object/render_object_types.h
        src/renderer/render_object/render_object_types.cpp
        src/renderer/render_object/render_reference.h
        src/renderer/environment/environment.cpp
        src/renderer/environment/environment.h
        src/renderer/environment/environment_constants.h
        src/renderer/environment/environment_types.h
        src/renderer/pipelines/deferred_mrt/deferred_mrt.cpp
        src/renderer/pipelines/deferred_mrt/deferred_mrt.h
        src/renderer/pipelines/deferred_resolve/deferred_resolve.cpp
        src/renderer/pipelines/deferred_resolve/deferred_resolve.h
        src/renderer/pipelines/environment/environment_pipeline.cpp
        src/renderer/pipelines/environment/environment_pipeline.h
        src/renderer/pipelines/post_process/post_process_pipeline.cpp
        src/renderer/pipelines/post_process/post_process_pipeline.h
        src/renderer/pipelines/visibility_pass/visibility_pass.cpp
        src/renderer/pipelines/visibility_pass/visibility_pass.h
        src/renderer/pipelines/temporal_antialiasing_pipeline/temporal_antialiasing_pipeline.cpp
        src/renderer/pipelines/temporal_antialiasing_pipeline/temporal_antialiasing_pipeline.h
        src/renderer/pipelines/basic_compute/basic_compute_pipeline.cpp
        src/renderer/pipelines/basic_compute/basic_compute_pipeline.h
        src/renderer/pipelines/basic_render/basic_render_pipeline.cpp
        src/renderer/pipelines/basic_render/basic_render_pipeline.h
        src/renderer/lighting/directional_light.h
        src/renderer/lighting/directional_light.cpp
        src/renderer/lighting/shadows/cascaded_shadow_map.cpp
        src/renderer/lighting/shadows/cascaded_shadow_map.h
        src/renderer/lighting/shadows/shadows.cpp
        src/renderer/lighting/shadows/shadows.h
        src/renderer/lighting/shadows/shadow_types.h
        src/renderer/descriptor_buffer/descriptor_buffer.cpp
        src/renderer/descriptor_buffer/descriptor_buffer.h
        src/renderer/descriptor_buffer/descriptor_buffer_sampler.cpp
        src/renderer/descriptor_buffer/descriptor_buffer_sampler.h
        src/renderer/descriptor_buffer/descriptor_buffer_types.cpp
        src/renderer/descriptor_buffer/descriptor_buffer_types.h
        src/renderer/descriptor_buffer/descriptor_buffer_uniform.cpp
        src/renderer/descriptor_buffer/descriptor_buffer_uniform.h
        src/renderer/render_object/render_object_constants.h
        src/renderer/post_process/post_process_types.h
)


set(UTIL_SOURCES
        src/util/math_constants.h
        src/util/file.h
        src/util/halton.h
        src/util/render_utils.h
)


if (WIN32)
    enable_language(RC)
    set(WINDOWS_SOURCES
            src/resources.h
            src/resources.rc
    )
endif ()

add_executable(WillEngine main.cpp
        ${IMGUI_SOURCES}
        ${FASTGLTF_SOURCES}
        ${HALF_SOURCES}
        ${ENGINE_SOURCES}
        ${PHYSICS_SOURCES}
        ${RENDERER_SOURCES}
        ${CORE_SOURCES}
        ${UTIL_SOURCES}
        ${WINDOWS_SOURCES}
)

target_include_directories(WillEngine PRIVATE
        ${Vulkan_INCLUDE_DIRS}                                              # Vulkan
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/stb                              # STBI
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/fastgltf/include                 # fastgltf
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/fastgltf/deps/simdjson           # simdjson
        ${IMGUI_DIR}                                                        # ImGui
        ${IMGUI_DIR}/backends                                               # ImGui backends
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/JoltPhysics
        ${VULKAN_PATH}/Include/shaderc                                    # ShaderC
        ${VULKAN_PATH}/Include/SDL2                                       # SDL2
        ${VULKAN_PATH}/Include/vma                                        # VMA
        ${VULKAN_PATH}/Include/Volk                                       # volk
        ${VULKAN_PATH}/Include/glm                                        # glm
)

target_link_libraries(WillEngine PRIVATE
        ${SDL2MAIN_LIB}
        ${SDL2_LIB}
        ${SHADERC_LIB}
        ${VOLK_LIB}
        fmt::fmt
        vk-bootstrap::vk-bootstrap
        fastgltf
        nlohmann_json::nlohmann_json
        Jolt
)