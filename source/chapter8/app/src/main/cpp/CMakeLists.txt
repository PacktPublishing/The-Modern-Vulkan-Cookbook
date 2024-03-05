cmake_minimum_required(VERSION 3.22)
project("open_xr_3")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set_property(
        GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake predefined targets"
)

if (ANDROID)
    add_definitions(-DXR_USE_PLATFORM_ANDROID)
endif ()

set(LOCAL_HEADERS
        Common.h
        OXRContext.h
        OXRSwapchain.h
        xr_linear.h
        )
set(LOCAL_SOURCES
        main.cpp
        OXRContext.cpp
        OXRSwapchain.cpp
        )
# set(VULKAN_SHADERS vulkan_shaders/frag.glsl vulkan_shaders/vert.glsl)

include_directories(${JNI_INCLUDE_DIRS} ${_classDir} ${_stubDir})

message(STATUS "Android: " ${ANDROID})
message(STATUS "Android NDK location " ${ANDROID_NDK})
message(STATUS "Android ABI: " ${ANDROID_ABI})

#set(BUILD_TESTS
#        OFF
#        CACHE INTERNAL "Build tests"
#        )
#set(BUILD_API_LAYERS
#        ON
#        CACHE INTERNAL "Use OpenXR layers"
#        )
#FetchContent_Declare(
#        OpenXR
#        GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK
#        TAG release-1.0.32.1
#        SOURCE_DIR
#        openxr
#)
#FETCHCONTENT_POPULATE(OpenXR)
#FetchContent_MakeAvailable(OpenXR)

# glm
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(glm)
include_directories(${glm_SOURCE_DIR})

#gltfsdk
FetchContent_Declare(
        gltfsdk
        GIT_REPOSITORY https://github.com/microsoft/glTF-SDK.git
        GIT_TAG        r1.9.6.0
)

set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)

