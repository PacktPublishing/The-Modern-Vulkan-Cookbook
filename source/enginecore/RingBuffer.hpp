#pragma once

#include <vector>

#include "vulkancore/Buffer.hpp"
#include "vulkancore/Common.hpp"
#include "vulkancore/Context.hpp"

namespace EngineCore {
// currently RingBuffer is only uniform type but can be extended to other types
// as well
class RingBuffer final {
 public:
  RingBuffer(uint32_t ringSize, const VulkanCore::Context& context, size_t bufferSize,
             const std::string& name = "Ring Buffer");

  void moveToNextBuffer();

  const VulkanCore::Buffer* buffer() const;

  const std::shared_ptr<VulkanCore::Buffer>& buffer(uint32_t index) const {
    ASSERT(index < bufferRing_.size(), "index should be smaller");
    return bufferRing_[index];
  }

 private:
  uint32_t ringSize_;
  const VulkanCore::Context& context_;
  uint32_t ringIndex_ = 0;
  size_t bufferSize_;
  std::vector<std::shared_ptr<VulkanCore::Buffer>> bufferRing_;
};
}  // namespace EngineCore
