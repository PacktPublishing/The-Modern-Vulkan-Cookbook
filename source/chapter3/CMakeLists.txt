cmake_minimum_required(VERSION 3.15)

project(Chapter3)
file(GLOB_RECURSE Chapter03_GPU_Lines_SOURCES CONFIGURE_DEPENDS mainGPULines.cpp)
file(GLOB_RECURSE Chapter03_GPU_Text_SOURCES CONFIGURE_DEPENDS mainGPUText.cpp)
file(GLOB_RECURSE Chapter03_GPU_Text_SDF_SOURCES CONFIGURE_DEPENDS mainGPUTextSDF.cpp FontManager.hpp FontManager.cpp)
file(GLOB_RECURSE Chapter03_GPU_Culling_SOURCES CONFIGURE_DEPENDS mainCullingCompute.cpp)

# List of recipes in the chapter
set (recipe_names
     "Chapter03_GPU_Lines"
     "Chapter03_GPU_Text_SDF"
     "Chapter03_GPU_Text"
#     "Chapter03_GPU_3"
#     "Chapter03_GPU_4"
     "Chapter03_GPU_Culling")

foreach (targ ${recipe_names})
  add_executable(${targ} ${${targ}_SOURCES})

  target_compile_features(${targ} PUBLIC cxx_std_20)
  set_property(TARGET ${targ} PROPERTY CMAKE_CXX_STANDARD 20)
  target_link_libraries(${targ} PUBLIC VulkanCore EngineCore GLTFSDK glfw imgui ${RequiredVulkanSDKLIBS} spirv-reflect-static TracyClient meshoptimizer freetype font-load-glyp)
endforeach()

include_directories(${PROJECT_SOURCE_DIR}/../)

file(COPY ../common/resources DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

add_custom_target(copy_resources_${PROJECT_NAME} ALL
                  COMMAND ${CMAKE_COMMAND} -E copy_directory
                      "${CMAKE_CURRENT_SOURCE_DIR}/resources" "${CMAKE_CURRENT_BINARY_DIR}/resources"
                  COMMAND ${CMAKE_COMMAND} -E copy_directory
                      "${CMAKE_CURRENT_SOURCE_DIR}/../enginecore/resources/shaders" "${CMAKE_CURRENT_BINARY_DIR}/resources/shaders"
)