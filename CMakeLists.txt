cmake_minimum_required(VERSION 3.28)
project(WillEngine)

set(CMAKE_CXX_STANDARD 23)

add_definitions(-DVK_NO_PROTOTYPES)

add_definitions(-DGLM_FORCE_DEPTH_ZERO_TO_ONE)
add_definitions(-DGLM_ENABLE_EXPERIMENTAL)

#add_definitions(-DJPH_PROFILE_ENABLED=0)
add_definitions(-DJPH_ENABLE_ASSERTS=1)
add_definitions(-DJPH_ENABLE_ASSERT_MESSAGES=1)

# #if WILL_ENGINE_DEBUG
# ##endif
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DWILL_ENGINE_DEBUG=1)
    add_definitions(-DJPH_DEBUG_RENDERER)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-DWILL_ENGINE_RELEASE=1)
endif ()

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
        src/core/engine.cpp
        src/core/engine.h
        src/renderer/imgui_wrapper.h
        src/renderer/imgui_wrapper.cpp
        src/core/scene/serializer.cpp
        src/core/scene/serializer.h
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
        src/core/game_object/game_object.h
        src/core/game_object/game_object.cpp
        src/core/game_object/transformable.h
        src/core/game_object/hierarchical.h
        src/core/game_object/renderable.h
        src/core/game_object/imgui_renderable.h
        src/core/game_object/component_container.h
        src/core/game_object/components/component.cpp
        src/core/game_object/components/component.h
        src/core/game_object/components/name_printing_component.cpp
        src/core/game_object/components/name_printing_component.h
        src/core/game_object/components/component_factory.h
        src/core/game_object/components/rigid_body_component.h
        src/core/game_object/components/rigid_body_component.cpp
        src/core/camera/camera.cpp
        src/core/camera/camera.h
        src/core/camera/camera_types.h
        src/core/camera/free_camera.cpp
        src/core/camera/free_camera.h
        src/core/camera/orbit_camera.cpp
        src/core/camera/orbit_camera.h
        src/core/profiler/profiler.cpp
        src/core/profiler/profiler.h
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
        src/physics/debug/jolt_debug_renderer.cpp
        src/physics/debug/jolt_debug_renderer.h
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
        src/renderer/assets/render_object/render_object.h
        src/renderer/assets/render_object/render_object.cpp
        src/renderer/assets/render_object/render_object_types.h
        src/renderer/assets/render_object/render_object_types.cpp
        src/renderer/assets/render_object/render_reference.h
        src/renderer/environment/environment.cpp
        src/renderer/environment/environment.h
        src/renderer/environment/environment_constants.h
        src/renderer/environment/environment_types.h
        src/renderer/pipelines/geometry/deferred_mrt/deferred_mrt_pipeline.cpp
        src/renderer/pipelines/geometry/deferred_mrt/deferred_mrt_pipeline.h
        src/renderer/pipelines/geometry/deferred_mrt/deferred_mrt_pipeline_types.h
        src/renderer/pipelines/geometry/deferred_resolve/deferred_resolve_pipeline.cpp
        src/renderer/pipelines/geometry/deferred_resolve/deferred_resolve_pipeline.h
        src/renderer/pipelines/geometry/deferred_resolve/deferred_resolve_pipeline_types.h
        src/renderer/pipelines/geometry/environment/environment_pipeline.cpp
        src/renderer/pipelines/geometry/environment/environment_pipeline.h
        src/renderer/pipelines/post/post_process/post_process_pipeline.cpp
        src/renderer/pipelines/post/post_process/post_process_pipeline.h
        src/renderer/pipelines/visibility_pass/visibility_pass_pipeline.cpp
        src/renderer/pipelines/visibility_pass/visibility_pass_pipeline.h
        src/renderer/pipelines/post/temporal_antialiasing/temporal_antialiasing_pipeline.cpp
        src/renderer/pipelines/post/temporal_antialiasing/temporal_antialiasing_pipeline.h
        src/renderer/pipelines/basic/basic_compute/basic_compute_pipeline.cpp
        src/renderer/pipelines/basic/basic_compute/basic_compute_pipeline.h
        src/renderer/pipelines/basic/basic_render/basic_render_pipeline.cpp
        src/renderer/pipelines/basic/basic_render/basic_render_pipeline.h
        src/renderer/lighting/directional_light.h
        src/renderer/lighting/directional_light.cpp
        src/renderer/pipelines/shadows/cascaded_shadow_map/cascaded_shadow_map.cpp
        src/renderer/pipelines/shadows/cascaded_shadow_map/cascaded_shadow_map.h
        src/renderer/pipelines/shadows/cascaded_shadow_map/shadow_types.h
        src/renderer/descriptor_buffer/descriptor_buffer.cpp
        src/renderer/descriptor_buffer/descriptor_buffer.h
        src/renderer/descriptor_buffer/descriptor_buffer_sampler.cpp
        src/renderer/descriptor_buffer/descriptor_buffer_sampler.h
        src/renderer/descriptor_buffer/descriptor_buffer_types.cpp
        src/renderer/descriptor_buffer/descriptor_buffer_types.h
        src/renderer/descriptor_buffer/descriptor_buffer_uniform.cpp
        src/renderer/descriptor_buffer/descriptor_buffer_uniform.h
        src/renderer/assets/render_object/render_object_constants.h
)


