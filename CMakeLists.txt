cmake_minimum_required(VERSION 3.28)
project(WillEngine)

set(CMAKE_CXX_STANDARD 23)

add_definitions(-DVK_NO_PROTOTYPES)

add_definitions(-DGLM_FORCE_DEPTH_ZERO_TO_ONE)
add_definitions(-DGLM_ENABLE_EXPERIMENTAL)

#add_definitions(-DJPH_PROFILE_ENABLED=0)
add_definitions(-DJPH_ENABLE_ASSERTS=1)
add_definitions(-DJPH_ENABLE_ASSERT_MESSAGES=1)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DWILL_ENGINE_DEBUG=1)
    add_definitions(-DJPH_DEBUG_RENDERER)
    add_definitions(-DWILL_ENGINE_DEBUG_DRAW=1)
else()
    add_definitions(-DWILL_ENGINE_RELEASE=1)

    if (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_definitions(-DWILL_ENGINE_DEBUG_DRAW=1)
        add_definitions(-DJPH_DEBUG_RENDERER)
    endif()
endif()

# Vulkan
find_package(Vulkan REQUIRED)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(LIB_SUFFIX "d")
else ()
    set(LIB_SUFFIX "")
endif ()

find_library(SHADERC_LIBRARIES
        NAMES shaderc_combined${LIB_SUFFIX}
        PATHS ${CMAKE_CURRENT_SOURCE_DIR}/lib
        REQUIRED
)

find_library(SDL3_LIBRARIES
        NAMES SDL3${LIB_SUFFIX}
        PATHS ${CMAKE_CURRENT_SOURCE_DIR}/lib
        REQUIRED
)

add_subdirectory(extern/fmt)
add_subdirectory(extern/fastgltf)
add_subdirectory(extern/fastgltf/deps) # simdjson

# Jolt Physics
set(PHYSICS_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/extern/JoltPhysics")
include(extern/JoltPhysics/Jolt/Jolt.cmake)
set(CMAKE_CXX_STANDARD 23) # Jolt sets it to 17
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/src)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui)
set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
        ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/ImGuiFileDialog/ImGuiFileDialog.cpp
)
set(FASTGLTF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extern/fastgltf)

set(FASTGLTF_SOURCES
        ${FASTGLTF_DIR}/src/base64.cpp
        ${FASTGLTF_DIR}/src/fastgltf.cpp
        ${FASTGLTF_DIR}/src/io.cpp
)

set(VK_BOOTSTRAP_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/vk-bootstrap/VkBootstrap.cpp
)

set(VOLK_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/volk/volk.c
)


set(ENGINE_SOURCES
        src/engine/core/engine.cpp
        src/engine/core/engine.h
        src/engine/renderer/imgui_wrapper.h
        src/engine/renderer/imgui_wrapper.cpp
        src/engine/core/scene/serializer.cpp
        src/engine/core/scene/serializer.h
)
set(CORE_SOURCES
        src/engine/core/input.cpp
        src/engine/core/input.h
        src/engine/core/time.cpp
        src/engine/core/time.h
        src/engine/core/engine_types.cpp
        src/engine/core/engine_types.h
        src/engine/core/transform.h
        src/engine/core/transform.cpp
        src/engine/core/game_object/game_object.h
        src/engine/core/game_object/game_object.cpp
        src/engine/core/game_object/transformable.h
        src/engine/core/game_object/hierarchical.h
        src/engine/core/game_object/renderable.h
        src/engine/core/game_object/imgui_renderable.h
        src/engine/core/game_object/component_container.h
        src/engine/core/game_object/components/component.cpp
        src/engine/core/game_object/components/component.h
        src/engine/core/game_object/components/name_printing_component.cpp
        src/engine/core/game_object/components/name_printing_component.h
        src/engine/core/game_object/components/component_factory.h
        src/engine/core/game_object/components/rigid_body_component.h
        src/engine/core/game_object/components/rigid_body_component.cpp
        src/engine/core/camera/camera.cpp
        src/engine/core/camera/camera.h
        src/engine/core/camera/camera_types.h
        src/engine/core/camera/free_camera.cpp
        src/engine/core/camera/free_camera.h
        src/engine/core/camera/orbit_camera.cpp
        src/engine/core/camera/orbit_camera.h
        src/engine/core/profiler/profiler.cpp
        src/engine/core/profiler/profiler.h
)


set(PHYSICS_SOURCES
        src/engine/physics/physics.cpp
        src/engine/physics/physics.h
        src/engine/physics/physics_filters.cpp
        src/engine/physics/physics_filters.h
        src/engine/physics/physics_types.h
        src/engine/physics/physics_utils.h
        src/engine/physics/physics_utils.cpp
        src/engine/physics/physics_constants.h
        src/engine/physics/physics_body.h
)

