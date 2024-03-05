#define VOLK_IMPLEMENTATION
#include "Context.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <map>

#include "Framebuffer.hpp"
#include "RenderPass.hpp"
#include "Sampler.hpp"
#include "Texture.hpp"

constexpr bool DEBUG_SHADER_PRINTF_CALLBACK = false;

namespace {
#if defined(VK_EXT_debug_utils)
VkBool32 VKAPI_PTR debugMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
  if (messageSeverity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) {
    LOGE("debugMessengerCallback : MessageCode is %s & Message is %s",
         pCallbackData->pMessageIdName, pCallbackData->pMessage);
#if defined(_WIN32)
    __debugbreak();
#else
    raise(SIGTRAP);
#endif
  } else if (messageSeverity & (~VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) {
    LOGW("debugMessengerCallback : MessageCode is %s & Message is %s",
         pCallbackData->pMessageIdName, pCallbackData->pMessage);
  } else {
    LOGI("debugMessengerCallback : MessageCode is %s & Message is %s",
         pCallbackData->pMessageIdName, pCallbackData->pMessage);
  }

  return VK_FALSE;
}
#endif
}  // namespace

namespace VulkanCore {

VkPhysicalDeviceFeatures Context::physicalDeviceFeatures_ = {
    .independentBlend = VK_TRUE,
    .vertexPipelineStoresAndAtomics = VK_TRUE,
    .fragmentStoresAndAtomics = VK_TRUE,
};

VkPhysicalDeviceVulkan11Features Context::enable11Features_ = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
};

VkPhysicalDeviceVulkan12Features Context::enable12Features_ = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
};

VkPhysicalDeviceVulkan13Features Context::enable13Features_ = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
};

VkPhysicalDeviceAccelerationStructureFeaturesKHR Context::accelStructFeatures_ = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
};

VkPhysicalDeviceRayTracingPipelineFeaturesKHR Context::rayTracingPipelineFeatures_ = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
};

VkPhysicalDeviceRayQueryFeaturesKHR Context::rayQueryFeatures_ = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
};

VkPhysicalDeviceMultiviewFeatures Context::multiviewFeatures_ = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
};

VkPhysicalDeviceFragmentDensityMapFeaturesEXT Context::fragmentDensityMapFeatures_ = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT,
};

VkPhysicalDeviceFragmentDensityMapOffsetFeaturesQCOM
    Context::fragmentDensityMapOffsetFeatures_ = {
        .sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_FEATURES_QCOM,
};

bool Context::enableMultiViewFlag_ = false;

Context::Context(void* window, const std::vector<std::string>& requestedLayers,
                 const std::vector<std::string>& requestedInstanceExtensions,
                 const std::vector<std::string>& requestedDeviceExtensions,
                 VkQueueFlags requestedQueueTypes, bool printEnumerations,
                 bool enableRayTracing, const std::string& name)
    : printEnumerations_{printEnumerations} {
  VK_CHECK(volkInitialize());

  enabledLayers_ = util::filterExtensions(enumerateInstanceLayers(), requestedLayers);
  enabledInstanceExtensions_ =
      util::filterExtensions(enumerateInstanceExtensions(), requestedInstanceExtensions);

  // Transform list of enabled Instance layers from std::string to const char*
  std::vector<const char*> instanceLayers(enabledLayers_.size());
  std::transform(enabledLayers_.begin(), enabledLayers_.end(), instanceLayers.begin(),
                 std::mem_fn(&std::string::c_str));

  {
    // Transform list of enabled extensions from std::string to const char*
    std::vector<const char*> instanceExtensions(enabledInstanceExtensions_.size());
    std::transform(enabledInstanceExtensions_.begin(), enabledInstanceExtensions_.end(),
                   instanceExtensions.begin(), std::mem_fn(&std::string::c_str));

    // Create the instance
    std::vector<VkValidationFeatureEnableEXT> validationFeaturesEnabled;
#if defined(VK_EXT_layer_settings)
    if constexpr (!DEBUG_SHADER_PRINTF_CALLBACK) {
      validationFeaturesEnabled.push_back(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT);
    } else {
      validationFeaturesEnabled.push_back(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT);
    }
#endif

#if defined(VK_EXT_layer_settings)
    const std::string layer_name = "VK_LAYER_KHRONOS_validation";
    const std::array<const char*, 1> setting_debug_action = {"VK_DBG_LAYER_ACTION_BREAK"};
    const std::array<const char*, 1> setting_gpu_based_action = {
        "GPU_BASED_DEBUG_PRINTF"};
    const std::array<VkBool32, 1> setting_printf_to_stdout = {VK_TRUE};
    const std::array<VkBool32, 1> setting_printf_verbose = {VK_TRUE};
    const std::array<VkLayerSettingEXT, 4> settings = {
        VkLayerSettingEXT{
            .pLayerName = layer_name.c_str(),
            .pSettingName = "debug_action",
            .type = VK_LAYER_SETTING_TYPE_STRING_EXT,
            .valueCount = 1,
            .pValues = setting_debug_action.data(),
        },
        VkLayerSettingEXT{
            .pLayerName = layer_name.c_str(),
            .pSettingName = "validate_gpu_based",
            .type = VK_LAYER_SETTING_TYPE_STRING_EXT,
            .valueCount = 1,
            .pValues = setting_gpu_based_action.data(),
        },
        VkLayerSettingEXT{
            .pLayerName = layer_name.c_str(),
            .pSettingName = "printf_to_stdout",
            .type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
            .valueCount = 1,
            .pValues = setting_printf_to_stdout.data(),
        },
        VkLayerSettingEXT{
            .pLayerName = layer_name.c_str(),
            .pSettingName = "printf_verbose",
            .type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
            .valueCount = 1,
            .pValues = setting_printf_verbose.data(),
        },
    };

    const VkLayerSettingsCreateInfoEXT layer_settings_create_info = {
        .sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT,
        .pNext = nullptr,
        .settingCount = static_cast<uint32_t>(settings.size()),
        .pSettings = settings.data(),
    };
#endif

    const VkValidationFeaturesEXT features = {
      .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
#if defined(VK_EXT_layer_settings)
      .pNext = DEBUG_SHADER_PRINTF_CALLBACK ? &layer_settings_create_info : nullptr,
#endif
      .enabledValidationFeatureCount =
          static_cast<uint32_t>(validationFeaturesEnabled.size()),
      .pEnabledValidationFeatures = validationFeaturesEnabled.data(),
    };

    const VkInstanceCreateInfo instanceInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = &features,
        .pApplicationInfo = &applicationInfo_,
        .enabledLayerCount = static_cast<uint32_t>(instanceLayers.size()),
        .ppEnabledLayerNames = instanceLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()),
        .ppEnabledExtensionNames = instanceExtensions.data(),
    };
    VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &instance_));
  }

  // Initialize volk for this instance
  volkLoadInstance(instance_);

