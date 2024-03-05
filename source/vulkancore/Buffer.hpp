#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Common.hpp"
#include "Utility.hpp"
#include "vk_mem_alloc.h"

namespace VulkanCore {

class Context;
class Texture;

class Buffer final {
 public:
  MOVABLE_ONLY(Buffer);

  // using this API will create stagingBuffer by default
  explicit Buffer(const Context* context, VmaAllocator vmaAllocator, VkDeviceSize size,
                  VkBufferUsageFlags usage, Buffer* actualBuffer,
                  const std::string& name = "");

  // this API will create only non staging by default
  explicit Buffer(const Context* context, VmaAllocator vmaAllocator,
                  const VkBufferCreateInfo& createInfo,
                  const VmaAllocationCreateInfo& allocInfo, const std::string& name = "");

  ~Buffer();

  VkDeviceSize size() const;

  void upload(VkDeviceSize offset = 0) const;

  void upload(VkDeviceSize offset, VkDeviceSize size) const;

  void uploadStagingBufferToGPU(const VkCommandBuffer& commandBuffer,
                                uint64_t srcOffset = 0, uint64_t dstOffset = 0) const;

  void copyDataToBuffer(const void* data, size_t size) const;

  VkBuffer vkBuffer() const { return buffer_; }

  VkDeviceAddress vkDeviceAddress() const;

  // managed by buffer
  VkBufferView requestBufferView(VkFormat viewFormat);

 private:
  const Context* context_ = nullptr;
  VmaAllocator allocator_;
  VkDeviceSize size_;
  VkBufferUsageFlags usage_;
  VmaAllocationCreateInfo allocCreateInfo_;
  VkBuffer buffer_ = VK_NULL_HANDLE;
  Buffer* actualBufferIfStaging_ = nullptr;
  VmaAllocation allocation_ = nullptr;
  VmaAllocationInfo allocationInfo_ = {};
  mutable VkDeviceAddress bufferDeviceAddress_ = 0;
  mutable void* mappedMemory_ = nullptr;
  std::unordered_map<VkFormat, VkBufferView> bufferViews_;
};

}  // namespace VulkanCore