set(RENDERER_SOURCES
        src/engine/renderer/vulkan_context.h
        src/engine/renderer/vulkan_context.cpp
        src/engine/renderer/resource_manager.h
        src/engine/renderer/resource_manager.cpp
        src/engine/renderer/immediate_submitter.h
        src/engine/renderer/immediate_submitter.cpp
        src/engine/renderer/vk_descriptors.cpp
        src/engine/renderer/vk_descriptors.h
        src/engine/renderer/vk_helpers.h
        src/engine/renderer/vk_pipelines.cpp
        src/engine/renderer/vk_helpers.cpp
        src/engine/renderer/vk_pipelines.h
        src/engine/renderer/vk_types.cpp
        src/engine/renderer/vk_types.h
        src/engine/renderer/renderer_constants.h
        src/engine/renderer/assets/render_object/render_object.h
        src/engine/renderer/assets/render_object/render_object.cpp
        src/engine/renderer/assets/render_object/render_object_types.h
        src/engine/renderer/assets/render_object/render_object_types.cpp
        src/engine/renderer/assets/render_object/render_reference.h
        src/engine/renderer/environment/environment.cpp
        src/engine/renderer/environment/environment.h
        src/engine/renderer/environment/environment_constants.h
        src/engine/renderer/environment/environment_types.h
        src/engine/renderer/pipelines/geometry/deferred_mrt/deferred_mrt_pipeline.cpp
        src/engine/renderer/pipelines/geometry/deferred_mrt/deferred_mrt_pipeline.h
        src/engine/renderer/pipelines/geometry/deferred_mrt/deferred_mrt_pipeline_types.h
        src/engine/renderer/pipelines/geometry/deferred_resolve/deferred_resolve_pipeline.cpp
        src/engine/renderer/pipelines/geometry/deferred_resolve/deferred_resolve_pipeline.h
        src/engine/renderer/pipelines/geometry/deferred_resolve/deferred_resolve_pipeline_types.h
        src/engine/renderer/pipelines/geometry/environment/environment_pipeline.cpp
        src/engine/renderer/pipelines/geometry/environment/environment_pipeline.h
        src/engine/renderer/pipelines/post/post_process/post_process_pipeline.cpp
        src/engine/renderer/pipelines/post/post_process/post_process_pipeline.h
        src/engine/renderer/pipelines/visibility_pass/visibility_pass_pipeline.cpp
        src/engine/renderer/pipelines/visibility_pass/visibility_pass_pipeline.h
        src/engine/renderer/pipelines/post/temporal_antialiasing/temporal_antialiasing_pipeline.cpp
        src/engine/renderer/pipelines/post/temporal_antialiasing/temporal_antialiasing_pipeline.h
        src/engine/renderer/pipelines/basic/basic_compute/basic_compute_pipeline.cpp
        src/engine/renderer/pipelines/basic/basic_compute/basic_compute_pipeline.h
        src/engine/renderer/pipelines/basic/basic_render/basic_render_pipeline.cpp
        src/engine/renderer/pipelines/basic/basic_render/basic_render_pipeline.h
        src/engine/renderer/lighting/directional_light.h
        src/engine/renderer/lighting/directional_light.cpp
        src/engine/renderer/pipelines/shadows/cascaded_shadow_map/cascaded_shadow_map.cpp
        src/engine/renderer/pipelines/shadows/cascaded_shadow_map/cascaded_shadow_map.h
        src/engine/renderer/pipelines/shadows/cascaded_shadow_map/shadow_types.h
        src/engine/renderer/descriptor_buffer/descriptor_buffer.cpp
        src/engine/renderer/descriptor_buffer/descriptor_buffer.h
        src/engine/renderer/descriptor_buffer/descriptor_buffer_sampler.cpp
        src/engine/renderer/descriptor_buffer/descriptor_buffer_sampler.h
        src/engine/renderer/descriptor_buffer/descriptor_buffer_types.cpp
        src/engine/renderer/descriptor_buffer/descriptor_buffer_types.h
        src/engine/renderer/descriptor_buffer/descriptor_buffer_uniform.cpp
        src/engine/renderer/descriptor_buffer/descriptor_buffer_uniform.h
        src/engine/renderer/assets/render_object/render_object_constants.h
)


set(UTIL_SOURCES
        src/engine/util/math_constants.h
        src/engine/util/file.h
        src/engine/util/halton.h
        src/engine/util/render_utils.h
        src/engine/util/profiling_utils.h
        src/engine/util/model_utils.h
        src/engine/util/model_utils.cpp
)


if (WIN32)
    enable_language(RC)
    set(WINDOWS_SOURCES
            src/resources.h
            src/resources.rc
    )
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(DEBUG_SOURCES
            src/engine/renderer/pipelines/debug/debug_renderer.cpp
            src/engine/renderer/pipelines/debug/debug_renderer.h
            src/engine/renderer/pipelines/debug/debug_composite_pipeline.cpp
            src/engine/renderer/pipelines/debug/debug_composite_pipeline.h
            src/engine/renderer/pipelines/debug/debug_highlighter.cpp
            src/engine/renderer/pipelines/debug/debug_highlighter.h
            src/engine/physics/debug/jolt_debug_renderer.cpp
            src/engine/physics/debug/jolt_debug_renderer.h
    )
endif()

