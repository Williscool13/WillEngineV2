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

find_library(SDL2MAIN_LIBRARIES
        NAMES SDL2main${LIB_SUFFIX}
        PATHS ${CMAKE_CURRENT_SOURCE_DIR}/lib
        REQUIRED
)

find_library(SDL2_LIBRARIES
        NAMES SDL2${LIB_SUFFIX}
        PATHS ${CMAKE_CURRENT_SOURCE_DIR}/lib
        REQUIRED
)

add_subdirectory(extern/fmt)
add_subdirectory(extern/fastgltf)
add_subdirectory(extern/fastgltf/deps) # simdjson

# Jolt Physics
set(PHYSICS_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/extern/JoltPhysics")
include(extern/JoltPhysics/Jolt/Jolt.cmake)
set(CMAKE_CXX_STANDARD 20) # Jolt sets it to 17

include_directories(${CMAKE_SOURCE_DIR})

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui)
set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
        ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/ImGuiFileDialog/ImGuiFileDialog.cpp
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
        ${VK_BOOTSTRAP_SOURCES}
        ${VOLK_SOURCES}
        ${ENGINE_SOURCES}
        ${PHYSICS_SOURCES}
        ${RENDERER_SOURCES}
        ${CORE_SOURCES}
        ${UTIL_SOURCES}
        ${WINDOWS_SOURCES}
)

target_include_directories(WillEngine PRIVATE
        ${Vulkan_INCLUDE_DIRS}                                              # Vulkan
        ${IMGUI_DIR}                                                        # ImGui
        ${IMGUI_DIR}/backends                                               # ImGui backends
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/JoltPhysics
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/shaderc                          # ShaderC
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/SDL2                             # SDL2
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/fastgltf/include                 # fastgltf
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/fastgltf/deps/simdjson           # simdjson

)

target_link_libraries(WillEngine PRIVATE
        ${SDL2MAIN_LIBRARIES}
        ${SDL2_LIBRARIES}
        ${VOLK_LIBRARIES}
        ${SHADERC_LIBRARIES}
        fmt::fmt
        fastgltf
        Jolt
)

set(SDL_DLL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/SDL2.dll")
set(SDLD_DLL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/SDL2d.dll")

add_custom_command(TARGET WillEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SDL_DLL_PATH} $<TARGET_FILE_DIR:WillEngine>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SDLD_DLL_PATH} $<TARGET_FILE_DIR:WillEngine>
)