if(NOT gltfsdk_POPULATED)
    set(ENABLE_UNIT_TESTS OFF CACHE BOOL "" FORCE)
    set(ENABLE_SAMPLES OFF CACHE BOOL "" FORCE)
    set(RAPIDJSON_BUILD_DOC OFF CACHE BOOL "" FORCE)
    set(RAPIDJSON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(RAPIDJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Populate(gltfsdk)
endif()
message(${gltfsdk_SOURCE_DIR})
#add_subdirectory(${gltfsdk_SOURCE_DIR})
#include_directories(${gltfsdk_SOURCE_DIR}/GLTFSDK/Inc)

FetchContent_Declare(
        bshoshanythreadpool
        GIT_REPOSITORY https://github.com/bshoshany/thread-pool.git
        GIT_TAG v3.3.0
)

FetchContent_GetProperties(bshoshanythreadpool)
if(NOT bshoshanythreadpool_POPULATED)
FetchContent_Populate(bshoshanythreadpool)
endif()

message(${bshoshanythreadpool_SOURCE_DIR})

option(BUILD_LOADER "Build loader" OFF)
if (ANDROID)
    if ((NOT ${ANDROID_ABI} MATCHES "x86-64") AND (NOT ${ANDROID_ABI} MATCHES "x86"))
        add_library(OpenXR::openxr_loader SHARED IMPORTED)
        set_property(
                TARGET
                OpenXR::openxr_loader
                PROPERTY
                IMPORTED_LOCATION
                ${PROJECT_SOURCE_DIR}/thirdparty/openxr/OpenXR/Libs/Android/${ANDROID_ABI}/${CMAKE_BUILD_TYPE}/libopenxr_loader.so
        )
    endif ()
endif ()

if (ANDROID)
    add_library(
            open_xr_3 MODULE
            ${LOCAL_SOURCES}
            ${LOCAL_HEADERS}
            # ${VULKAN_SHADERS}
            #        $<TARGET_OBJECTS:android_native_app_glue>
    )

    # VulkanCore
    set(VulkanCore_src_DIR ../../../../../../)
    set(VulkanCore_build_DIR ../../../../../a_out)
    file(MAKE_DIRECTORY ${VulkanCore_build_DIR})

    add_subdirectory(${VulkanCore_src_DIR} ${VulkanCore_build_DIR} EXCLUDE_FROM_ALL)
    include_directories(${VulkanCore_src_DIR}/source)
    target_link_libraries(open_xr_3 VulkanCore EngineCore)

    include_directories(${gltfsdk_SOURCE_DIR}/GLTFSDK/Inc)

    include_directories(${bshoshanythreadpool_SOURCE_DIR})

    # EngineCore
#    set(EngineCore_src_DIR ../../../../../../)
#    set(EngineCore_build_DIR ../../../../../a_out)
#    file(MAKE_DIRECTORY ${EngineCore_build_DIR})
#
#    add_subdirectory(${EngineCore_src_DIR} ${EngineCore_build_DIR} EXCLUDE_FROM_ALL)
#    include_directories(${EngineCore_src_DIR}/source)
#    target_link_libraries(open_xr_3 EngineCore)

    # Vulkan
    find_package(Vulkan)
    if (Vulkan_FOUND)
        set(XR_USE_GRAPHICS_API_VULKAN TRUE)
        add_definitions(-DXR_USE_GRAPHICS_API_VULKAN)
        message(STATUS "Enabling Vulkan support")
        message(STATUS "Vulkan include dir: " ${Vulkan_INCLUDE_DIRS})
        message(STATUS "Vulkan lib: " ${Vulkan_LIBRARY})

        target_include_directories(open_xr_3 PRIVATE ${Vulkan_INCLUDE_DIRS})
        # target_link_libraries(open_xr_3 ${Vulkan_LIBRARY})
        # target_link_libraries(open_xr_3 ${Vulkan_LIBRARY}/libVulkan)
    endif ()

    # VMA
    FetchContent_Declare(
            vma
            GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
            GIT_TAG v3.0.1
    )
    FetchContent_MakeAvailable(vma)
    include_directories(${vma_SOURCE_DIR}/include)

    if (ANDROID)
        find_library(ANDROID_LIBRARY NAMES android)
        find_library(ANDROID_LOG_LIBRARY NAMES log)
    endif ()

    message(STATUS "Android libraries: " ${ANDROID_LIBRARY} ${ANDROID_LOG_LIBRARY})
    target_link_libraries(
            open_xr_3 ${ANDROID_LIBRARY} ${ANDROID_LOG_LIBRARY}
    )

    message(STATUS "open_xr_3 will default to Vulkan graphics plugin")
    target_compile_definitions(
            open_xr_3 PRIVATE Vulkan # DEFAULT_GRAPHICS_PLUGIN_VULKAN
    )

    add_library(app-glue
            STATIC
            ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
endif ()

source_group("Headers" FILES ${LOCAL_HEADERS})
source_group("Shaders" FILES ${VULKAN_SHADERS})

target_link_libraries(open_xr_3 app-glue)
if (ANDROID)
    target_link_libraries(open_xr_3 OpenXR::openxr_loader)
endif ()

set(native_app_glue_dir "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")

target_include_directories(
        open_xr_3
        PRIVATE
        # For including compiled shaders
        "${CMAKE_CURRENT_BINARY_DIR}"
        # for OpenXR headers
        ${PROJECT_BINARY_DIR}/include
        ${native_app_glue_dir}
        ${PROJECT_SOURCE_DIR}/thirdparty/OpenXR-SDK/include
        ${gltfsdk_SOURCE_DIR}/GLTFSDK/Inc
        ${bshoshanythreadpool_SOURCE_DIR}
)

#if (GLSLANG_VALIDATOR AND NOT GLSLC_COMMAND)
#    target_compile_definitions(open_xr_3 PRIVATE USE_GLSLANGVALIDATOR)
#endif ()
#
#if (TARGET openxr-gfxwrapper)
#    target_link_libraries(open_xr_3 PRIVATE openxr-gfxwrapper)
#endif ()