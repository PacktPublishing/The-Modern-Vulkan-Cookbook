#pragma once

#include <unordered_map>

#include "Common.hpp"
#include "Utility.hpp"
#include "vk_mem_alloc.h"

namespace VulkanCore {

class Buffer;
class Context;

class Texture final {
 public:
  MOVABLE_ONLY(Texture);

  explicit Texture(const Context& context, VkImageType type, VkFormat format,
                   VkImageCreateFlags flags, VkImageUsageFlags usageFlags,
                   VkExtent3D extents, uint32_t numMipLevels,
                   uint32_t layerCount, VkMemoryPropertyFlags memoryFlags,
                   bool generateMips = false,
                   VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT,
                   const std::string& name = "", bool multiview = false,
                   VkImageTiling = VK_IMAGE_TILING_OPTIMAL);

  // To be used with images that have been created elsewhere (like swapchains,
  // for instance)
  explicit Texture(const Context& context, VkDevice device, VkImage image,
                   VkFormat format, VkExtent3D extents, uint32_t numlayers = 1,
                   bool multiview = false, const std::string& name = "");

  ~Texture();

  VkFormat vkFormat() const { return format_; }

  VkImageView vkImageView(uint32_t mipLevel = UINT32_MAX);

  VkImage vkImage() const { return image_; }

  VkExtent3D vkExtents() const { return extents_; }

  VkImageLayout vkLayout() const { return layout_; }

  void setImageLayout(VkImageLayout layout) { layout_ = layout; }

  VkDeviceSize vkDeviceSize() const { return deviceSize_; }

  void uploadAndGenMips(VkCommandBuffer cmdBuffer, const Buffer* stagingBuffer,
                        void* data);

  void uploadOnly(VkCommandBuffer cmdBuffer, const Buffer* stagingBuffer,
                  void* data, uint32_t layer = 0);

  void addReleaseBarrier(VkCommandBuffer cmdBuffer,
                         uint32_t srcQueueFamilyIndex,
                         uint32_t dstQueueFamilyIndex);

  void addAcquireBarrier(VkCommandBuffer cmdBuffer,
                         uint32_t srcQueueFamilyIndex,
                         uint32_t dstQueueFamilyIndex);

  void transitionImageLayout(VkCommandBuffer cmdBuffer,
                             VkImageLayout newLayout);

  bool isDepth() const;

  bool isStencil() const;

  uint32_t pixelSizeInBytes() const;

  uint32_t numMipLevels() const;

  void generateMips(VkCommandBuffer cmdBuffer);

  std::vector<std::shared_ptr<VkImageView>> generateViewForEachMips();

  VkSampleCountFlagBits VkSampleCount() const;

 private:
  uint32_t getMipLevelsCount(uint32_t texWidth, uint32_t texHeight) const;

  VkImageView createImageView(const Context& context, VkImageViewType viewType,
                              VkFormat format, uint32_t numMipLevels,
                              uint32_t layers, const std::string& name = "");

 private:
  const Context& context_;
  VmaAllocator vmaAllocator_ = nullptr;
  VmaAllocation vmaAllocation_ = nullptr;
  VkDeviceSize deviceSize_ = 0;
  VkImageUsageFlags usageFlags_ = 0;
  VkImageCreateFlags flags_ = 0;
  VkImageType type_ = VK_IMAGE_TYPE_2D;
  VkImage image_ = VK_NULL_HANDLE;
  VkImageView imageView_ = VK_NULL_HANDLE;
  std::unordered_map<uint32_t, VkImageView> imageViewFramebuffers_;
  VkFormat format_ = VK_FORMAT_UNDEFINED;
  VkExtent3D extents_;
  VkImageLayout layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
  bool ownsVkImage_ = false;
  uint32_t mipLevels_ = 1;
  uint32_t layerCount_ = 1;
  bool multiview_ = false;
  bool generateMips_ = false;
  VkImageViewType viewType_;
  VkSampleCountFlagBits msaaSamples_ = VK_SAMPLE_COUNT_1_BIT;
  VkImageTiling imageTiling_ = VK_IMAGE_TILING_OPTIMAL;
  std::string debugName_;
};

}  // namespace VulkanCore
