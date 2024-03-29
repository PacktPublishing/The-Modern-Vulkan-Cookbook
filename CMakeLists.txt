cmake_minimum_required(VERSION 3.20)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project("Modern Vulkan Cookbook" VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    #ShaderMake requires this as minimum version for NRD
    # which is required for ray tracing project
    set(_MIN_WINDOWS_SDK_VERSION_BUILD 20348)
endif()

# third party & vulkan etc
find_package(Vulkan REQUIRED SPIRV-Tools)

# Download dependencies automatically
include(FetchContent)

if (VulkanHeaderVersion GREATER_EQUAL 275)
  FetchContent_Declare(glslang
      GIT_REPOSITORY https://github.com/KhronosGroup/glslang
      GIT_TAG vulkan-sdk-1.3.${VulkanHeaderVersion}.0)
  if (NOT glslang_POPULATED)
    set(ENABLE_OPT OFF)
  endif()
  set(GLSLANG_LIB "glslang") 
else()
  find_package(Vulkan REQUIRED SPIRV-Tools glslang)
endif()

include_directories(${Vulkan_INCLUDE_DIR})
message(STATUS "Vulkan_INCLUDE_DIR: ${Vulkan_INCLUDE_DIR}")
link_directories(${Vulkan_INCLUDE_DIR}/../lib)


FetchContent_Declare(spirvreflect
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
    GIT_TAG sdk-1.3.236.0)

if(NOT spirvreflect_POPULATED)
  set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "SPIRV_REFLECT_EXE" FORCE)
  set(SPIRV_REFLECT_EXAMPLES OFF CACHE BOOL "SPIRV_REFLECT_EXAMPLES" FORCE)
  set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "SPIRV_REFLECT_STATIC_LIB" FORCE)
  FetchContent_Populate(spirvreflect)
endif()
add_subdirectory(${spirvreflect_SOURCE_DIR})

FetchContent_MakeAvailable(spirvreflect)

FetchContent_Declare(
        volk
        GIT_REPOSITORY https://github.com/zeux/volk.git
        GIT_TAG 1.3.270
)
FetchContent_MakeAvailable(volk)

FetchContent_Declare(
        vma
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG v3.0.1
)
FetchContent_MakeAvailable(vma)
include_directories(${vma_SOURCE_DIR}/include)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0.9.9.8
)

set(GLI_TEST_ENABLE OFF CACHE INTERNAL "")
FetchContent_Declare(
    gli
    GIT_REPOSITORY https://github.com/g-truc/gli.git
)
FetchContent_Populate(gli)
include_directories(${gli_SOURCE_DIR})

if (NOT ANDROID)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.89.9
)
FetchContent_GetProperties(imgui)
if (NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
    add_Library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
    target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
    target_compile_definitions(imgui PUBLIC IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
endif ()

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.3.8
)
if(NOT glfw_POPULATED)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Populate(glfw)
endif()

add_subdirectory(${glfw_SOURCE_DIR})
endif()

FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
)
FetchContent_Populate(stb)

FetchContent_Declare (
  tracy
  GIT_REPOSITORY https://github.com/wolfpld/tracy.git
  GIT_TAG v0.9.1
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)

FetchContent_Declare(
  threadpool
  GIT_REPOSITORY https://github.com/bshoshany/thread-pool.git
  GIT_TAG v3.3.0
)

FetchContent_Populate(threadpool)

