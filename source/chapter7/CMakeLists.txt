cmake_minimum_required(VERSION 3.15)

project(Chapter7)
file(GLOB_RECURSE Chapter07_RayTracer_SOURCES CONFIGURE_DEPENDS mainRayTracer.cpp)
file(GLOB_RECURSE Chapter07_HybridRenderer_SOURCES CONFIGURE_DEPENDS mainHybridRenderer.cpp)

# List of recipes in the chapter
set (recipe_names
     "Chapter07_RayTracer"
     "Chapter07_HybridRenderer")

foreach (targ ${recipe_names})
  add_executable(${targ} ${${targ}_SOURCES})

  target_compile_features(${targ} PUBLIC cxx_std_20)
  set_property(TARGET ${targ} PROPERTY CMAKE_CXX_STANDARD 20)
  set (LinkLibs VulkanCore EngineCore GLTFSDK glfw imgui ${RequiredVulkanSDKLIBS} spirv-reflect-static TracyClient meshoptimizer) 
    target_link_libraries(${targ} PUBLIC ${LinkLibs})

endforeach()

include_directories(${PROJECT_SOURCE_DIR}/../)

file(COPY ../common/resources DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

add_custom_target(copy_resources_${PROJECT_NAME} ALL
                  COMMAND ${CMAKE_COMMAND} -E copy_directory
                      "${CMAKE_CURRENT_SOURCE_DIR}/resources" "${CMAKE_CURRENT_BINARY_DIR}/resources"
                  COMMAND ${CMAKE_COMMAND} -E copy_directory
                      "${CMAKE_CURRENT_SOURCE_DIR}/../enginecore/resources/shaders" "${CMAKE_CURRENT_BINARY_DIR}/resources/shaders"
)