#if defined(VK_EXT_debug_utils)
  if (enabledInstanceExtensions_.contains(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
    const VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .flags = 0,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
#if defined(VK_EXT_device_address_binding_report)
                     | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT
#endif
      ,
      .pfnUserCallback = &debugMessengerCallback,
      .pUserData = nullptr,
    };
    VK_CHECK(
        vkCreateDebugUtilsMessengerEXT(instance_, &messengerInfo, nullptr, &messenger_));
  }
#endif

// Surface
#if defined(VK_USE_PLATFORM_WIN32_KHR) && defined(VK_KHR_win32_surface)
  if (enabledInstanceExtensions_.contains(VK_KHR_WIN32_SURFACE_EXTENSION_NAME)) {
    if (window != nullptr) {
      const VkWin32SurfaceCreateInfoKHR ci = {
          .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
          .hinstance = GetModuleHandle(NULL),
          .hwnd = (HWND)window,
      };
      VK_CHECK(vkCreateWin32SurfaceKHR(instance_, &ci, nullptr, &surface_));
    }
  }
#endif

  // Find all physical devices in the system and choose one
  physicalDevice_ = choosePhysicalDevice(
      enumeratePhysicalDevices(requestedDeviceExtensions, enableRayTracing),
      requestedDeviceExtensions);

  // Always request a graphics queue
  physicalDevice_.reserveQueues(requestedQueueTypes | VK_QUEUE_GRAPHICS_BIT, surface_);

  // Create the device
  {
    // Transform list of enabled extensions from std::string to const char *
    std::vector<const char*> deviceExtensions(physicalDevice_.enabledExtensions().size());
    std::transform(physicalDevice_.enabledExtensions().begin(),
                   physicalDevice_.enabledExtensions().end(), deviceExtensions.begin(),
                   std::mem_fn(&std::string::c_str));

    const auto familyIndices = physicalDevice_.queueFamilyIndexAndCount();

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    std::vector<std::vector<float>> prioritiesForAllFamilies(familyIndices.size());
    for (size_t index = 0; const auto& [queueFamilyIndex, queueCount] : familyIndices) {
      prioritiesForAllFamilies[index] = std::vector<float>(queueCount, 1.0f);
      queueCreateInfos.emplace_back(VkDeviceQueueCreateInfo{
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex = queueFamilyIndex,
          .queueCount = queueCount,
          .pQueuePriorities = prioritiesForAllFamilies[index].data(),
      });
      ++index;
    }

    const VkPhysicalDeviceFeatures2 deviceFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .features = physicalDeviceFeatures_,
    };

    VulkanFeatureChain<> featureChain;

    featureChain.pushBack(deviceFeatures);

    featureChain.pushBack(enable11Features_);
    featureChain.pushBack(enable12Features_);
    featureChain.pushBack(enable13Features_);

    if (physicalDevice_.isRayTracingSupported()) {
      featureChain.pushBack(accelStructFeatures_);
      featureChain.pushBack(rayTracingPipelineFeatures_);
      featureChain.pushBack(rayQueryFeatures_);
    }

    if (physicalDevice_.isMultiviewSupported() && enableMultiViewFlag_) {
      enable11Features_.multiview = VK_TRUE;
    }

    if (physicalDevice_.isFragmentDensityMapSupported()) {
      featureChain.pushBack(fragmentDensityMapFeatures_);
    }

    if (physicalDevice_.isFragmentDensityMapOffsetSupported()) {
      featureChain.pushBack(fragmentDensityMapOffsetFeatures_);
    }

    const VkDeviceCreateInfo dci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = featureChain.firstNextPtr(),
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = static_cast<uint32_t>(instanceLayers.size()),
        .ppEnabledLayerNames = instanceLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
    };
    VK_CHECK(vkCreateDevice(physicalDevice_.vkPhysicalDevice(), &dci, nullptr, &device_));
    setVkObjectname(device_, VK_OBJECT_TYPE_DEVICE, "Device");
  }

  if (physicalDevice_.graphicsFamilyIndex().has_value()) {
    if (physicalDevice_.graphicsFamilyCount() > 0) {
      graphicsQueues_.resize(physicalDevice_.graphicsFamilyCount(), VK_NULL_HANDLE);

      for (int i = 0; i < graphicsQueues_.size(); ++i) {
        vkGetDeviceQueue(device_, physicalDevice_.graphicsFamilyIndex().value(),
                         uint32_t(i), &graphicsQueues_[i]);
      }
    }
  }
  if (physicalDevice_.computeFamilyIndex().has_value()) {
    if (physicalDevice_.computeFamilyCount() > 0) {
      computeQueues_.resize(physicalDevice_.computeFamilyCount(), VK_NULL_HANDLE);

      for (int i = 0; i < computeQueues_.size(); ++i) {
        vkGetDeviceQueue(device_, physicalDevice_.computeFamilyIndex().value(),
                         uint32_t(i), &computeQueues_[i]);
      }
    }
  }
  if (physicalDevice_.transferFamilyIndex().has_value()) {
    if (physicalDevice_.transferFamilyCount() > 0) {
      transferQueues_.resize(physicalDevice_.transferFamilyCount(), VK_NULL_HANDLE);

      for (int i = 0; i < transferQueues_.size(); ++i) {
        vkGetDeviceQueue(device_, physicalDevice_.transferFamilyIndex().value(),
                         uint32_t(i), &transferQueues_[i]);
      }
    }
  }
  if (physicalDevice_.sparseFamilyIndex().has_value()) {
    if (physicalDevice_.sparseFamilyCount() > 0) {
      sparseQueues_.resize(physicalDevice_.sparseFamilyCount(), VK_NULL_HANDLE);

      for (int i = 0; i < sparseQueues_.size(); ++i) {
        vkGetDeviceQueue(device_, physicalDevice_.sparseFamilyIndex().value(),
                         uint32_t(i), &sparseQueues_[i]);
      }
    }
  }
  if (physicalDevice_.presentationFamilyIndex().has_value()) {
    vkGetDeviceQueue(device_, physicalDevice_.presentationFamilyIndex().value(), 0,
                     &presentationQueue_);
  }

  // Initialize volk for this device
  volkLoadDevice(device_);

  // Create the allocator
  createMemoryAllocator();

  // Naming objects created before we had a device
  setVkObjectname(surface_, VK_OBJECT_TYPE_SURFACE_KHR, "Surface: " + name);
}  // namespace VulkanCore

