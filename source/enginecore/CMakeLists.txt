cmake_minimum_required(VERSION 3.15)

project(EngineCore)
file(GLOB_RECURSE sources CONFIGURE_DEPENDS *.cpp *.hpp *.glsl *.comp *.frag *.vert *.rgen *.rmiss *.rchit *.rahit)

add_library(${PROJECT_NAME} ${sources})

target_link_libraries(${PROJECT_NAME} GLTFSDK meshoptimizer)

include_directories(../)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
set_property(TARGET ${PROJECT_NAME} PROPERTY CMAKE_CXX_STANDARD 20)