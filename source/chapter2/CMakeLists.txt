cmake_minimum_required(VERSION 3.15)

project(Chapter2)
file(GLOB_RECURSE Chapter02_Bindfull_SOURCES CONFIGURE_DEPENDS mainBindfull.cpp *.hpp)
file(GLOB_RECURSE Chapter02_MeshOptimized_SOURCES CONFIGURE_DEPENDS mainMeshOptimized.cpp *.hpp)
file(GLOB_RECURSE Chapter02_MultiDrawIndirect_SOURCES CONFIGURE_DEPENDS mainMultiDrawIndirect.cpp *.hpp)

# List of recipes in the chapter
set (recipe_names
     "Chapter02_Bindfull"
     "Chapter02_MeshOptimized"
     "Chapter02_MultiDrawIndirect")

foreach (targ ${recipe_names})
  add_executable(${targ} ${${targ}_SOURCES})

  target_compile_features(${targ} PUBLIC cxx_std_20)
  set_property(TARGET ${targ} PROPERTY CMAKE_CXX_STANDARD 20)
  target_link_libraries(${targ} PUBLIC VulkanCore EngineCore GLTFSDK glfw imgui ${RequiredVulkanSDKLIBS} spirv-reflect-static TracyClient meshoptimizer)
endforeach()

include_directories(${PROJECT_SOURCE_DIR}/../)

file(COPY ../common/resources DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

add_custom_target(copy_resources_${PROJECT_NAME} ALL
                  COMMAND ${CMAKE_COMMAND} -E copy_directory
                      "${CMAKE_CURRENT_SOURCE_DIR}/resources" "${CMAKE_CURRENT_BINARY_DIR}/resources"
)