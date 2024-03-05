#include "Sampler.hpp"

#include "Context.hpp"

namespace VulkanCore {
Sampler::Sampler(const Context& context, VkFilter minFilter, VkFilter magFilter,
                 VkSamplerAddressMode addressModeU, VkSamplerAddressMode addressModeV,
                 VkSamplerAddressMode addressModeW, float maxLod, const std::string& name)
    : device_{context.device()} {
  const VkSamplerCreateInfo samplerInfo = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = minFilter,
      .minFilter = magFilter,
      .mipmapMode =
          maxLod > 0 ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .addressModeU = addressModeU,
      .addressModeV = addressModeV,
      .addressModeW = addressModeW,
      .mipLodBias = 0,
      .anisotropyEnable = VK_FALSE,
      .minLod = 0,
      .maxLod = maxLod,
  };
  VK_CHECK(vkCreateSampler(device_, &samplerInfo, nullptr, &sampler_));
  context.setVkObjectname(sampler_, VK_OBJECT_TYPE_SAMPLER, "Sampler: " + name);
};

Sampler::Sampler(const Context& context, VkFilter minFilter, VkFilter magFilter,
                 VkSamplerAddressMode addressModeU, VkSamplerAddressMode addressModeV,
                 VkSamplerAddressMode addressModeW, float maxLod, bool compareEnable,
                 VkCompareOp compareOp, const std::string& name /*= ""*/)
    : device_{context.device()} {
  const VkSamplerCreateInfo samplerInfo = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = minFilter,
      .minFilter = magFilter,
      .mipmapMode =
          maxLod > 0 ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .addressModeU = addressModeU,
      .addressModeV = addressModeV,
      .addressModeW = addressModeW,
      .mipLodBias = 0,
      .anisotropyEnable = VK_FALSE,
      .compareEnable = compareEnable,
      .compareOp = compareOp,
      .minLod = 0,
      .maxLod = maxLod,
  };
  VK_CHECK(vkCreateSampler(device_, &samplerInfo, nullptr, &sampler_));
  context.setVkObjectname(sampler_, VK_OBJECT_TYPE_SAMPLER, "Sampler: " + name);
}

}  // namespace VulkanCore
