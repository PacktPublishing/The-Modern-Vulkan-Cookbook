#include "CommandQueueManager.hpp"

#include <algorithm>
#include <iostream>
#include <tracy/Tracy.hpp>

#include "Context.hpp"

namespace VulkanCore {

CommandQueueManager::CommandQueueManager(const Context& context, VkDevice device,
                                         uint32_t count, uint32_t concurrentNumCommands,
                                         uint32_t queueFamilyIndex, VkQueue queue,
                                         VkCommandPoolCreateFlags flags,
                                         const std::string& name)
    : commandsInFlight_(concurrentNumCommands),
      queueFamilyIndex_(queueFamilyIndex),
      queue_(queue),
      device_(device) {
  fences_.reserve(commandsInFlight_);
  isSubmitted_.reserve(commandsInFlight_);
  bufferToDispose_.resize(commandsInFlight_);
  deallocators_.resize(commandsInFlight_);

  const VkCommandPoolCreateInfo commandPoolInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = flags,
      .queueFamilyIndex = queueFamilyIndex_,
  };
  VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool_));
  context.setVkObjectname(commandPool_, VK_OBJECT_TYPE_COMMAND_POOL,
                          "Command pool: " + name);

  const VkCommandBufferAllocateInfo commandBufferInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = commandPool_,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };
  for (size_t i = 0; i < count; ++i) {
    VkCommandBuffer cmdBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device, &commandBufferInfo, &cmdBuffer));
    context.setVkObjectname(cmdBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER,
                            "Command buffer: " + name + " " + std::to_string(i));

    commandBuffers_.push_back(cmdBuffer);
  }

  for (size_t i = 0; i < commandsInFlight_; ++i) {
    VkFence fence;
    const VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &fence));

    fences_.push_back(std::move(fence));
    isSubmitted_.push_back(false);
  }
}

CommandQueueManager::~CommandQueueManager() {
  deallocateResources();

  for (size_t i = 0; i < commandsInFlight_; ++i) {
    vkDestroyFence(device_, fences_[i], nullptr);
  }

  for (size_t i = 0; i < commandBuffers_.size(); ++i) {
    vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffers_[i]);
  }

  vkDestroyCommandPool(device_, commandPool_, nullptr);
}

void CommandQueueManager::submit(const VkSubmitInfo* submitInfo) {
  ZoneScopedN("CmdMgr: submit");
  VK_CHECK(vkResetFences(device_, 1, &fences_[fenceCurrentIndex_]));
  VK_CHECK(vkQueueSubmit(queue_, 1, submitInfo, fences_[fenceCurrentIndex_]));
  isSubmitted_[fenceCurrentIndex_] = true;
}

void CommandQueueManager::goToNextCmdBuffer() {
  commandBufferCurrentIndex_ =
      (commandBufferCurrentIndex_ + 1) % static_cast<uint32_t>(commandBuffers_.size());
  fenceCurrentIndex_ = (fenceCurrentIndex_ + 1) % commandsInFlight_;
}

void CommandQueueManager::waitUntilSubmitIsComplete() {
  ZoneScopedN("CmdMgr: waitUntilSubmitIscomplete");

  if (!isSubmitted_[fenceCurrentIndex_]) {
    return;
  }

  const auto result =
      vkWaitForFences(device_, 1, &fences_[fenceCurrentIndex_], true, UINT32_MAX);
  if (result == VK_TIMEOUT) {
    std::cerr << "Timeout!" << std::endl;
    vkDeviceWaitIdle(device_);
  }

  isSubmitted_[fenceCurrentIndex_] = false;
  bufferToDispose_[fenceCurrentIndex_].clear();
  deallocateResources();
}

void CommandQueueManager::waitUntilAllSubmitsAreComplete() {
  ZoneScopedN("CmdMgr: waitUntilAllSubmitIscomplete");
  for (size_t index = 0; auto& fence : fences_) {
    VK_CHECK(vkWaitForFences(device_, 1, &fence, true, UINT32_MAX));
    VK_CHECK(vkResetFences(device_, 1, &fence));
    isSubmitted_[index++] = false;
  }
  bufferToDispose_.clear();
  deallocateResources();
}

void CommandQueueManager::disposeWhenSubmitCompletes(std::shared_ptr<Buffer> buffer) {
  ZoneScopedN("CmdMgr: disposeWhenSubmitCompletes");
  bufferToDispose_[fenceCurrentIndex_].push_back(std::move(buffer));
}

void CommandQueueManager::disposeWhenSubmitCompletes(
    std::function<void()>&& deallocator) {
  ZoneScopedN("CmdMgr: disposeWhenSubmitCompletes");
  deallocators_[fenceCurrentIndex_].push_back(std::move(deallocator));
}

VkCommandBuffer CommandQueueManager::getCmdBufferToBegin() {
  ZoneScopedN("CmdMgr: getCmdBufferToBegin");
  VK_CHECK(vkWaitForFences(device_, 1, &fences_[fenceCurrentIndex_], true, UINT32_MAX));
  VK_CHECK(vkResetCommandBuffer(commandBuffers_[commandBufferCurrentIndex_],
                                VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT));

  const VkCommandBufferBeginInfo info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  VK_CHECK(vkBeginCommandBuffer(commandBuffers_[commandBufferCurrentIndex_], &info));

  return commandBuffers_[commandBufferCurrentIndex_];
}

VkCommandBuffer CommandQueueManager::getCmdBuffer() {
  ZoneScopedN("CmdMgr: getCmdBuffer");
  const VkCommandBufferAllocateInfo commandBufferInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = commandPool_,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };
  VkCommandBuffer cmdBuffer{VK_NULL_HANDLE};
  VK_CHECK(vkAllocateCommandBuffers(device_, &commandBufferInfo, &cmdBuffer));

  return cmdBuffer;
}

void CommandQueueManager::endCmdBuffer(VkCommandBuffer cmdBuffer) {
  ZoneScopedN("CmdMgr: endCmdBuffer");
  VK_CHECK(vkEndCommandBuffer(cmdBuffer));
}

void CommandQueueManager::deallocateResources() {
  for (auto& deallocators : deallocators_) {
    for (auto& deallocator : deallocators) {
      deallocator();
    }
  }
}
}  // namespace VulkanCore