set(UTIL_SOURCES
        src/util/math_constants.h
        src/util/file.h
        src/util/halton.h
        src/util/render_utils.h
        src/util/profiling_utils.h
        src/util/model_utils.h
        src/util/model_utils.cpp
)


if (WIN32)
    enable_language(RC)
    set(WINDOWS_SOURCES
            src/resources.h
            src/resources.rc
    )
endif ()

set(TEMP_SOURCES
        src/core/game_object/components/mesh_renderer_component.h
        src/core/game_object/components/mesh_renderer_component.cpp
        src/util/math_utils.h
        src/renderer/terrain/terrain_manager.cpp
        src/renderer/terrain/terrain_manager.h
        src/renderer/terrain/terrain_types.h
        src/util/heightmap_utils.h
        src/util/noise_utils.h
        src/renderer/terrain/terrain_chunk.cpp
        src/renderer/terrain/terrain_chunk.h
        src/renderer/pipelines/geometry/terrain/terrain_pipeline.cpp
        src/renderer/pipelines/geometry/terrain/terrain_pipeline.h
        src/core/scene/map.cpp
        src/core/scene/map.h
        src/core/game_object/terrain.h
        src/renderer/assets/material/material.cpp
        src/renderer/assets/material/material.h
        src/renderer/assets/texture/texture.cpp
        src/renderer/assets/texture/texture.h
        src/renderer/assets/asset_manager.cpp
        src/renderer/assets/asset_manager.h
        src/core/game_object/components/terrain_component.cpp
        src/core/game_object/components/terrain_component.h
        src/renderer/terrain/terrain_constants.h
        src/core/scene/serializer_types.h
        src/core/scene/serializer_constants.h
        src/renderer/assets/texture/texture_resource.cpp
        src/renderer/assets/texture/texture_resource.h
        src/renderer/assets/texture/texture_types.h
        src/renderer/pipelines/shadows/ground_truth_ambient_occlusion/ground_truth_ambient_occlusion_pipeline.cpp
        src/renderer/pipelines/shadows/ground_truth_ambient_occlusion/ground_truth_ambient_occlusion_pipeline.h
        src/renderer/pipelines/shadows/ground_truth_ambient_occlusion/ambient_occlusion_types.h
        src/renderer/pipelines/geometry/transparent_pipeline/transparent_pipeline.cpp
        src/renderer/pipelines/geometry/transparent_pipeline/transparent_pipeline.h
        src/engine_constants.h
        src/renderer/pipelines/shadows/contact_shadow/contact_shadows_pipeline.cpp
        src/renderer/pipelines/shadows/contact_shadow/contact_shadows_pipeline.h
        src/renderer/pipelines/shadows/contact_shadow/contact_shadows_pipeline_types.h

        src/renderer/pipelines/geometry/environment/environment_pipeline_types.h
        src/renderer/pipelines/basic/basic_render/basic_render_pipeline_types.h
        src/renderer/pipelines/post/post_process/post_process_pipeline_types.h
        src/renderer/pipelines/post/temporal_antialiasing/temporal_antialiasing_pipeline_types.h
        src/renderer/pipelines/geometry/terrain/terrain_pipeline_types.h
        src/renderer/pipelines/geometry/transparent_pipeline/transparent_pipeline_types.h
        src/renderer/pipelines/basic/basic_compute/basic_compute_pipeline_types.h
        src/renderer/pipelines/visibility_pass/visibility_pass_pipeline_types.h
        src/renderer/pipelines/debug/debug_renderer.cpp
        src/renderer/pipelines/debug/debug_renderer.h
        src/renderer/pipelines/debug/debug_renderer_types.h
        src/renderer/pipelines/debug/debug_composite_pipeline.cpp
        src/renderer/pipelines/debug/debug_composite_pipeline.h
        src/renderer/pipelines/debug/debug_highlighter.cpp
        src/renderer/pipelines/debug/debug_highlighter.h
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