Context::Context(const VkApplicationInfo& appInfo,
                 const std::vector<std::string>& requestedLayers,
                 const std::vector<std::string>& requestedInstanceExtensions,
                 bool printEnumerations, const std::string& name)
    : applicationInfo_{appInfo}, printEnumerations_{printEnumerations} {
  VK_CHECK(volkInitialize());

  enabledLayers_ = util::filterExtensions(enumerateInstanceLayers(), requestedLayers);
  enabledInstanceExtensions_ =
      util::filterExtensions(enumerateInstanceExtensions(), requestedInstanceExtensions);

  // Transform list of enabled Instance layers from std::string to const char*
  std::vector<const char*> instanceLayers(enabledLayers_.size());
  std::transform(enabledLayers_.begin(), enabledLayers_.end(), instanceLayers.begin(),
                 std::mem_fn(&std::string::c_str));

  {
    // Transform list of enabled extensions from std::string to const char*
    std::vector<const char*> instanceExtensions(enabledInstanceExtensions_.size());
    std::transform(enabledInstanceExtensions_.begin(), enabledInstanceExtensions_.end(),
                   instanceExtensions.begin(), std::mem_fn(&std::string::c_str));

    // Create the instance
    const VkValidationFeatureEnableEXT validationFeaturesEnabled[] = {
        VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
        VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
    };  // VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT};

    const VkValidationFeaturesEXT features = {
        .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
        .pNext = nullptr,
        .enabledValidationFeatureCount =
            sizeof(validationFeaturesEnabled) / sizeof(VkValidationFeatureEnableEXT),
        .pEnabledValidationFeatures = validationFeaturesEnabled,
    };

    const VkInstanceCreateInfo instanceInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = &features,
        .pApplicationInfo = &applicationInfo_,
        .enabledLayerCount = static_cast<uint32_t>(instanceLayers.size()),
        .ppEnabledLayerNames = instanceLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()),
        .ppEnabledExtensionNames = instanceExtensions.data(),
    };
    VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &instance_));

    ASSERT(instance_ != VK_NULL_HANDLE, "Error creating VkInstance");
  }

  // Initialize volk for this instance
  volkLoadInstance(instance_);

