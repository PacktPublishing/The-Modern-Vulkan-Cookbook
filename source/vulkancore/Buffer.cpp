#include "Buffer.hpp"

#include <algorithm>
#include <iostream>

#include "Context.hpp"
#include "Texture.hpp"

namespace VulkanCore {

Buffer::Buffer(const Context* context, VmaAllocator vmaAllocator, VkDeviceSize size,
               VkBufferUsageFlags usage, Buffer* actualBuffer, const std::string& name)
    : context_{context},
      allocator_(vmaAllocator),
      size_(size),
      actualBufferIfStaging_(actualBuffer) {
  ASSERT(actualBufferIfStaging_,
         "Actual Buffer must not be null in case of staging buffer");
  ASSERT(actualBufferIfStaging_->usage_ & VK_BUFFER_USAGE_TRANSFER_DST_BIT,
         "Actual buffer must be dst buffer in case of staging buffer");
  ASSERT(actualBufferIfStaging_->allocCreateInfo_.usage == VMA_MEMORY_USAGE_GPU_ONLY,
         "Actual buffer must be GPU only in case of staging buffer, staging "
         "buffer will upload from cpu to this gpu buffer");
  VkBufferCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = {},
      .size = size_,
      .usage = usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = {},
      .pQueueFamilyIndices = {},
  };

  allocCreateInfo_ = {VMA_ALLOCATION_CREATE_MAPPED_BIT, VMA_MEMORY_USAGE_CPU_ONLY};

  VK_CHECK(vmaCreateBuffer(allocator_, &createInfo, &allocCreateInfo_, &buffer_,
                           &allocation_, nullptr));
  vmaGetAllocationInfo(allocator_, allocation_, &allocationInfo_);

  context->setVkObjectname(buffer_, VK_OBJECT_TYPE_BUFFER, "Staging Buffer: " + name);
}

Buffer::Buffer(const Context* context, VmaAllocator vmaAllocator,
               const VkBufferCreateInfo& createInfo,
               const VmaAllocationCreateInfo& allocInfo, const std::string& name)
    : context_{context},
      allocator_(vmaAllocator),
      size_(createInfo.size),
      usage_(createInfo.usage),
      allocCreateInfo_(allocInfo) {
  VK_CHECK(vmaCreateBuffer(allocator_, &createInfo, &allocInfo, &buffer_, &allocation_,
                           nullptr));
  vmaGetAllocationInfo(allocator_, allocation_, &allocationInfo_);

  context->setVkObjectname(buffer_, VK_OBJECT_TYPE_BUFFER, "Buffer: " + name);
}

Buffer::~Buffer() {
  if (mappedMemory_) {
    vmaUnmapMemory(allocator_, allocation_);
  }

  for (auto& [bufferViewFormat, bufferView] : bufferViews_) {
    vkDestroyBufferView(context_->device(), bufferView, nullptr);
  }

  vmaDestroyBuffer(allocator_, buffer_, allocation_);
}

VkDeviceSize Buffer::size() const { return size_; }

void Buffer::upload(VkDeviceSize offset) const { upload(offset, size_); }

void Buffer::upload(VkDeviceSize offset, VkDeviceSize size) const {
  VK_CHECK(
      vmaFlushAllocation(allocator_, allocation_, offset,
                         size));  // we could control the offset & size as well if needed
}

void Buffer::uploadStagingBufferToGPU(const VkCommandBuffer& commandBuffer,
                                      uint64_t srcOffset, uint64_t dstOffset) const {
  VkBufferCopy region{.srcOffset = srcOffset, .dstOffset = dstOffset, .size = size_};
  ASSERT(actualBufferIfStaging_ != nullptr,
         "actualBufferIfStaging_ can't be null in case of staging");
  vkCmdCopyBuffer(commandBuffer, vkBuffer(), actualBufferIfStaging_->vkBuffer(), 1,
                  &region);
}

void Buffer::copyDataToBuffer(const void* data, size_t size) const {
  if (!mappedMemory_) {
    VK_CHECK(vmaMapMemory(allocator_, allocation_, &mappedMemory_));
  }
  memcpy(mappedMemory_, data, size);
}

VkDeviceAddress Buffer::vkDeviceAddress() const {
  if (actualBufferIfStaging_) {
    return actualBufferIfStaging_->vkDeviceAddress();
  }

#if defined(VK_KHR_buffer_device_address) && defined(_WIN32)
  if (!bufferDeviceAddress_) {
    const VkBufferDeviceAddressInfo bdAddressInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer_,
    };
    bufferDeviceAddress_ = vkGetBufferDeviceAddress(context_->device(), &bdAddressInfo);
  }
  return bufferDeviceAddress_;
#else
  return 0;
#endif
}

VkBufferView Buffer::requestBufferView(VkFormat viewFormat) {
  auto itr = bufferViews_.find(viewFormat);
  if (itr != bufferViews_.end()) {
    return itr->second;
  }

  VkBufferViewCreateInfo createInfo{
      .sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
      .flags = 0,
      .buffer = vkBuffer(),
      .format = viewFormat,
      .offset = 0,
      .range = size_,
  };
  VkBufferView bufferView;
  VK_CHECK(vkCreateBufferView(context_->device(), &createInfo, nullptr, &bufferView));
  bufferViews_[viewFormat] = bufferView;
  return bufferView;
}

}  // namespace VulkanCore
