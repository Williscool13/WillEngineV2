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
)

set(CORE_SOURCES
        src/core/engine.cpp
        src/core/engine.h
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
        src/core/game_object/physics_body.h
)


set(PHYSICS_SOURCES
        src/physics/physics.cpp
        src/physics/physics.h
        src/physics/physics_filters.cpp
        src/physics/physics_filters.h
        src/physics/physics_types.h
        src/physics/physics_utils.h
        src/physics/physics_constants.h
        src/physics/debug/debug_renderer.cpp
        src/physics/debug/debug_renderer.h
)

set(RENDERER_SOURCES
        src/renderer/vulkan_context.h
        src/renderer/vulkan_context.cpp
        src/renderer/resource_manager.h
        src/renderer/resource_manager.cpp
        src/renderer/immediate_submitter.h
        src/renderer/immediate_submitter.cpp
        src/renderer/imgui_wrapper.h
        src/renderer/imgui_wrapper.cpp
        src/renderer/vk_descriptors.cpp
        src/renderer/vk_descriptors.h
        src/renderer/vk_helpers.h
        src/renderer/vk_pipelines.cpp
        src/renderer/vk_helpers.cpp
        src/renderer/vk_pipelines.h
        src/renderer/vk_types.cpp
        src/renderer/vk_types.h
        src/renderer/renderer_constants.h
        src/renderer/pipelines/basic_compute/basic_compute_pipeline.cpp
        src/renderer/pipelines/basic_compute/basic_compute_pipeline.h
        src/renderer/pipelines/basic_render/basic_render_pipeline.cpp
        src/renderer/pipelines/basic_render/basic_render_pipeline.h
        src/renderer/descriptor_buffer/descriptor_buffer.cpp
        src/renderer/descriptor_buffer/descriptor_buffer.h
        src/renderer/descriptor_buffer/descriptor_buffer_sampler.cpp
        src/renderer/descriptor_buffer/descriptor_buffer_sampler.h
        src/renderer/descriptor_buffer/descriptor_buffer_types.cpp
        src/renderer/descriptor_buffer/descriptor_buffer_types.h
        src/renderer/descriptor_buffer/descriptor_buffer_uniform.cpp
        src/renderer/descriptor_buffer/descriptor_buffer_uniform.h
        src/renderer/render_object/render_object_constants.h
)

set(GAME_SOURCES

)

set(UTIL_SOURCES
        src/util/math_constants.h
)


if (WIN32)
    enable_language(RC)
    set(WINDOWS_SOURCES
            src/resources.h
            src/resources.rc
            src/core/game_object/renderable.h
            src/renderer/render_object/render_object.h
            src/renderer/render_object/render_object.cpp
            src/renderer/render_object/render_object_types.h
            src/renderer/render_object/render_object_types.cpp
            src/util/file.h
            src/renderer/pipelines/deferred_mrt/deferred_mrt.cpp
            src/renderer/pipelines/deferred_mrt/deferred_mrt.h
            src/renderer/render_object/render_reference.h
            src/util/halton.h
            src/renderer/pipelines/deferred_resolve/deferred_resolve.cpp
            src/renderer/pipelines/deferred_resolve/deferred_resolve.h
            src/renderer/pipelines/environment/environment_pipeline.cpp
            src/renderer/pipelines/environment/environment_pipeline.h
            src/renderer/environment/environment.cpp
            src/renderer/environment/environment.h
            src/renderer/environment/environment_constants.h
            src/renderer/environment/environment_types.h

    )
endif ()

add_executable(WillEngine main.cpp
        ${IMGUI_SOURCES}
        ${FASTGLTF_SOURCES}
        ${HALF_SOURCES}
        ${CORE_SOURCES}
        ${PHYSICS_SOURCES}
        ${RENDERER_SOURCES}
        ${GAME_SOURCES}
        ${UTIL_SOURCES}
        ${WINDOWS_SOURCES}
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