cmake_minimum_required(VERSION 3.15)

project(VulkanCore)
file(GLOB_RECURSE sources CONFIGURE_DEPENDS *.cpp *.hpp)

add_library(${PROJECT_NAME} ${sources})

if (TRACY_ENABLE)
  add_compile_definitions(TRACY_ENABLE)
endif()
if (TRACY_CALLSTACK)
  add_compile_definitions(TRACY_CALLSTACK)
endif()

message(STATUS "Vulkan DIR: " ${Vulkan_INCLUDE_DIR})
include_directories(${Vulkan_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME} INTERFACE volk TracyClient)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
set_property(TARGET ${PROJECT_NAME} PROPERTY CMAKE_CXX_STANDARD 20)