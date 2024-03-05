#pragma once

#include <any>
#include <array>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "Buffer.hpp"
#include "CommandQueueManager.hpp"
#include "Common.hpp"
#include "PhysicalDevice.hpp"
#include "Pipeline.hpp"
#include "ShaderModule.hpp"
#include "Swapchain.hpp"
#include "Utility.hpp"
#include "vk_mem_alloc.h"

namespace VulkanCore {

class Framebuffer;
class RenderPass;
class Sampler;
class Texture;

template <size_t CHAIN_SIZE = 10>
class VulkanFeatureChain {
 public:
  VulkanFeatureChain() = default;
  MOVABLE_ONLY(VulkanFeatureChain);

  auto& pushBack(auto nextVulkanChainStruct) {
    ASSERT(currentIndex_ < CHAIN_SIZE, "Chain is full");
    data_[currentIndex_] = nextVulkanChainStruct;

    auto& next = std::any_cast<decltype(nextVulkanChainStruct)&>(data_[currentIndex_]);

    next.pNext = std::exchange(firstNext_, &next);
    currentIndex_++;

    return next;
  }

  [[nodiscard]] void* firstNextPtr() const { return firstNext_; };

 private:
  std::array<std::any, CHAIN_SIZE> data_;
  VkBaseInStructure* root_ = nullptr;
  int currentIndex_ = 0;
  void* firstNext_ = VK_NULL_HANDLE;
};

class Context final {
 public:
  MOVABLE_ONLY(Context);

  explicit Context(void* window, const std::vector<std::string>& requestedLayers,
                   const std::vector<std::string>& requestedInstanceExtensions,
                   const std::vector<std::string>& requestedDeviceExtensions,
                   VkQueueFlags requestedQueueTypes, bool printEnumerations = false,
                   bool enableRayTracing = false, const std::string& name = "");

  explicit Context(const VkApplicationInfo& appInfo,
                   const std::vector<std::string>& requestedLayers,
                   const std::vector<std::string>& requestedInstanceExtensions,
                   bool printEnumerations = false, const std::string& name = "");

  void createVkDevice(VkPhysicalDevice vkPhysicalDevice,
                      const std::vector<std::string>& requestedDeviceExtensions,
                      VkQueueFlags requestedQueueTypes, const std::string& name = "");

  ~Context();

  static void enableDefaultFeatures();

  static void enableScalarLayoutFeatures();

  static void enableDynamicRenderingFeature();

  static void enableBufferDeviceAddressFeature();

  static void enableIndirectRenderingFeature();

  static void enable16bitFloatFeature();

  static void enableIndependentBlending();

  static void enableMaintenance4Feature();

  static void enableSynchronization2Feature();

  static void enableRayTracingFeatures();

  static bool enableMultiViewFlag_;
  static void enableMultiView();

  static bool isMultiviewEnabled();

  static void enableFragmentDensityMapFeatures();

  static void enableFragmentDensityMapOffsetFeatures();

  VkDevice device() const { return device_; }

  VkInstance instance() const { return instance_; }

  [[nodiscard]] inline VmaAllocator memoryAllocator() const { return allocator_; }

  const PhysicalDevice& physicalDevice() const;

  void createSwapchain(VkFormat format, VkColorSpaceKHR colorSpace,
                       VkPresentModeKHR presentMode, const VkExtent2D& extent);

  Swapchain* swapchain() const;

  VkQueue graphicsQueue(int index = 0) const { return graphicsQueues_[index]; }

  std::shared_ptr<Buffer> createBuffer(size_t size, VkBufferUsageFlags flags,
                                       VmaMemoryUsage memoryUsage,
                                       const std::string& name = "") const;

  std::shared_ptr<Buffer> createPersistentBuffer(size_t size, VkBufferUsageFlags flags,
                                                 const std::string& name = "") const;

  std::shared_ptr<Buffer> createStagingBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                              const std::string& name = "") const;