#if defined(VK_EXT_debug_utils)
  if (enabledInstanceExtensions_.contains(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
    const VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .flags = 0,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
#if defined(VK_EXT_device_address_binding_report)
                     | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT
#endif
      ,
      .pfnUserCallback = &debugMessengerCallback,
      .pUserData = nullptr,
    };
    VK_CHECK(
        vkCreateDebugUtilsMessengerEXT(instance_, &messengerInfo, nullptr, &messenger_));
  }
#endif

  if (device_) {
    setVkObjectname(instance_, VK_OBJECT_TYPE_INSTANCE, "Instance: " + name);
  }
}

void Context::createVkDevice(VkPhysicalDevice vkPhysicalDevice,
                             const std::vector<std::string>& requestedDeviceExtensions,
                             VkQueueFlags requestedQueueTypes, const std::string& name) {
  physicalDevice_ = PhysicalDevice(vkPhysicalDevice, VK_NULL_HANDLE,
                                   requestedDeviceExtensions, printEnumerations_, false);

  // Always request a graphics queue
  physicalDevice_.reserveQueues(requestedQueueTypes | VK_QUEUE_GRAPHICS_BIT,
                                VK_NULL_HANDLE);

  // Create the device
  {
    // Transform list of enabled extensions from std::string to const char *
    std::vector<const char*> deviceExtensions(physicalDevice_.enabledExtensions().size());
    std::transform(physicalDevice_.enabledExtensions().begin(),
                   physicalDevice_.enabledExtensions().end(), deviceExtensions.begin(),
                   std::mem_fn(&std::string::c_str));

    const auto familyIndices = physicalDevice_.queueFamilyIndexAndCount();

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    std::vector<std::vector<float>> prioritiesForAllFamilies(familyIndices.size());
    for (size_t index = 0; const auto& [queueFamilyIndex, queueCount] : familyIndices) {
      prioritiesForAllFamilies[index] = std::vector<float>(queueCount, 1.0f);
      queueCreateInfos.emplace_back(VkDeviceQueueCreateInfo{
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex = queueFamilyIndex,
          .queueCount = queueCount,
          .pQueuePriorities = prioritiesForAllFamilies[index].data(),
      });
      ++index;
    }

    VkPhysicalDeviceFeatures2 deviceFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .features = physicalDeviceFeatures_,
    };

    VulkanFeatureChain<> featureChain;

    featureChain.pushBack(deviceFeatures);

    featureChain.pushBack(enable11Features_);
    featureChain.pushBack(enable12Features_);
#if defined(_WIN32)
    featureChain.pushBack(enable13Features_);
#else
    // this should be only done if we are on vulkan 1.1 & buffer address is
    // enabled
    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddrFeature = {};
    bufferDeviceAddrFeature.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bufferDeviceAddrFeature.bufferDeviceAddress = VK_TRUE;
    bufferDeviceAddrFeature.bufferDeviceAddressCaptureReplay = VK_TRUE;
    deviceFeatures.pNext = &bufferDeviceAddrFeature;
#endif

    if (physicalDevice_.isRayTracingSupported()) {
      featureChain.pushBack(accelStructFeatures_);
      featureChain.pushBack(rayTracingPipelineFeatures_);
      featureChain.pushBack(rayQueryFeatures_);
    }

    if (physicalDevice_.isMultiviewSupported() && enableMultiViewFlag_) {
      enable11Features_.multiview = VK_TRUE;
    }

    if (physicalDevice_.isFragmentDensityMapSupported()) {
      featureChain.pushBack(fragmentDensityMapFeatures_);
    }

    if (physicalDevice_.isFragmentDensityMapOffsetSupported()) {
      featureChain.pushBack(fragmentDensityMapOffsetFeatures_);
    }

    std::vector<const char*> instanceLayers(enabledLayers_.size());
    std::transform(enabledLayers_.begin(), enabledLayers_.end(), instanceLayers.begin(),
                   std::mem_fn(&std::string::c_str));

    const VkDeviceCreateInfo dci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = featureChain.firstNextPtr(),
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = static_cast<uint32_t>(instanceLayers.size()),
        .ppEnabledLayerNames = instanceLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
    };
    VK_CHECK(vkCreateDevice(physicalDevice_.vkPhysicalDevice(), &dci, nullptr, &device_));
    setVkObjectname(device_, VK_OBJECT_TYPE_DEVICE, "Device");
  }

  if (physicalDevice_.graphicsFamilyIndex().has_value()) {
    if (physicalDevice_.graphicsFamilyCount() > 0) {
      graphicsQueues_.resize(physicalDevice_.graphicsFamilyCount(), VK_NULL_HANDLE);

      for (int i = 0; i < graphicsQueues_.size(); ++i) {
        vkGetDeviceQueue(device_, physicalDevice_.graphicsFamilyIndex().value(),
                         uint32_t(i), &graphicsQueues_[i]);
      }
    }
  }
  if (physicalDevice_.computeFamilyIndex().has_value()) {
    if (physicalDevice_.computeFamilyCount() > 0) {
      computeQueues_.resize(physicalDevice_.computeFamilyCount(), VK_NULL_HANDLE);

      for (int i = 0; i < computeQueues_.size(); ++i) {
        vkGetDeviceQueue(device_, physicalDevice_.computeFamilyIndex().value(),
                         uint32_t(i), &computeQueues_[i]);
      }
    }
  }
  if (physicalDevice_.transferFamilyIndex().has_value()) {
    if (physicalDevice_.transferFamilyCount() > 0) {
      transferQueues_.resize(physicalDevice_.transferFamilyCount(), VK_NULL_HANDLE);

      for (int i = 0; i < transferQueues_.size(); ++i) {
        vkGetDeviceQueue(device_, physicalDevice_.transferFamilyIndex().value(),
                         uint32_t(i), &transferQueues_[i]);
      }
    }
  }
  if (physicalDevice_.sparseFamilyIndex().has_value()) {
    if (physicalDevice_.sparseFamilyCount() > 0) {
      sparseQueues_.resize(physicalDevice_.sparseFamilyCount(), VK_NULL_HANDLE);

      for (int i = 0; i < sparseQueues_.size(); ++i) {
        vkGetDeviceQueue(device_, physicalDevice_.sparseFamilyIndex().value(),
                         uint32_t(i), &sparseQueues_[i]);
      }
    }
  }
  if (physicalDevice_.presentationFamilyIndex().has_value()) {
    vkGetDeviceQueue(device_, physicalDevice_.presentationFamilyIndex().value(), 0,
                     &presentationQueue_);
  }

  // Initialize volk for this device
  volkLoadDevice(device_);

  // Create the allocator
  createMemoryAllocator();

  setVkObjectname(device_, VK_OBJECT_TYPE_DEVICE, "Device: " + name);

  setVkObjectname(instance_, VK_OBJECT_TYPE_INSTANCE, "Instance: " + name);
}

