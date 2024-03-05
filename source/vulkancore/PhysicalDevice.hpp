#pragma once

#include <list>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "Common.hpp"
#include "Utility.hpp"

namespace VulkanCore {

class PhysicalDevice final {
 public:
  explicit PhysicalDevice(){};
  explicit PhysicalDevice(VkPhysicalDevice device, VkSurfaceKHR surface,
                          const std::vector<std::string>& requestedExtensions,
                          bool printEnumerations = false, bool enableRayTracing = false);

  [[nodiscard]] VkPhysicalDevice vkPhysicalDevice() const;

  [[nodiscard]] const std::vector<std::string>& extensions() const;

  // surface may be VK_NULL_HANDLE, as we may be rendering offscreen
  void reserveQueues(VkQueueFlags requestedQueueTypes, VkSurfaceKHR surface);

  [[nodiscard]] std::vector<std::pair<uint32_t, uint32_t>> queueFamilyIndexAndCount()
      const;

  [[nodiscard]] std::optional<uint32_t> graphicsFamilyIndex() const;
  [[nodiscard]] std::optional<uint32_t> computeFamilyIndex() const;
  [[nodiscard]] std::optional<uint32_t> transferFamilyIndex() const;
  [[nodiscard]] std::optional<uint32_t> sparseFamilyIndex() const;
  [[nodiscard]] std::optional<uint32_t> presentationFamilyIndex() const;

  [[nodiscard]] uint32_t graphicsFamilyCount() const { return graphicsQueueCount_; }
  [[nodiscard]] uint32_t computeFamilyCount() const { return computeQueueCount_; }
  [[nodiscard]] uint32_t transferFamilyCount() const { return transferQueueCount_; }
  [[nodiscard]] uint32_t sparseFamilyCount() const { return sparseQueueCount_; }
  [[nodiscard]] uint32_t presentationFamilyCount() const {
    return presentationQueueCount_;
  }

  const VkSurfaceCapabilitiesKHR& surfaceCapabilities() const;

  const VkPhysicalDeviceFeatures2& features() const { return features_; }

  const VkPhysicalDeviceProperties2& properties() const { return properties_; }

  const std::unordered_set<std::string>& enabledExtensions() const {
    return enabledExtensions_;
  }

  bool isRayTracingSupported() const {
    return (accelStructFeature_.accelerationStructure &&
            rayTracingFeature_.rayTracingPipeline && rayQueryFeature_.rayQuery);
  }

  VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties() const {
    return rayTracingPipelineProperties_;
  }

  const VkPhysicalDeviceFragmentDensityMapPropertiesEXT& fragmentDensityMapProperties()
      const {
    return fragmentDensityMapProperties_;
  }

  const VkPhysicalDeviceFragmentDensityMapOffsetPropertiesQCOM&
  fragmentDensityMapOffsetProperties() const {
    return fragmentDensityMapOffsetProperties_;
  }

  const std::vector<VkPresentModeKHR>& presentModes() const { return presentModes_; }

  bool isMultiviewSupported() const { return multiviewFeature_.multiview; }

  bool isFragmentDensityMapSupported() const {
    return fragmentDensityMapFeature_.fragmentDensityMap == VK_TRUE;
  }

  bool isFragmentDensityMapOffsetSupported() const {
    return fragmentDensityMapOffsetFeature_.fragmentDensityMapOffset == VK_TRUE;
  }

 private:
  void enumerateSurfaceFormats(VkSurfaceKHR surface);
  void enumerateSurfaceCapabilities(VkSurfaceKHR surface);
  void enumeratePresentationModes(VkSurfaceKHR surface);

 private:
  VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
  std::vector<std::string> extensions_;

  VkPhysicalDeviceFragmentDensityMapOffsetPropertiesQCOM
      fragmentDensityMapOffsetProperties_{
          .sType =
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_PROPERTIES_QCOM,
          .pNext = nullptr,
      };

  VkPhysicalDeviceFragmentDensityMapPropertiesEXT fragmentDensityMapProperties_{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT,
      .pNext = &fragmentDensityMapOffsetProperties_,
  };

  VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties_{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
      .pNext = &fragmentDensityMapProperties_,
  };

  VkPhysicalDeviceProperties2 properties_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
      .pNext = &rayTracingPipelineProperties_,
  };

  // Features
  VkPhysicalDeviceFragmentDensityMapOffsetFeaturesQCOM fragmentDensityMapOffsetFeature_ =
      {
          .sType =
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_FEATURES_QCOM,
          .pNext = nullptr,
  };

  VkPhysicalDeviceFragmentDensityMapFeaturesEXT fragmentDensityMapFeature_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT,
      .pNext = &fragmentDensityMapOffsetFeature_,
  };

  VkPhysicalDeviceMultiviewFeatures multiviewFeature_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
      .pNext = &fragmentDensityMapFeature_,
  };

  VkPhysicalDeviceTimelineSemaphoreFeatures timelineSemaphoreFeature_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,
      .pNext = &multiviewFeature_,
  };

  VkPhysicalDeviceMeshShaderFeaturesNV meshShaderFeature_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV,
      .pNext = (void*)&timelineSemaphoreFeature_,
  };

  VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeature_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
      .pNext = (void*)&meshShaderFeature_,
  };

  VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeature_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
      .pNext = (void*)&rayQueryFeature_,
  };

  VkPhysicalDeviceAccelerationStructureFeaturesKHR accelStructFeature_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
      .pNext = (void*)&rayTracingFeature_,
  };

  VkPhysicalDeviceDescriptorIndexingFeatures descIndexFeature_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
      .pNext = (void*)&accelStructFeature_,
  };

  VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
      .pNext = (void*)&descIndexFeature_,
  };

  VkPhysicalDeviceVulkan12Features features12_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .pNext = (void*)&bufferDeviceAddressFeatures_,
  };

  VkPhysicalDeviceFeatures2 features_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      .pNext = (void*)&features12_,
  };
  VkPhysicalDeviceMemoryProperties2 memoryProperties_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
  };
  std::optional<uint32_t> graphicsFamilyIndex_;
  uint32_t graphicsQueueCount_ = 0;
  std::optional<uint32_t> computeFamilyIndex_;
  uint32_t computeQueueCount_ = 0;
  std::optional<uint32_t> transferFamilyIndex_;
  uint32_t transferQueueCount_ = 0;
  std::optional<uint32_t> sparseFamilyIndex_;
  uint32_t sparseQueueCount_ = 0;
  std::optional<uint32_t> presentationFamilyIndex_;
  uint32_t presentationQueueCount_ = 0;
  std::vector<VkQueueFamilyProperties> queueFamilyProperties_;

  // Swapchain support
  std::vector<VkSurfaceFormatKHR> surfaceFormats_;
  VkSurfaceCapabilitiesKHR surfaceCapabilities_;
  std::vector<VkPresentModeKHR> presentModes_;
  std::unordered_set<std::string> enabledExtensions_;
};  // namespace VulkanCore

}  // namespace VulkanCore
