#include "PhysicalDevice.hpp"

#include <algorithm>
#include <iostream>
#include <set>

namespace VulkanCore {

PhysicalDevice::PhysicalDevice(VkPhysicalDevice device, VkSurfaceKHR surface,
                               const std::vector<std::string>& requestedExtensions,
                               bool printEnumerations, bool enableRayTracing)
    : physicalDevice_{device} {
  if (!enableRayTracing) {
    descIndexFeature_.pNext = &meshShaderFeature_;
  }

  // Features
  vkGetPhysicalDeviceFeatures2(physicalDevice_, &features_);

  // Properties
  vkGetPhysicalDeviceProperties2(physicalDevice_, &properties_);

  // Get memory properties
  vkGetPhysicalDeviceMemoryProperties2(physicalDevice_, &memoryProperties_);

  // Enumerate queues
  {
    uint32_t queueFamilyCount{0};
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount, nullptr);
    queueFamilyProperties_.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount,
                                             queueFamilyProperties_.data());
  }

  // Enumerate extensions
  {
    uint32_t propertyCount{0};
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr,
                                                  &propertyCount, nullptr));
    std::vector<VkExtensionProperties> properties(propertyCount);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr,
                                                  &propertyCount, properties.data()));

    std::transform(properties.begin(), properties.end(), std::back_inserter(extensions_),
                   [](const VkExtensionProperties& property) {
                     return std::string(property.extensionName);
                   });

    enabledExtensions_ = util::filterExtensions(extensions_, requestedExtensions);
  }

  if (surface != VK_NULL_HANDLE) {
    enumerateSurfaceFormats(surface);
    enumerateSurfaceCapabilities(surface);
    enumeratePresentationModes(surface);
  }

  // Print device extensions for debugging
  if (printEnumerations) {
    std::cerr << properties_.properties.deviceName << " "
              << properties_.properties.vendorID << " ("
              << properties_.properties.deviceID << ") - ";
    const auto apiVersion = properties_.properties.apiVersion;
    std::cerr << "Vulkan " << VK_API_VERSION_MAJOR(apiVersion) << "."
              << VK_API_VERSION_MINOR(apiVersion) << "."
              << VK_API_VERSION_PATCH(apiVersion) << "."
              << VK_API_VERSION_VARIANT(apiVersion) << ")" << std::endl;

    std::cerr << "Extensions: " << std::endl;
    for (const auto& extension : extensions_) {
      std::cerr << "\t" << extension << std::endl;
    }

    std::cerr << "Supported surface formats: " << std::endl;
    for (const auto format : surfaceFormats_) {
#ifdef _WIN32
      std::cerr << "\t" << string_VkFormat(format.format) << " : "
                << string_VkColorSpaceKHR(format.colorSpace) << std::endl;
#else
      std::cerr << "\t" << format.format << " : " << format.colorSpace << std::endl;
#endif
    }

    std::cerr << "Supported presentation modes: " << std::endl;
    for (const auto mode : presentModes_) {
#ifdef _WIN32
      std::cerr << "\t" << string_VkPresentModeKHR(mode) << std::endl;
#else
      std::cerr << "\t" << mode << std::endl;
#endif
    }
  }
}

VkPhysicalDevice PhysicalDevice::vkPhysicalDevice() const { return physicalDevice_; }

const std::vector<std::string>& PhysicalDevice::extensions() const { return extensions_; }