Context::~Context() {
  vkDeviceWaitIdle(device_);

  swapchain_.reset();
  vmaDestroyAllocator(allocator_);
  vkDestroyDevice(device_, nullptr);
  if (surface_ != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
  }
#if defined(VK_EXT_debug_utils)
  if (enabledInstanceExtensions_.contains(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
    vkDestroyDebugUtilsMessengerEXT(instance_, messenger_, nullptr);
  }
#endif

  vkDestroyInstance(instance_, nullptr);
}

void Context::enableDefaultFeatures() {
  // do we need these for defaults?
  enable12Features_.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
  enable12Features_.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
  // enable12Features_.descriptorBindingUniformBufferUpdateAfterBind =
  //     VK_TRUE,  // This
  //               // makes creating a device on a 1060 fail
  enable12Features_.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
  enable12Features_.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
  enable12Features_.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
  enable12Features_.descriptorBindingPartiallyBound = VK_TRUE;
  enable12Features_.descriptorBindingVariableDescriptorCount = VK_TRUE;
  enable12Features_.descriptorIndexing = VK_TRUE;
  enable12Features_.runtimeDescriptorArray = VK_TRUE;
}

void Context::enableScalarLayoutFeatures() {
  enable12Features_.scalarBlockLayout = VK_TRUE;
}

void Context::enableDynamicRenderingFeature() {
  enable13Features_.dynamicRendering = VK_TRUE;
}

void Context::enableBufferDeviceAddressFeature() {
  enable12Features_.bufferDeviceAddress = VK_TRUE;
  enable12Features_.bufferDeviceAddressCaptureReplay = VK_TRUE;
}

void Context::enableIndirectRenderingFeature() {
  enable11Features_.shaderDrawParameters = VK_TRUE;
  enable12Features_.drawIndirectCount = VK_TRUE;
  physicalDeviceFeatures_.multiDrawIndirect = VK_TRUE;
  physicalDeviceFeatures_.drawIndirectFirstInstance = VK_TRUE;
}

void Context::enable16bitFloatFeature() {
  enable11Features_.storageBuffer16BitAccess = VK_TRUE;
  enable12Features_.shaderFloat16 = VK_TRUE;
}

void Context::enableIndependentBlending() {
  physicalDeviceFeatures_.independentBlend = VK_TRUE;
}

void Context::enableMaintenance4Feature() { enable13Features_.maintenance4 = VK_TRUE; }

void Context::enableSynchronization2Feature() {
  enable13Features_.synchronization2 = VK_TRUE;
}

void Context::enableRayTracingFeatures() {
  accelStructFeatures_.accelerationStructure = VK_TRUE;
  rayTracingPipelineFeatures_.rayTracingPipeline = VK_TRUE;
  rayQueryFeatures_.rayQuery = VK_TRUE;
}

void Context::enableMultiView() { enableMultiViewFlag_ = true; }

bool Context::isMultiviewEnabled() { return enableMultiViewFlag_; }

void Context::enableFragmentDensityMapFeatures() {
  fragmentDensityMapFeatures_.fragmentDensityMap = VK_TRUE;
}

void Context::enableFragmentDensityMapOffsetFeatures() {
  fragmentDensityMapOffsetFeatures_.fragmentDensityMapOffset = VK_TRUE;
}

const PhysicalDevice& Context::physicalDevice() const { return physicalDevice_; }

void Context::createSwapchain(VkFormat format, VkColorSpaceKHR colorSpace,
                              VkPresentModeKHR presentMode, const VkExtent2D& extent) {
  ASSERT(surface_ != VK_NULL_HANDLE,
         "You are trying to create a swapchain without a surface. The Context "
         "must be provided a valid surface for it to be able to create a "
         "swapchain");

  swapchain_ =
      std::make_unique<Swapchain>(*this, physicalDevice_, surface_, presentationQueue_,
                                  format, colorSpace, presentMode, extent);
}

Swapchain* Context::swapchain() const { return swapchain_.get(); }

std::shared_ptr<Buffer> Context::createBuffer(size_t size, VkBufferUsageFlags flags,
                                              VmaMemoryUsage memoryUsage,
                                              const std::string& name) const {
  return std::make_shared<Buffer>(
      this, memoryAllocator(),
      VkBufferCreateInfo{
          .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          .size = size,
          .usage = flags,
      },
      VmaAllocationCreateInfo{
          .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
          .usage = memoryUsage,
          .preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
      },
      name);
}

std::shared_ptr<Buffer> Context::createPersistentBuffer(size_t size,
                                                        VkBufferUsageFlags flags,
                                                        const std::string& name) const {
  const VkMemoryPropertyFlags cpuVisibleMemoryFlags =
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
      VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
  return std::make_shared<Buffer>(this, memoryAllocator(),
                                  VkBufferCreateInfo{
                                      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                      .size = size,
                                      .usage = flags,
                                      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                  },
                                  VmaAllocationCreateInfo{
                                      .flags = 0,
                                      .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                                      .requiredFlags = cpuVisibleMemoryFlags,
                                  },
                                  name);
}

std::shared_ptr<Buffer> Context::createStagingBuffer(VkDeviceSize size,
                                                     VkBufferUsageFlags flags,
                                                     const std::string& name) const {
  const VkBufferCreateInfo stagingBufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = size,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
  };

  const VmaAllocationCreateInfo stagingAllocationCreateInfo = {
      .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
               VMA_ALLOCATION_CREATE_MAPPED_BIT,
      .usage = VMA_MEMORY_USAGE_CPU_ONLY,
  };

  return std::make_shared<Buffer>(this, memoryAllocator(), stagingBufferCreateInfo,
                                  stagingAllocationCreateInfo, name);
}

