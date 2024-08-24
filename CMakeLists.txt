cmake_minimum_required(VERSION 3.28)
project(WillEngine)

set(CMAKE_CXX_STANDARD 20)

# Vulkan
find_package(Vulkan REQUIRED)

# SDL2
set(SDL2_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/extern/SDL2/lib/SDL2.lib")
set(SDL2MAIN_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/extern/SDL2/lib/SDL2main.lib")

# GLM
add_subdirectory(extern/glm)
# fmt
add_subdirectory(extern/fmt)
# volk
add_subdirectory(extern/volk)
# vkb
add_subdirectory(extern/vk-bootstrap)

# ImGui setup
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui)
set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
        ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
        src/core/camera/free_camera.cpp
        src/core/camera/free_camera.h
        src/core/render_object.cpp
        src/core/render_object.h
)

add_definitions(-DVK_NO_PROTOTYPES)

add_executable(WillEngine main.cpp
        ${IMGUI_SOURCES}
        src/core/Engine.cpp
        src/core/Engine.h
        src/renderer/vk_helpers.cpp
        src/renderer/vk_helpers.h
        src/renderer/vk_types.cpp
        src/renderer/vk_types.h
        src/renderer/vk_descriptors.cpp
        src/renderer/vk_descriptors.h
        src/renderer/vk_descriptor_buffer.cpp
        src/renderer/vk_descriptor_buffer.h
        src/util/math_constants.h
        src/renderer/vk_pipelines.cpp
        src/renderer/vk_pipelines.h
        src/core/scene.cpp
        src/core/scene.h
        src/core/camera/camera.cpp
        src/core/camera/camera.h
        src/core/input.cpp
        src/core/input.h
        src/core/game_object.cpp
        src/core/game_object.h
        src/util/time_utils.cpp
        src/util/time_utils.h
        src/util/transform.cpp
        src/util/transform.h)


add_custom_target(compile_shaders
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/shaders/compile_shaders_fast.sh
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        COMMENT "Compiling shaders"
)

add_dependencies(WillEngine compile_shaders)

target_include_directories(WillEngine PRIVATE
        ${Vulkan_INCLUDE_DIRS}                                              # Vulkan
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/SDL2/include                     # SDL
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/VulkanMemoryAllocator/include    # VMA
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/stb                              # STBI
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/volk
        ${IMGUI_DIR}                                                        # ImGui
        ${IMGUI_DIR}/backends                                               # ImGui backends
)

target_link_libraries(WillEngine PRIVATE
        ${SDL2MAIN_LIBRARY} # Needs to happen before SDL2_LIBRARY
        ${SDL2_LIBRARY}
        #${Vulkan_LIBRARIES} Replaced by VOLK
        glm::glm
        fmt::fmt
        volk
        vk-bootstrap::vk-bootstrap
)

set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
add_custom_command(
        OUTPUT ${SHADER_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADER_SOURCE_DIR} ${SHADER_BINARY_DIR}
        DEPENDS ${SHADER_SOURCE_DIR}
        COMMENT "Copying shaders to build directory"
)
add_custom_target(copy_shaders DEPENDS ${SHADER_BINARY_DIR})
add_dependencies(WillEngine copy_shaders)