FetchContent_Declare(
    freetype
    GIT_REPOSITORY https://github.com/freetype/freetype.git
    GIT_TAG        VER-2-13-0
)
FetchContent_GetProperties(freetype)
if(NOT freetype_POPULATED)
        FetchContent_Populate(freetype)
        # Set variables for FreeType
        set(FT_DISABLE_ZLIB ON CACHE BOOL "Disable zlib for embedded Freetype" FORCE)
        set(FT_DISABLE_BZIP2 ON CACHE BOOL "Disable bzip2 for embedded Freetype" FORCE)
        set(FT_DISABLE_PNG ON CACHE BOOL "Disable libpng for embedded Freetype" FORCE)
        set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "Disable harfbuzz for embedded Freetype" FORCE)
        set(FT_DISABLE_BROTLI ON CACHE BOOL "Disable brotli for embedded Freetype" FORCE)
        set(BUILD_SHARED_LIBS OFF)       # Force static link

        # Import FreeType directory
        add_subdirectory(${freetype_SOURCE_DIR} ${freetype_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

FetchContent_Declare(
    font-demo
    GIT_REPOSITORY https://github.com/kocsis1david/font-demo
    GIT_TAG        f29e61effae795c0f470e3bbb8547bf0e812e9cf
)
FetchContent_GetProperties(font-demo)

if(NOT font-demo_POPULATED)
        FetchContent_Populate(font-demo)
        message(STATUS "font-demo source dir: ${font-demo_SOURCE_DIR}")
        add_library(font-load-glyp "${font-demo_SOURCE_DIR}/geometry.c" "${font-demo_SOURCE_DIR}/outline.c")
        add_definitions(-DFONT_DEMO_SOURCE_DIR="${font-demo_SOURCE_DIR}")
endif()

FetchContent_Declare(
  gltfsdk
  GIT_REPOSITORY https://github.com/microsoft/glTF-SDK.git
  GIT_TAG        r1.9.6.0
)
if(NOT gltfsdk_POPULATED)
  set(ENABLE_UNIT_TESTS OFF CACHE BOOL "" FORCE)
  set(ENABLE_SAMPLES OFF CACHE BOOL "" FORCE)
  set(RAPIDJSON_BUILD_DOC OFF CACHE BOOL "" FORCE)
  set(RAPIDJSON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(RAPIDJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  FetchContent_Populate(gltfsdk)
endif()
message(${gltfsdk_SOURCE_DIR})
add_subdirectory(${gltfsdk_SOURCE_DIR})

FetchContent_Declare(
  meshopt
  GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
  GIT_TAG v0.18
)
if(NOT meshopt_POPULATED)
  FetchContent_Populate(meshopt)
endif()
message(${meshopt_SOURCE_DIR})
add_subdirectory(${meshopt_SOURCE_DIR})

FetchContent_Declare(
  dlss
  GIT_REPOSITORY https://github.com/NVIDIA/DLSS.git
  GIT_TAG        v3.5.0
)
if(NOT dlss_POPULATED)
  FetchContent_Populate(dlss)
endif()
message(STATUS "DLSS_SRC_DIR: ${dlss_SOURCE_DIR}")
message(${dlss_SOURCE_DIR})

set(DLSS_DLL "${dlss_SOURCE_DIR}/lib/Windows_x86_64/rel/nvngx_dlss.dll")

link_directories(${dlss_SOURCE_DIR}/lib/Windows_x86_64/x86_64/)

# Make libs available
if (ANDROID)
  FetchContent_MakeAvailable(glm gli stb tracy gltfsdk meshopt dlss volk "${GLSLANG_LIB}")
else()
	FetchContent_MakeAvailable(glm gli imgui stb tracy gltfsdk meshopt dlss volk "${GLSLANG_LIB}")
endif()

include_directories(${stb_SOURCE_DIR})
include_directories(${volk_SOURCE_DIR})
include_directories(${glm_SOURCE_DIR})
include_directories(${gli_SOURCE_DIR})
include_directories(${spirvcross_SOURCE_DIR})
include_directories(${spirvreflect_SOURCE_DIR})
include_directories(${gltfsdk_SOURCE_DIR}/GLTFSDK/Inc)
if (NOT ANDROID)
  include_directories(${GLFW_SOURCE_DIR}/include)
  include_directories(${imgui_SOURCE_DIR}/)
endif()
message(WARNING "Tracy path: ${tracy_SOURCE_DIR}")
include_directories(${tracy_SOURCE_DIR}/public)
include_directories(${threadpool_SOURCE_DIR})
include_directories(${meshopt_SOURCE_DIR}/src)
include_directories(${freetype_SOURCE_DIR}/include)
include_directories(${font-demo_SOURCE_DIR})
include_directories(${dlss_SOURCE_DIR}/include/)

set(RequiredVulkanSDKLIBS 
    debug OSDependentd optimized OSDependent
    debug MachineIndependentd optimized MachineIndependent
    debug GenericCodeGend optimized GenericCodeGen
    debug glslangd optimized glslang
    debug SPIRVd optimized SPIRV
    debug SPIRV-Toolsd optimized SPIRV-Tools
    debug SPIRV-Tools-optd optimized SPIRV-Tools-opt
    debug glslang-default-resource-limitsd optimized glslang-default-resource-limits
    debug spirv-cross-cored optimized spirv-cross-core
    debug spirv-cross-glsld optimized spirv-cross-glsl
    debug spirv-cross-reflectd optimized spirv-cross-reflect)
if (GLSLANG_LIB)
  set(RequiredVulkanSDKLIBS 
      debug OSDependentd optimized OSDependent
      debug MachineIndependentd optimized MachineIndependent
      debug GenericCodeGend optimized GenericCodeGen
      debug glslangd optimized glslang
      debug SPIRVd optimized SPIRV
      debug SPIRV-Toolsd optimized SPIRV-Tools
      debug SPIRV-Tools-optd optimized SPIRV-Tools-opt
      debug glslang-default-resource-limitsd optimized glslang-default-resource-limits
      debug spirv-cross-cored optimized spirv-cross-core
      debug spirv-cross-glsld optimized spirv-cross-glsl
      debug spirv-cross-reflectd optimized spirv-cross-reflect)
endif()

add_subdirectory(source/vulkancore)
add_subdirectory(source/enginecore)
if(WIN32)
add_subdirectory(source/chapter1)
add_subdirectory(source/chapter2)
add_subdirectory(source/chapter3)
add_subdirectory(source/chapter4)
add_subdirectory(source/chapter5)
add_subdirectory(source/chapter6)
add_subdirectory(source/chapter7)
endif()