set(TEMP_SOURCES
        src/engine/core/game_object/components/mesh_renderer_component.h
        src/engine/core/game_object/components/mesh_renderer_component.cpp
        src/engine/util/math_utils.h
        src/engine/renderer/terrain/terrain_manager.cpp
        src/engine/renderer/terrain/terrain_manager.h
        src/engine/renderer/terrain/terrain_types.h
        src/engine/util/heightmap_utils.h
        src/engine/util/noise_utils.h
        src/engine/renderer/terrain/terrain_chunk.cpp
        src/engine/renderer/terrain/terrain_chunk.h
        src/engine/renderer/pipelines/geometry/terrain/terrain_pipeline.cpp
        src/engine/renderer/pipelines/geometry/terrain/terrain_pipeline.h
        src/engine/core/scene/map.cpp
        src/engine/core/scene/map.h
        src/engine/core/game_object/terrain.h
        src/engine/renderer/assets/material/material.cpp
        src/engine/renderer/assets/material/material.h
        src/engine/renderer/assets/texture/texture.cpp
        src/engine/renderer/assets/texture/texture.h
        src/engine/renderer/assets/asset_manager.cpp
        src/engine/renderer/assets/asset_manager.h
        src/engine/core/game_object/components/terrain_component.cpp
        src/engine/core/game_object/components/terrain_component.h
        src/engine/renderer/terrain/terrain_constants.h
        src/engine/core/scene/serializer_types.h
        src/engine/core/scene/serializer_constants.h
        src/engine/renderer/assets/texture/texture_resource.cpp
        src/engine/renderer/assets/texture/texture_resource.h
        src/engine/renderer/assets/texture/texture_types.h
        src/engine/renderer/pipelines/shadows/ground_truth_ambient_occlusion/ground_truth_ambient_occlusion_pipeline.cpp
        src/engine/renderer/pipelines/shadows/ground_truth_ambient_occlusion/ground_truth_ambient_occlusion_pipeline.h
        src/engine/renderer/pipelines/shadows/ground_truth_ambient_occlusion/ambient_occlusion_types.h
        src/engine/renderer/pipelines/geometry/transparent_pipeline/transparent_pipeline.cpp
        src/engine/renderer/pipelines/geometry/transparent_pipeline/transparent_pipeline.h
        src/engine/engine_constants.h
        src/engine/renderer/pipelines/shadows/contact_shadow/contact_shadows_pipeline.cpp
        src/engine/renderer/pipelines/shadows/contact_shadow/contact_shadows_pipeline.h
        src/engine/renderer/pipelines/shadows/contact_shadow/contact_shadows_pipeline_types.h
        src/engine/renderer/pipelines/geometry/environment/environment_pipeline_types.h
        src/engine/renderer/pipelines/basic/basic_render/basic_render_pipeline_types.h
        src/engine/renderer/pipelines/post/post_process/post_process_pipeline_types.h
        src/engine/renderer/pipelines/post/temporal_antialiasing/temporal_antialiasing_pipeline_types.h
        src/engine/renderer/pipelines/geometry/terrain/terrain_pipeline_types.h
        src/engine/renderer/pipelines/geometry/transparent_pipeline/transparent_pipeline_types.h
        src/engine/renderer/pipelines/basic/basic_compute/basic_compute_pipeline_types.h
        src/engine/renderer/pipelines/visibility_pass/visibility_pass_pipeline_types.h
        src/engine/renderer/pipelines/debug/debug_renderer_types.h
        src/engine/core/game_object/game_object_factory.cpp
        src/engine/core/game_object/game_object_factory.h
        src/engine/renderer/pipelines/debug/debug_highlight_types.h
)

add_executable(WillEngine main.cpp
        ${IMGUI_SOURCES}
        ${FASTGLTF_SOURCES}
        ${VK_BOOTSTRAP_SOURCES}
        ${VOLK_SOURCES}
        ${ENGINE_SOURCES}
        ${PHYSICS_SOURCES}
        ${RENDERER_SOURCES}
        ${CORE_SOURCES}
        ${UTIL_SOURCES}
        ${WINDOWS_SOURCES}
        ${TEMP_SOURCES}
        ${DEBUG_SOURCES}
)

target_include_directories(WillEngine PRIVATE
        ${Vulkan_INCLUDE_DIRS}                                              # Vulkan
        ${IMGUI_DIR}                                                        # ImGui
        ${IMGUI_DIR}/backends                                               # ImGui backends
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/JoltPhysics
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/shaderc                          # ShaderC
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/SDL3                             # SDL2
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/fastgltf/include                 # fastgltf
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/fastgltf/deps/simdjson           # simdjson
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/FastNoiseLite

)

target_link_libraries(WillEngine PRIVATE
        ${SDL3_LIBRARIES}
        ${VOLK_LIBRARIES}
        ${SHADERC_LIBRARIES}
        fmt::fmt
        fastgltf
        Jolt
)

set(SDL_DLL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/SDL3${LIB_SUFFIX}.dll")

add_custom_command(TARGET WillEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SDL_DLL_PATH} $<TARGET_FILE_DIR:WillEngine>
)