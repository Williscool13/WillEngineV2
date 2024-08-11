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
)

add_definitions(-DVK_NO_PROTOTYPES)

add_executable(WillEngine main.cpp
        src/core/engine.cpp
        src/core/engine.h
        src/util/VkHelpers.cpp
        src/util/VkHelpers.h
        src/core/VkTypes.cpp
        src/core/VkTypes.h
        src/core/VkDescriptors.cpp
        src/core/VkDescriptors.h
        ${IMGUI_SOURCES})

target_include_directories(WillEngine PRIVATE
        ${Vulkan_INCLUDE_DIRS}                                      # Vulkan
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/SDL2/include             # SDL
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/VulkanMemoryAllocator    # VMA
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/stb                      # STBI
        ${IMGUI_DIR}                                                # ImGui
        ${IMGUI_DIR}/backends                                       # ImGui backends
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