std::shared_ptr<Buffer> Context::createStagingBuffer(VkDeviceSize size,
                                                     VkBufferUsageFlags usage,
                                                     Buffer* actualBuffer,
                                                     const std::string& name) const {
  return std::make_shared<Buffer>(this, memoryAllocator(), size, usage, actualBuffer,
                                  name);
}

// creates staging buffer & upload data to gpubuffer
void Context::uploadToGPUBuffer(VulkanCore::CommandQueueManager& queueMgr,
                                VkCommandBuffer commandBuffer,
                                VulkanCore::Buffer* gpuBuffer, const void* data,
                                long totalSize, uint64_t gpuBufferOffset) const {
  auto stagingBuffer = createStagingBuffer(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                           gpuBuffer, "staging buffer");
  stagingBuffer->copyDataToBuffer(data, totalSize);

  stagingBuffer->uploadStagingBufferToGPU(commandBuffer, 0, gpuBufferOffset);
  queueMgr.disposeWhenSubmitCompletes(std::move(stagingBuffer));
}

std::shared_ptr<Texture> Context::createTexture(
    VkImageType type, VkFormat format, VkImageCreateFlags flags,
    VkImageUsageFlags usageFlags, VkExtent3D extents, uint32_t numMipLevels,
    uint32_t layerCount, VkMemoryPropertyFlags memoryFlags, bool generateMips,
    VkSampleCountFlagBits msaaSamples, const std::string& name) const {
  return std::make_shared<Texture>(*this, type, format, flags, usageFlags, extents,
                                   numMipLevels, layerCount, memoryFlags, generateMips,
                                   msaaSamples, name);
}

std::shared_ptr<Sampler> Context::createSampler(VkFilter minFilter, VkFilter magFilter,
                                                VkSamplerAddressMode addressModeU,
                                                VkSamplerAddressMode addressModeV,
                                                VkSamplerAddressMode addressModeW,
                                                float maxLod,
                                                const std::string& name) const {
  return std::make_shared<Sampler>(*this, minFilter, magFilter, addressModeU,
                                   addressModeV, addressModeW, maxLod, name);
}