  std::shared_ptr<Buffer> createStagingBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                              Buffer* actualBuffer,
                                              const std::string& name = "") const;

  void uploadToGPUBuffer(VulkanCore::CommandQueueManager& queueMgr,
                         VkCommandBuffer commandBuffer, VulkanCore::Buffer* gpuBuffer,
                         const void* data, long totalSize,
                         uint64_t gpuBufferOffset = 0) const;

  std::shared_ptr<Texture> createTexture(
      VkImageType type, VkFormat format, VkImageCreateFlags flags,
      VkImageUsageFlags usageFlags, VkExtent3D extents, uint32_t numMipLevels,
      uint32_t layerCount, VkMemoryPropertyFlags memoryFlags, bool generateMips = false,
      VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT,
      const std::string& name = "") const;

  std::shared_ptr<Sampler> createSampler(VkFilter minFilter, VkFilter magFilter,
                                         VkSamplerAddressMode addressModeU,
                                         VkSamplerAddressMode addressModeV,
                                         VkSamplerAddressMode addressModeW, float maxLod,
                                         const std::string& name = "") const;

  std::shared_ptr<Sampler> createSampler(VkFilter minFilter, VkFilter magFilter,
                                         VkSamplerAddressMode addressModeU,
                                         VkSamplerAddressMode addressModeV,
                                         VkSamplerAddressMode addressModeW, float maxLod,
                                         bool compareEnable, VkCompareOp compareOp,
                                         const std::string& name = "") const;

  CommandQueueManager createGraphicsCommandQueue(uint32_t count,
                                                 uint32_t concurrentNumCommands,

                                                 const std::string& name = "",
                                                 int graphicsQueueIndex = -1);

  std::shared_ptr<ShaderModule> createShaderModule(const std::string& filePath,
                                                   VkShaderStageFlagBits stages,
                                                   const std::string& name = "");
  std::shared_ptr<ShaderModule> createShaderModule(const std::string& filePath,
                                                   const std::string& entryPoint,
                                                   VkShaderStageFlagBits stages,
                                                   const std::string& name = "");
  std::shared_ptr<ShaderModule> createShaderModule(const std::vector<char>& shader,
                                                   const std::string& entryPoint,
                                                   VkShaderStageFlagBits stages,
                                                   const std::string& name = "");

  std::shared_ptr<Pipeline> createGraphicsPipeline(
      const Pipeline::GraphicsPipelineDescriptor& desc, VkRenderPass renderPass,
      const std::string& name = "");

  std::shared_ptr<Pipeline> createComputePipeline(
      const Pipeline::ComputePipelineDescriptor& desc, const std::string& name = "");

  std::shared_ptr<Pipeline> createRayTracingPipeline(
      const Pipeline::RayTracingPipelineDescriptor& desc, const std::string& name = "");

  CommandQueueManager createTransferCommandQueue(uint32_t count,
                                                 uint32_t concurrentNumCommands,
                                                 const std::string& name,
                                                 int transferQueueIndex = -1);

  std::shared_ptr<RenderPass> createRenderPass(
      const std::vector<std::shared_ptr<Texture>>& attachments,
      const std::vector<VkAttachmentLoadOp>& loadOp,
      const std::vector<VkAttachmentStoreOp>& storeOp,
      const std::vector<VkImageLayout>& layout, VkPipelineBindPoint bindPoint,
      const std::vector<std::shared_ptr<Texture>>& resolveAttachments = {},
      const std::string& name = "") const;

  std::unique_ptr<Framebuffer> createFramebuffer(
      VkRenderPass renderPass,
      const std::vector<std::shared_ptr<Texture>>& colorAttachments,
      std::shared_ptr<Texture> depthAttachment,
      std::shared_ptr<Texture> stencilAttachment, const std::string& name = "") const;

  /// <summary>
  /// Exports the current internal state of VMA to a file, which can be
  /// inspected graphically. More information:
  /// https://chromium.googlesource.com/external/github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/+/refs/tags/upstream/v2.1.0/tools/VmaDumpVis/README.md
  /// </summary>
  /// <param name="fileName">The information will be written into the the
  /// file with this name</param>
  void dumpMemoryStats(const std::string& fileName) const;

  template <typename T>
  void setVkObjectname(T handle, VkObjectType type, const std::string& name) const {
#if defined(VK_EXT_debug_utils)
    if (enabledInstanceExtensions_.find(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) !=
        enabledInstanceExtensions_.end()) {
      const VkDebugUtilsObjectNameInfoEXT objectNameInfo = {
          .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
          .objectType = type,
          .objectHandle = reinterpret_cast<uint64_t>(handle),
          .pObjectName = name.c_str(),
      };
      VK_CHECK(vkSetDebugUtilsObjectNameEXT(device_, &objectNameInfo));
    }
#else
    (void)handle;
    (void)type;
    (void)name;
#endif
  }

  void beginDebugUtilsLabel(VkCommandBuffer commandBuffer, const std::string& name,
                            const glm::vec4& color) const;

  void endDebugUtilsLabel(VkCommandBuffer commandBuffer) const;

 private:
  void createMemoryAllocator();

  [[nodiscard]] static std::vector<std::string> enumerateInstanceLayers(
      bool printEnumerations_ = false);

  [[nodiscard]] std::vector<std::string> enumerateInstanceExtensions();

  [[nodiscard]] std::vector<PhysicalDevice> enumeratePhysicalDevices(
      const std::vector<std::string>& requestedExtensions, bool enableRayTracing) const;

  [[nodiscard]] PhysicalDevice choosePhysicalDevice(
      std::vector<PhysicalDevice>&& devices,
      const std::vector<std::string>& deviceExtensions) const;

 private:
  const VkApplicationInfo applicationInfo_ = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "Modern Vulkan Cookbook",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_3,
  };
  VkInstance instance_ = VK_NULL_HANDLE;
  PhysicalDevice physicalDevice_;
  VkDevice device_ = VK_NULL_HANDLE;
  VmaAllocator allocator_ = nullptr;
  bool printEnumerations_ = false;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  std::vector<VkSurfaceFormatKHR> surfaceFormats_;
  // these will be considered main queues
  VkQueue presentationQueue_ = VK_NULL_HANDLE;

  static VkPhysicalDeviceFeatures physicalDeviceFeatures_;
  static VkPhysicalDeviceVulkan11Features enable11Features_;
  static VkPhysicalDeviceVulkan12Features enable12Features_;
  static VkPhysicalDeviceVulkan13Features enable13Features_;

  // features required for ray tracing
  static VkPhysicalDeviceAccelerationStructureFeaturesKHR accelStructFeatures_;
  static VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures_;
  static VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures_;
  static VkPhysicalDeviceMultiviewFeatures multiviewFeatures_;
  static VkPhysicalDeviceFragmentDensityMapFeaturesEXT fragmentDensityMapFeatures_;
  static VkPhysicalDeviceFragmentDensityMapOffsetFeaturesQCOM
      fragmentDensityMapOffsetFeatures_;

  // these are extra queues which can be used for any other async stuff if
  // required, these won't contain above queues
  std::vector<VkQueue> graphicsQueues_;
  std::vector<VkQueue> computeQueues_;
  std::vector<VkQueue> transferQueues_;
  std::vector<VkQueue> sparseQueues_;

  std::unique_ptr<Swapchain> swapchain_;
  std::unordered_set<std::string> enabledLayers_;
  std::unordered_set<std::string> enabledInstanceExtensions_;
#if defined(VK_EXT_debug_utils)
  VkDebugUtilsMessengerEXT messenger_ = VK_NULL_HANDLE;
#endif
};

}  // namespace VulkanCore