void PhysicalDevice::reserveQueues(VkQueueFlags requestedQueueTypes,
                                   VkSurfaceKHR surface) {
  ASSERT(requestedQueueTypes > 0, "Requested queue types is empty");

  // We only share queues with presentation, a vulkan queue can support multiple
  // operations (such as graphics, compute, sparse, transfer etc), however in
  // that case that queue can be used only through one thread This code ensures
  // we treat each queue as independent and it can only be used either for
  // graphics/compute/transfer or sparse, this helps us with multithreading,
  // however if the device only have one queue for all, then because of this
  // code we may not be able to create compute/transfer queues
  for (uint32_t queueFamilyIndex = 0;
       queueFamilyIndex < queueFamilyProperties_.size() && requestedQueueTypes != 0;
       ++queueFamilyIndex) {
    if (!presentationFamilyIndex_.has_value() && surface != VK_NULL_HANDLE) {
      VkBool32 supportsPresent{VK_FALSE};
      vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice_, queueFamilyIndex, surface,
                                           &supportsPresent);
      if (supportsPresent == VK_TRUE) {
        presentationFamilyIndex_ = queueFamilyIndex;
        presentationQueueCount_ = queueFamilyProperties_[queueFamilyIndex].queueCount;
      }
    }
    if (!graphicsFamilyIndex_.has_value() &&
        (requestedQueueTypes & queueFamilyProperties_[queueFamilyIndex].queueFlags) &
            VK_QUEUE_GRAPHICS_BIT) {
      graphicsFamilyIndex_ = queueFamilyIndex;
      graphicsQueueCount_ = queueFamilyProperties_[queueFamilyIndex].queueCount;
      requestedQueueTypes &= ~VK_QUEUE_GRAPHICS_BIT;
      continue;
    }

    if (!computeFamilyIndex_.has_value() &&
        (requestedQueueTypes & queueFamilyProperties_[queueFamilyIndex].queueFlags) &
            VK_QUEUE_COMPUTE_BIT) {
      computeFamilyIndex_ = queueFamilyIndex;
      computeQueueCount_ = queueFamilyProperties_[queueFamilyIndex].queueCount;
      requestedQueueTypes &= ~VK_QUEUE_COMPUTE_BIT;
      continue;
    }

    if (!transferFamilyIndex_.has_value() &&
        (requestedQueueTypes & queueFamilyProperties_[queueFamilyIndex].queueFlags) &
            VK_QUEUE_TRANSFER_BIT) {
      transferFamilyIndex_ = queueFamilyIndex;
      transferQueueCount_ = queueFamilyProperties_[queueFamilyIndex].queueCount;
      requestedQueueTypes &= ~VK_QUEUE_TRANSFER_BIT;
      continue;
    }

    if (!sparseFamilyIndex_.has_value() &&
        (requestedQueueTypes & queueFamilyProperties_[queueFamilyIndex].queueFlags) &
            VK_QUEUE_SPARSE_BINDING_BIT) {
      sparseFamilyIndex_ = queueFamilyIndex;
      sparseQueueCount_ = queueFamilyProperties_[queueFamilyIndex].queueCount;
      requestedQueueTypes &= ~VK_QUEUE_SPARSE_BINDING_BIT;
      continue;
    }
  }

  ASSERT(graphicsFamilyIndex_.has_value() || computeFamilyIndex_.has_value() ||
             transferFamilyIndex_.has_value() || sparseFamilyIndex_.has_value(),
         "No suitable queue(s) found");

  ASSERT(surface == VK_NULL_HANDLE || presentationFamilyIndex_.has_value(),
         "No queues with presentation capabilities found");
}

[[nodiscard]] std::vector<std::pair<uint32_t, uint32_t>>
PhysicalDevice::queueFamilyIndexAndCount() const {
  std::set<std::pair<uint32_t, uint32_t>> familyIndices;
  if (graphicsFamilyIndex_.has_value()) {
    familyIndices.insert({graphicsFamilyIndex_.value(), graphicsFamilyCount()});
  }
  if (computeFamilyIndex_.has_value()) {
    familyIndices.insert({computeFamilyIndex_.value(), computeFamilyCount()});
  }
  if (transferFamilyIndex_.has_value()) {
    familyIndices.insert({transferFamilyIndex_.value(), transferFamilyCount()});
  }
  if (sparseFamilyIndex_.has_value()) {
    familyIndices.insert({sparseFamilyIndex_.value(), sparseFamilyCount()});
  }
  if (presentationFamilyIndex_.has_value()) {
    familyIndices.insert({presentationFamilyIndex_.value(), presentationFamilyCount()});
  }
  std::vector<std::pair<uint32_t, uint32_t>> returnValues(familyIndices.begin(),
                                                          familyIndices.end());
  return returnValues;
}

std::optional<uint32_t> PhysicalDevice::graphicsFamilyIndex() const {
  return graphicsFamilyIndex_;
}

std::optional<uint32_t> PhysicalDevice::computeFamilyIndex() const {
  return computeFamilyIndex_;
}

std::optional<uint32_t> PhysicalDevice::transferFamilyIndex() const {
  return transferFamilyIndex_;
}

std::optional<uint32_t> PhysicalDevice::sparseFamilyIndex() const {
  return sparseFamilyIndex_;
}

std::optional<uint32_t> PhysicalDevice::presentationFamilyIndex() const {
  return presentationFamilyIndex_;
}

const VkSurfaceCapabilitiesKHR& PhysicalDevice::surfaceCapabilities() const {
  return surfaceCapabilities_;
}

void PhysicalDevice::enumerateSurfaceFormats(VkSurfaceKHR surface) {
  uint32_t formatCount{0};
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface, &formatCount, nullptr);
  surfaceFormats_.resize(formatCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface, &formatCount,
                                       surfaceFormats_.data());
}

void PhysicalDevice::enumerateSurfaceCapabilities(VkSurfaceKHR surface) {
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface,
                                            &surfaceCapabilities_);
}

void PhysicalDevice::enumeratePresentationModes(VkSurfaceKHR surface) {
  uint32_t presentModeCount{0};
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface, &presentModeCount,
                                            nullptr);

  presentModes_.resize(presentModeCount);
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface, &presentModeCount,
                                            presentModes_.data());
}
}  // namespace VulkanCore
