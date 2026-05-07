cmake_minimum_required(VERSION 3.25)
project(renderer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(library/vulkan-headers)
if(NOT TARGET Vulkan::Headers)
    add_library(Vulkan::Headers INTERFACE IMPORTED)
    set_target_properties(Vulkan::Headers PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_SOURCE_DIR}/library/vulkan-headers/include"
    )
endif()

add_subdirectory(library/volk)
add_subdirectory(library/vma)
add_subdirectory(library/glfw)

set(SOURCES
	src/main.cc
	src/engine.cc

	# core
	src/core/error/error_list.cc
	src/core/error/error_macros.cc

	# renderer
	src/renderer/driver_context.cc
	src/renderer/driver_device.cc
	src/renderer/rendering_device.cc

	src/renderer/vma.cc
)

add_compile_options(-g)
add_compile_options(-Wall)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Headers
	volk
	VulkanMemoryAllocator
	glfw
)