std::shared_ptr<VulkanCore::Sampler> Context::createSampler(
    VkFilter minFilter, VkFilter magFilter, VkSamplerAddressMode addressModeU,
    VkSamplerAddressMode addressModeV, VkSamplerAddressMode addressModeW, float maxLod,
    bool compareEnable, VkCompareOp compareOp, const std::string& name /*= ""*/) const {
  return std::make_shared<Sampler>(*this, minFilter, magFilter, addressModeU,
                                   addressModeV, addressModeW, maxLod, compareEnable,
                                   compareOp, name);
}

CommandQueueManager Context::createGraphicsCommandQueue(uint32_t count,
                                                        uint32_t concurrentNumCommands,
                                                        const std::string& name,
                                                        int graphicsQueueIndex) {
  if (graphicsQueueIndex != -1) {
    ASSERT(graphicsQueueIndex < graphicsQueues_.size(),
           "Don't have enough graphics queue, specify smaller queue index");
  }
  return CommandQueueManager(
      *this, device_, count, concurrentNumCommands,
      physicalDevice_.graphicsFamilyIndex().value(),
      graphicsQueueIndex != -1 ? graphicsQueues_[graphicsQueueIndex] : graphicsQueues_[0],
      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, name);
}

VulkanCore::CommandQueueManager Context::createTransferCommandQueue(
    uint32_t count, uint32_t concurrentNumCommands, const std::string& name,
    int transferQueueIndex) {
  if (transferQueueIndex != -1) {
    ASSERT(transferQueueIndex < transferQueues_.size(),
           "Don't have enough transfer queue, specify smaller queue index");
  }
  return CommandQueueManager(
      *this, device_, count, concurrentNumCommands,
      physicalDevice_.transferFamilyIndex().value(),
      transferQueueIndex != -1 ? transferQueues_[transferQueueIndex] : transferQueues_[0],
      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, name);
}

std::shared_ptr<ShaderModule> Context::createShaderModule(const std::string& filePath,
                                                          VkShaderStageFlagBits stages,
                                                          const std::string& name) {
  return std::make_shared<ShaderModule>(this, filePath, stages, name);
}

std::shared_ptr<ShaderModule> Context::createShaderModule(const std::string& filePath,
                                                          const std::string& entryPoint,
                                                          VkShaderStageFlagBits stages,
                                                          const std::string& name) {
  return std::make_shared<ShaderModule>(this, filePath, entryPoint, stages, name);
}

std::shared_ptr<ShaderModule> Context::createShaderModule(const std::vector<char>& shader,
                                                          const std::string& entryPoint,
                                                          VkShaderStageFlagBits stages,
                                                          const std::string& name) {
  return std::make_shared<ShaderModule>(this, shader, entryPoint, stages, name);
}

std::shared_ptr<Pipeline> Context::createGraphicsPipeline(
    const Pipeline::GraphicsPipelineDescriptor& desc, VkRenderPass renderPass,
    const std::string& name) {
  return std::make_shared<Pipeline>(this, desc, renderPass, name);
}

std::shared_ptr<VulkanCore::Pipeline> Context::createComputePipeline(
    const Pipeline::ComputePipelineDescriptor& desc, const std::string& name /*= ""*/) {
  return std::make_shared<Pipeline>(this, desc, name);
}

std::shared_ptr<VulkanCore::Pipeline> Context::createRayTracingPipeline(
    const Pipeline::RayTracingPipelineDescriptor& desc,
    const std::string& name /*= ""*/) {
  return std::make_shared<Pipeline>(this, desc, name);
}

std::shared_ptr<RenderPass> Context::createRenderPass(
    const std::vector<std::shared_ptr<Texture>>& attachments,
    const std::vector<VkAttachmentLoadOp>& loadOp,
    const std::vector<VkAttachmentStoreOp>& storeOp,
    const std::vector<VkImageLayout>& layout, VkPipelineBindPoint bindPoint,
    const std::vector<std::shared_ptr<Texture>>& resolveAttachments,
    const std::string& name) const {
  return std::make_shared<RenderPass>(*this, attachments, resolveAttachments, loadOp,
                                      storeOp, layout, bindPoint, name);
}

std::unique_ptr<Framebuffer> Context::createFramebuffer(
    VkRenderPass renderPass,
    const std::vector<std::shared_ptr<Texture>>& colorAttachments,
    std::shared_ptr<Texture> depthAttachment, std::shared_ptr<Texture> stencilAttachment,
    const std::string& name) const {
  return std::make_unique<Framebuffer>(*this, device_, renderPass, colorAttachments,
                                       depthAttachment, stencilAttachment, name);
}

void Context::beginDebugUtilsLabel(VkCommandBuffer commandBuffer, const std::string& name,
                                   const glm::vec4& color) const {
#if defined(VK_EXT_debug_utils) && defined(_WIN32)
  VkDebugUtilsLabelEXT utilsLabelInfo = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
      .pLabelName = name.c_str(),
  };
  memcpy(utilsLabelInfo.color, &color[0], sizeof(float) * 4);
  vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &utilsLabelInfo);
#endif
}

