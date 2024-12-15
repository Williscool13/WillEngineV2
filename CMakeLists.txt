cmake_minimum_required(VERSION 3.28)
project(WillEngine)

set(CMAKE_CXX_STANDARD 20)

add_definitions(-DVK_NO_PROTOTYPES)
add_definitions(-DGLM_FORCE_DEPTH_ZERO_TO_ONE)
add_definitions(-DGLM_ENABLE_EXPERIMENTAL)

#add_definitions(-DJPH_PROFILE_ENABLED=0)
#add_definitions(-DJPH_DEBUG_RENDERER=0)

# Vulkan
find_package(Vulkan REQUIRED)

# SDL2
set(SDL2_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/extern/SDL2/lib/SDL2.lib")
set(SDL2MAIN_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/extern/SDL2/lib/SDL2main.lib")

# Jolt Physics
set(PHYSICS_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/extern/JoltPhysics")

add_subdirectory(extern/glm)
add_subdirectory(extern/fmt)
add_subdirectory(extern/volk)
add_subdirectory(extern/vk-bootstrap)
add_subdirectory(extern/fastgltf)
add_subdirectory(extern/fastgltf/deps) # simdjson

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
        src/renderer/resource_manager.cpp
        src/renderer/resource_manager.h
        src/renderer/immediate_submitter.cpp
        src/renderer/immediate_submitter.h
        src/physics/physics_body.cpp
        src/physics/physics_body.h
        src/physics/physics_utils.cpp
        src/physics/physics_utils.h
        src/game/player/player.cpp
        src/game/player/player.h
        src/core/imgui_wrapper.cpp
        src/core/imgui_wrapper.h
)


add_executable(WillEngine main.cpp
        ${IMGUI_SOURCES}
        ${FASTGLTF_SOURCES}
        ${HALF_SOURCES}
        src/core/Engine.cpp
        src/core/Engine.h
        src/renderer/render_object/render_object.cpp
        src/renderer/render_object/render_object.h
        src/core/scene.cpp
        src/core/scene.h
        src/core/input.cpp
        src/core/input.h
        src/core/game_object.cpp
        src/core/game_object.h
        src/game/camera/free_camera.cpp
        src/game/camera/free_camera.h
        src/core/camera/camera.cpp
        src/core/camera/camera.h
        src/physics/physics.cpp
        src/physics/physics.h
        src/physics/physics_filters.cpp
        src/physics/physics_filters.h
        src/renderer/vulkan_context.cpp
        src/renderer/vulkan_context.h
        src/renderer/environment/environment.cpp
        src/renderer/environment/environment.h
        src/renderer/pipelines/acceleration_algorithms/frustum_culling_descriptor_layout.cpp
        src/renderer/pipelines/acceleration_algorithms/frustum_culling_descriptor_layout.h
        src/renderer/pipelines/acceleration_algorithms/frustum_culling_pipeline.cpp
        src/renderer/pipelines/acceleration_algorithms/frustum_culling_pipeline.h
        src/renderer/pipelines/deferred/deferred_mrt.cpp
        src/renderer/pipelines/deferred/deferred_mrt.h
        src/renderer/pipelines/deferred/deferred_resolve.cpp
        src/renderer/pipelines/deferred/deferred_resolve.h
        src/renderer/pipelines/post_processing/temporal_antialiasing.cpp
        src/renderer/pipelines/post_processing/temporal_antialiasing.h
        src/renderer/pipelines/environment/environment_map_pipeline.cpp
        src/renderer/pipelines/environment/environment_map_pipeline.h
        src/renderer/pipelines/post_processing/post_process.cpp
        src/renderer/pipelines/post_processing/post_process.h
        src/renderer/pipelines/environment/environment_descriptor_layouts.cpp
        src/renderer/pipelines/environment/environment_descriptor_layouts.h
        src/renderer/scene/scene_descriptor_layouts.cpp
        src/renderer/scene/scene_descriptor_layouts.h
        src/renderer/render_object/render_object_descriptor_layout.cpp
        src/renderer/render_object/render_object_descriptor_layout.h
        src/renderer/vk_helpers.cpp
        src/renderer/vk_helpers.h
        src/renderer/vk_types.cpp
        src/renderer/vk_types.h
        src/renderer/vk_descriptors.cpp
        src/renderer/vk_descriptors.h
        src/renderer/vk_descriptor_buffer.cpp
        src/renderer/vk_descriptor_buffer.h
        src/renderer/vk_loader.cpp
        src/renderer/vk_loader.h
        src/renderer/vk_pipelines.cpp
        src/renderer/vk_pipelines.h
        src/renderer/data_structures/bounding_sphere.cpp
        src/renderer/data_structures/bounding_sphere.h
        src/util/math_constants.h
        src/util/time_utils.cpp
        src/util/time_utils.h
        src/util/transform.cpp
        src/util/transform.h
        src/util/file_utils.cpp
        src/util/file_utils.h
        src/util/halton.cpp
        src/util/halton.h
)


target_include_directories(WillEngine PRIVATE
        ${Vulkan_INCLUDE_DIRS}                                              # Vulkan
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/SDL2/include                     # SDL
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/VulkanMemoryAllocator/include    # VMA
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/stb                              # STBI
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/volk                             # volk
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/fastgltf/include                 # fastgltf
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/fastgltf/deps/simdjson           # simdjson
        ${IMGUI_DIR}                                                        # ImGui
        ${IMGUI_DIR}/backends                                               # ImGui backends
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/JoltPhysics
)



add_custom_target(compile_shaders
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/shaders/compile_shaders_fast.sh
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/shaders
        DEPENDS ${shader_copy_target}
        COMMENT "Compiling shaders"
)
add_dependencies(WillEngine compile_shaders)

target_link_libraries(WillEngine PRIVATE
        ${SDL2MAIN_LIBRARY} # Needs to happen before SDL2_LIBRARY
        ${SDL2_LIBRARY}
        #${Vulkan_LIBRARIES} Replaced by VOLK
        glm::glm
        fmt::fmt
        volk
        vk-bootstrap::vk-bootstrap
        fastgltf
        Jolt
)