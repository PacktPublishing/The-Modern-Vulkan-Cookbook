#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Buffer.hpp"
#include "Common.hpp"
#include "Utility.hpp"

namespace VulkanCore {

class Context;

class CommandQueueManager final {
 public:
  explicit CommandQueueManager(
      const Context& context, VkDevice device, uint32_t count,
      uint32_t concurrentNumCommands, uint32_t queueFamilyIndex, VkQueue queue,
      VkCommandPoolCreateFlags flags =
          VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,  // default is
      // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
      // since we want to
      // record a command
      // buffer every
      // frame, so we want
      // to be able to
      // reset and record
      // over it
      const std::string& name = "");

  ~CommandQueueManager();

  void submit(const VkSubmitInfo* submitInfo);

  void goToNextCmdBuffer();

  void waitUntilSubmitIsComplete();
  void waitUntilAllSubmitsAreComplete();

  void disposeWhenSubmitCompletes(std::shared_ptr<Buffer> buffer);
  void disposeWhenSubmitCompletes(std::function<void()>&& deallocator);

  VkCommandBuffer getCmdBufferToBegin();

  VkCommandBuffer getCmdBuffer();

  void endCmdBuffer(VkCommandBuffer cmdBuffer);

  uint32_t queueFamilyIndex() const { return queueFamilyIndex_; }

 private:
  void deallocateResources();

 private:
  uint32_t commandsInFlight_ = 2;
  uint32_t queueFamilyIndex_ = 0;
  VkQueue queue_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkCommandPool commandPool_ = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> commandBuffers_;
  std::vector<VkFence> fences_;
  std::vector<bool> isSubmitted_;
  uint32_t fenceCurrentIndex_ = 0;
  uint32_t commandBufferCurrentIndex_ = 0;
  std::vector<std::vector<std::shared_ptr<Buffer>>>
      bufferToDispose_;  // fenceIndex to list of buffers associated with that
                         // fence that needs to be released
  std::vector<std::vector<std::function<void()>>> deallocators_;
};

}  // namespace VulkanCore