void Context::endDebugUtilsLabel(VkCommandBuffer commandBuffer) const {
#if defined(VK_EXT_debug_utils) && defined(_WIN32)
  vkCmdEndDebugUtilsLabelEXT(commandBuffer);
#endif
}

void Context::createMemoryAllocator() {
  const VmaVulkanFunctions vulkanFunctions = {
    .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
    .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
    .vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
    .vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
    .vkAllocateMemory = vkAllocateMemory,
    .vkFreeMemory = vkFreeMemory,
    .vkMapMemory = vkMapMemory,
    .vkUnmapMemory = vkUnmapMemory,
    .vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
    .vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
    .vkBindBufferMemory = vkBindBufferMemory,
    .vkBindImageMemory = vkBindImageMemory,
    .vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
    .vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
    .vkCreateBuffer = vkCreateBuffer,
    .vkDestroyBuffer = vkDestroyBuffer,
    .vkCreateImage = vkCreateImage,
    .vkDestroyImage = vkDestroyImage,
    .vkCmdCopyBuffer = vkCmdCopyBuffer,
#if VMA_VULKAN_VERSION >= 1001000
    .vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2,
    .vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2,
    .vkBindBufferMemory2KHR = vkBindBufferMemory2,
    .vkBindImageMemory2KHR = vkBindImageMemory2,
    .vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2,
#endif
#if VMA_VULKAN_VERSION >= 1003000
    .vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements,
    .vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements,
#endif
  };

  const VmaAllocatorCreateInfo allocInfo = {
#if defined(VK_KHR_buffer_device_address) && defined(_WIN32)
    .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
#endif
    .physicalDevice = physicalDevice_.vkPhysicalDevice(),
    .device = device_,
    .pVulkanFunctions = &vulkanFunctions,
    .instance = instance_,
    .vulkanApiVersion = applicationInfo_.apiVersion,
  };
  vmaCreateAllocator(&allocInfo, &allocator_);
}

void Context::dumpMemoryStats(const std::string& fileName) const {
  char* memoryStats{nullptr};
  ASSERT(allocator_, "Allocator must be initialized");
  vmaBuildStatsString(allocator_, &memoryStats, true);

  std::ofstream out(fileName);
  out << std::string(memoryStats);
  out.close();

  vmaFreeStatsString(allocator_, memoryStats);
}

std::vector<std::string> Context::enumerateInstanceLayers(bool printEnumerations) {
  uint32_t instanceLayerCount{0};
  VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));
  std::vector<VkLayerProperties> layers(instanceLayerCount);
  VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, layers.data()));

  std::vector<std::string> returnValues;
  std::transform(
      layers.begin(), layers.end(), std::back_inserter(returnValues),
      [](const VkLayerProperties& properties) { return properties.layerName; });

  if (printEnumerations) {
    std::cerr << "Found " << instanceLayerCount << " available layer(s)" << std::endl;
    for (const auto& layer : returnValues) {
      std::cerr << "\t" << layer << std::endl;
    }
  }

  return returnValues;
}

[[nodiscard]] std::vector<std::string> Context::enumerateInstanceExtensions() {
  uint32_t extensionsCount{0};
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount, nullptr);
  std::vector<VkExtensionProperties> extensionProperties(extensionsCount);
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount,
                                         extensionProperties.data());

  std::vector<std::string> returnValues;
  std::transform(
      extensionProperties.begin(), extensionProperties.end(),
      std::back_inserter(returnValues),
      [](const VkExtensionProperties& properties) { return properties.extensionName; });

  if (printEnumerations_) {
    std::cerr << "Found " << extensionsCount << " extension(s) for the instance"
              << std::endl;
    for (const auto& layer : returnValues) {
      std::cerr << "\t" << layer << std::endl;
    }
  }

  return returnValues;
}

std::vector<PhysicalDevice> Context::enumeratePhysicalDevices(
    const std::vector<std::string>& requestedExtensions, bool enableRayTracing) const {
  uint32_t deviceCount{0};
  VK_CHECK(vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr));
  ASSERT(deviceCount > 0, "No Vulkan devices found");
  std::vector<VkPhysicalDevice> devices(deviceCount);
  VK_CHECK(vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data()));

  if (printEnumerations_) {
    std::cerr << "Found " << deviceCount << " Vulkan capable device(s)" << std::endl;
  }

  std::vector<PhysicalDevice> physicalDevices;
  for (const auto device : devices) {
    physicalDevices.emplace_back(PhysicalDevice(device, surface_, requestedExtensions,
                                                printEnumerations_, enableRayTracing));
  }
  return physicalDevices;
}

PhysicalDevice Context::choosePhysicalDevice(
    std::vector<PhysicalDevice>&& devices,
    const std::vector<std::string>& deviceExtensions) const {
  (void)deviceExtensions;
  ASSERT(!devices.empty(), "The list of devices can't be empty");

  for (const auto device : devices) {
    std::string devicename(device.properties().properties.deviceName);
    const auto result = devicename.find("NVIDIA");
    if (result != std::string::npos) {
      return device;
    }
  }
  return devices[0];
}

}  // namespace VulkanCore
