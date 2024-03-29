#pragma once

#ifdef _WIN32
#if !defined(VK_USE_PLATFORM_WIN32_KHR)
#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#endif

// #include <vulkan/vulkan.h>
#define VK_NO_PROTOTYPES
#include <volk.h>

#ifdef _WIN32
#include <vulkan/vk_enum_string_helper.h>
#endif

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

#ifdef _WIN32
#define VK_CHECK(func)                                                                 \
  {                                                                                    \
    const VkResult result = func;                                                      \
    if (result != VK_SUCCESS) {                                                        \
      std::cerr << "Error calling function " << #func << " at " << __FILE__ << ":"     \
                << __LINE__ << ". Result is " << string_VkResult(result) << std::endl; \
      assert(false);                                                                   \
    }                                                                                  \
  }
#else
#define VK_CHECK(func)                                                             \
  {                                                                                \
    const VkResult result = func;                                                  \
    if (result != VK_SUCCESS) {                                                    \
      std::cerr << "Error calling function " << #func << " at " << __FILE__ << ":" \
                << __LINE__ << ". Result is " << result << std::endl;              \
      assert(false);                                                               \
    }                                                                              \
  }
#endif

#ifdef __ANDROID__
#define TAG "OPENXR_SAMPLE"
#include <android/log.h>
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#else
#define LOGE(format, ...)                 \
  do {                                    \
    fprintf(stderr, format, __VA_ARGS__); \
    fprintf(stderr, "\n");                \
  } while (0)
#define LOGW(format, ...) LOGE(format, __VA_ARGS__)
#define LOGI(format, ...) LOGE(format, __VA_ARGS__)
#define LOGD(format, ...) LOGE(format, __VA_ARGS__)
#endif

namespace VulkanCore {

constexpr glm::vec4 RENDER_COLOR{1.f, 0.f, 0.f, 1.0f};

VkImageViewType imageTypeToImageViewType(VkImageType imageType, VkImageCreateFlags flags,
                                         bool multiview);

uint32_t bytesPerPixel(VkFormat format);

}  // namespace VulkanCore
