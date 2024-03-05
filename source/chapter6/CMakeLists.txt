cmake_minimum_required(VERSION 3.15)

project(Chapter6)
file(GLOB_RECURSE Chapter06_DLSS_SOURCES CONFIGURE_DEPENDS mainDLSS.cpp)
file(GLOB_RECURSE Chapter06_MSAA_SOURCES CONFIGURE_DEPENDS mainMSAA.cpp)
file(GLOB_RECURSE Chapter06_TAA_SOURCES CONFIGURE_DEPENDS mainTAA.cpp)
file(GLOB_RECURSE Chapter06_FXAA_SOURCES CONFIGURE_DEPENDS mainFXAA.cpp)
file(GLOB_RECURSE Chapter06_SpecAA_SOURCES CONFIGURE_DEPENDS mainSpecAA.cpp)

# List of recipes in the chapter
set (recipe_names
     "Chapter06_DLSS"
	 "Chapter06_MSAA"
     "Chapter06_FXAA"
     "Chapter06_TAA")

foreach (targ ${recipe_names})
  add_executable(${targ} ${${targ}_SOURCES}) 

  target_compile_features(${targ} PUBLIC cxx_std_20)
  set_property(TARGET ${targ} PROPERTY CMAKE_CXX_STANDARD 20)
  set (LinkLibs VulkanCore EngineCore GLTFSDK glfw imgui ${RequiredVulkanSDKLIBS} spirv-reflect-static TracyClient meshoptimizer)  
  
  if(${targ} STREQUAL "Chapter06_DLSS")
    list(APPEND LinkLibs debug nvsdk_ngx_d_dbg optimized nvsdk_ngx_d)

    add_custom_command(TARGET ${targ} POST_BUILD 
               COMMAND ${CMAKE_COMMAND} -E copy_if_different
               ${DLSS_DLL}
                $<TARGET_FILE_DIR:${targ}>)

  endif()

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