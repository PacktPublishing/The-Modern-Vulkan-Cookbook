#include "RingBuffer.hpp"

namespace EngineCore {

RingBuffer::RingBuffer(uint32_t ringSize, const VulkanCore::Context& context,
                       size_t bufferSize, const std::string& name)
    : ringSize_(ringSize), context_(context), bufferSize_(bufferSize) {
  for (int i = 0; i < ringSize_; ++i) {
    bufferRing_.emplace_back(
        context_.createPersistentBuffer(bufferSize_,
#if defined(VK_KHR_buffer_device_address) && defined(_WIN32)
                                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
#endif
                                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                        name + " " + std::to_string(i)));
  }
}

void RingBuffer::moveToNextBuffer() {
  ringIndex_++;
  if (ringIndex_ >= ringSize_) {
    ringIndex_ = 0;
  }
}

const VulkanCore::Buffer* RingBuffer::buffer() const {
  return bufferRing_[ringIndex_].get();
}

}  // namespace EngineCore
