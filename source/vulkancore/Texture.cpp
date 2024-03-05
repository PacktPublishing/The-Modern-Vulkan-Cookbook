#include "Texture.hpp"

#ifdef _WIN32
#include <vulkan/vk_enum_string_helper.h>
#endif

#include "Buffer.hpp"
#include "Context.hpp"

namespace VulkanCore {

Texture::Texture(const Context& context, VkImageType type, VkFormat format,
                 VkImageCreateFlags flags, VkImageUsageFlags usageFlags,
                 VkExtent3D extents, uint32_t numMipLevels, uint32_t layerCount,
                 VkMemoryPropertyFlags memoryFlags, bool generateMips,
                 VkSampleCountFlagBits msaaSamples, const std::string& name,
                 bool multiview, VkImageTiling imageTiling)
    : context_{context},
      vmaAllocator_{context.memoryAllocator()},
      usageFlags_{usageFlags},
      flags_{flags},
      type_{type},
      format_{format},
      extents_{extents},
      ownsVkImage_{true},
      mipLevels_(numMipLevels),
      layerCount_(layerCount),
      multiview_(multiview),
      generateMips_(generateMips),
      msaaSamples_(msaaSamples),
      imageTiling_(imageTiling),
      debugName_(name) {
  ASSERT(extents.width > 0 && extents.height > 0,
         "Texture cannot have dimensions equal to 0");
  ASSERT(mipLevels_ > 0, "Texture must have at least one mip level");

  if (generateMips_) {
    mipLevels_ = getMipLevelsCount(extents.width, extents.height);
  }

  ASSERT(!(mipLevels_ > 1 && msaaSamples_ != VK_SAMPLE_COUNT_1_BIT),
         "Multisampled images cannot have more than 1 mip level");

  const VkImageCreateInfo imageInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .flags = flags,
      .imageType = type,
      .format = format,
      .extent = extents,
      .mipLevels = mipLevels_,
      .arrayLayers = layerCount,
      .samples = msaaSamples_,
      .tiling = imageTiling_,
      .usage = usageFlags,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  const VmaAllocationCreateInfo allocCreateInfo = {
      .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
      .usage = memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                   ? VMA_MEMORY_USAGE_AUTO_PREFER_HOST
                   : VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      .priority = 1.0f,
  };

  VK_CHECK(vmaCreateImage(vmaAllocator_, &imageInfo, &allocCreateInfo, &image_,
                          &vmaAllocation_, nullptr));

  if (vmaAllocation_ != nullptr) {
    VmaAllocationInfo allocationInfo;
    vmaGetAllocationInfo(vmaAllocator_, vmaAllocation_, &allocationInfo);
    deviceSize_ = allocationInfo.size;
  }

  context.setVkObjectname(image_, VK_OBJECT_TYPE_IMAGE, "Image: " + name);

  const VkImageViewType imageViewType =
      VulkanCore::imageTypeToImageViewType(type, flags, multiview_);

  viewType_ = imageViewType;

  imageView_ =
      createImageView(context, imageViewType, format_, mipLevels_, layerCount, name);
}

Texture::Texture(const Context& context, VkDevice device, VkImage image, VkFormat format,
                 VkExtent3D extents, uint32_t numlayers, bool multiview,
                 const std::string& name)
    : context_{context},
      image_{image},
      format_{format},
      extents_{extents},
      layerCount_(numlayers),
      multiview_(multiview),
      ownsVkImage_{false},
      debugName_{name} {
  context.setVkObjectname(image_, VK_OBJECT_TYPE_IMAGE, "Image: " + name);

  imageView_ = createImageView(
      context, !multiview_ ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY, format,
      1, layerCount_, name);
}

Texture::~Texture() {
  for (const auto imageView : imageViewFramebuffers_) {
    vkDestroyImageView(context_.device(), imageView.second, nullptr);
  }

  vkDestroyImageView(context_.device(), imageView_, nullptr);

  if (ownsVkImage_) {
    vmaDestroyImage(vmaAllocator_, image_, vmaAllocation_);
  }
}

VkImageView Texture::vkImageView(uint32_t mipLevel) {
  ASSERT(mipLevel == UINT32_MAX || mipLevel < mipLevels_, "Invalid mip level");

  if (mipLevel == UINT32_MAX) {
    return imageView_;
  }

  if (!imageViewFramebuffers_.contains(mipLevel)) {
    const VkImageViewType imageViewType =
        VulkanCore::imageTypeToImageViewType(type_, flags_, multiview_);

    imageViewFramebuffers_[mipLevel] =
        createImageView(context_, imageViewType, format_, 1, VK_REMAINING_ARRAY_LAYERS,
                        "Image View for Framebuffer: " + debugName_);
  }

  return imageViewFramebuffers_[mipLevel];
}

void Texture::uploadAndGenMips(VkCommandBuffer cmdBuffer, const Buffer* stagingBuffer,
                               void* data) {
  uploadOnly(cmdBuffer, stagingBuffer, data);
  context_.beginDebugUtilsLabel(cmdBuffer,
                                "Transition to Shader_Read_Only & Generate mips",
                                {1.0f, 0.0f, 0.0f, 1.0f});
  generateMips(cmdBuffer);
  if (layout_ != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    transitionImageLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }

  context_.endDebugUtilsLabel(cmdBuffer);
}

void Texture::uploadOnly(VkCommandBuffer cmdBuffer, const Buffer* stagingBuffer,
                         void* data, uint32_t layer) {
  context_.beginDebugUtilsLabel(cmdBuffer, "Uploading image", {1.0f, 0.0f, 0.0f, 1.0f});

  stagingBuffer->copyDataToBuffer(
      data, pixelSizeInBytes() * extents_.width * extents_.height * extents_.depth);

  if (layout_ == VK_IMAGE_LAYOUT_UNDEFINED) {
    transitionImageLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  }

  const VkImageAspectFlags aspectMask =
      isDepth() ? isStencil() ? VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT
                              : VK_IMAGE_ASPECT_DEPTH_BIT
                : VK_IMAGE_ASPECT_COLOR_BIT;
  const VkBufferImageCopy bufCopy = {
      .bufferOffset = 0,
      .imageSubresource =
          {
              .aspectMask = aspectMask,
              .mipLevel = 0,
              .baseArrayLayer = layer,
              .layerCount = 1,
          },
      .imageOffset =
          {
              .x = 0,
              .y = 0,
              .z = 0,
          },
      .imageExtent = extents_,
  };
  vkCmdCopyBufferToImage(cmdBuffer, stagingBuffer->vkBuffer(), image_,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufCopy);
  context_.endDebugUtilsLabel(cmdBuffer);
}

void Texture::addReleaseBarrier(VkCommandBuffer cmdBuffer, uint32_t srcQueueFamilyIndex,
                                uint32_t dstQueueFamilyIndex) {
  VkImageMemoryBarrier2 releaseBarrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
      .srcQueueFamilyIndex = srcQueueFamilyIndex,
      .dstQueueFamilyIndex = dstQueueFamilyIndex,
      .image = image_,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels_, 0, 1},
  };

  VkDependencyInfo dependency_info{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &releaseBarrier,
  };

  vkCmdPipelineBarrier2(cmdBuffer, &dependency_info);
}

void Texture::addAcquireBarrier(VkCommandBuffer cmdBuffer, uint32_t srcQueueFamilyIndex,
                                uint32_t dstQueueFamilyIndex) {
  VkImageMemoryBarrier2 acquireBarrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
      .srcQueueFamilyIndex = srcQueueFamilyIndex,
      .dstQueueFamilyIndex = dstQueueFamilyIndex,
      .image = image_,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels_, 0, 1},
  };

  VkDependencyInfo dependency_info{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &acquireBarrier,
  };

  vkCmdPipelineBarrier2(cmdBuffer, &dependency_info);
}

void Texture::transitionImageLayout(VkCommandBuffer cmdBuffer, VkImageLayout newLayout) {
  VkAccessFlags srcAccessMask = VK_ACCESS_NONE;
  VkAccessFlags dstAccessMask = VK_ACCESS_NONE;
  VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

  constexpr VkPipelineStageFlags depthStageMask =
      0 | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
      VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

  constexpr VkPipelineStageFlags sampledStageMask =
      0 | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

  /*std::cerr << "[transImgeLayot] Transitioning image " << debugName_ << " from
     "
            << string_VkImageLayout(layout_) << " to " <<
     string_VkImageLayout(newLayout)
            << std::endl;*/

  auto oldLayout = layout_;

  if (oldLayout == newLayout) {
    return;
  }

  switch (oldLayout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      break;

    case VK_IMAGE_LAYOUT_GENERAL:
      sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
      srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      sourceStage = depthStageMask;
      srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
      sourceStage = depthStageMask | sampledStageMask;
      break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      sourceStage = sampledStageMask;
      break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
      break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
      srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_PREINITIALIZED:
      sourceStage = VK_PIPELINE_STAGE_HOST_BIT;
      srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      break;

    default:
      ASSERT(false, "Unknown image layout.");
      break;
  }

  switch (newLayout) {
    case VK_IMAGE_LAYOUT_GENERAL:
    case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
      destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
      dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      dstAccessMask =
          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      destinationStage = depthStageMask;
      dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
      destinationStage = depthStageMask | sampledStageMask;
      dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
      break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      destinationStage = sampledStageMask;
      dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
      break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
      dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
      dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      // vkQueuePresentKHR performs automatic visibility operations
      break;

    default:
      ASSERT(false, "Unknown image layout.");
      break;
  }

  const VkImageAspectFlags aspectMask =
      isDepth() ? isStencil() ? VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT
                              : VK_IMAGE_ASPECT_DEPTH_BIT
                : VK_IMAGE_ASPECT_COLOR_BIT;
  const VkImageMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = srcAccessMask,
      .dstAccessMask = dstAccessMask,
      .oldLayout = layout_,
      .newLayout = newLayout,
      .image = image_,
      .subresourceRange =
          {
              .aspectMask = aspectMask,
              .baseMipLevel = 0,
              .levelCount = mipLevels_,
              .baseArrayLayer = 0,
              .layerCount = multiview_ ? VK_REMAINING_ARRAY_LAYERS : 1,
          },
  };
  vkCmdPipelineBarrier(cmdBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  layout_ = newLayout;
}

bool Texture::isDepth() const {
  return (format_ == VK_FORMAT_D16_UNORM || format_ == VK_FORMAT_D16_UNORM_S8_UINT ||
          format_ == VK_FORMAT_D24_UNORM_S8_UINT || format_ == VK_FORMAT_D32_SFLOAT ||
          format_ == VK_FORMAT_D32_SFLOAT_S8_UINT ||
          format_ == VK_FORMAT_X8_D24_UNORM_PACK32);
}

bool Texture::isStencil() const {
  return (format_ == VK_FORMAT_S8_UINT || format_ == VK_FORMAT_D16_UNORM_S8_UINT ||
          format_ == VK_FORMAT_D24_UNORM_S8_UINT ||
          format_ == VK_FORMAT_D32_SFLOAT_S8_UINT);
}

uint32_t Texture::pixelSizeInBytes() const { return bytesPerPixel(format_); }

uint32_t Texture::numMipLevels() const { return mipLevels_; }

uint32_t Texture::getMipLevelsCount(uint32_t texWidth, uint32_t texHeight) const {
  return static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
}

void Texture::generateMips(VkCommandBuffer cmdBuffer) {
  if (!generateMips_) {
    return;
  }
  context_.beginDebugUtilsLabel(cmdBuffer, "Generate Mips", {0.0f, 1.0f, 0.0f, 1.0f});
  VkFormatProperties formatProperties;
  vkGetPhysicalDeviceFormatProperties(context_.physicalDevice().vkPhysicalDevice(),
                                      format_, &formatProperties);

  ASSERT(formatProperties.optimalTilingFeatures &
             VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT,
         "Device doesn't support linear blit, can't generate mips");

  const VkImageAspectFlags aspectMask =
      isDepth() ? isStencil() ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
                              : VK_IMAGE_ASPECT_DEPTH_BIT
                : VK_IMAGE_ASPECT_COLOR_BIT;

  VkImageMemoryBarrier barrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                               .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                               .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                               .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                               .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                               .image = image_,
                               .subresourceRange = {
                                   .aspectMask = aspectMask,
                                   .baseMipLevel = 0,
                                   .levelCount = 1,
                                   .baseArrayLayer = 0,
                                   .layerCount = 1,
                               }};

  int32_t mipWidth = extents_.width;
  int32_t mipHeight = extents_.height;

  for (uint32_t i = 1; i <= mipLevels_; i++) {
    barrier.subresourceRange.baseMipLevel = i - 1;
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);

    if (i == mipLevels_) {
      break;
    }

    const int32_t newMipWidth = mipWidth > 1 ? mipWidth >> 1 : mipWidth;
    const int32_t newMipHeight = mipHeight > 1 ? mipHeight >> 1 : mipHeight;

    VkImageBlit blit{
        .srcSubresource =
            {
                .aspectMask = aspectMask,
                .mipLevel = i - 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .srcOffsets = {{
                           0,
                           0,
                           0,
                       },
                       {
                           mipWidth,
                           mipHeight,
                           1,
                       }},
        .dstSubresource =
            {
                .aspectMask = aspectMask,
                .mipLevel = i,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .dstOffsets = {{
                           0,
                           0,
                           0,
                       },
                       {
                           newMipWidth,
                           newMipHeight,
                           1,
                       }},
    };

    vkCmdBlitImage(cmdBuffer, image_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image_,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

    mipWidth = newMipWidth;
    mipHeight = newMipHeight;
  }

  const VkImageMemoryBarrier finalBarrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image_,
      .subresourceRange =
          {
              .aspectMask = aspectMask,
              .baseMipLevel = 0,
              .levelCount = VK_REMAINING_MIP_LEVELS,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },

  };

  vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                       1, &finalBarrier);

  layout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  context_.endDebugUtilsLabel(cmdBuffer);
}

std::vector<std::shared_ptr<VkImageView>> Texture::generateViewForEachMips() {
  std::vector<std::shared_ptr<VkImageView>> output;
  output.reserve(mipLevels_);

  for (int i = 0; i < mipLevels_; ++i) {
    const VkImageAspectFlags aspectMask =
        isDepth() ? (isStencil() ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
                                 : VK_IMAGE_ASPECT_DEPTH_BIT)
                  : VK_IMAGE_ASPECT_COLOR_BIT;
    const VkImageViewCreateInfo imageViewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image_,
        .viewType = viewType_,
        .format = format_,
        .components =
            {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
        .subresourceRange = {
            .aspectMask = aspectMask,
            .baseMipLevel = uint32_t(i),
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = layerCount_,
        }};

    output.push_back(std::make_shared<VkImageView>(VK_NULL_HANDLE));

    VK_CHECK(vkCreateImageView(context_.device(), &imageViewInfo, nullptr,
                               output.back().get()));
    context_.setVkObjectname(imageView_, VK_OBJECT_TYPE_IMAGE_VIEW, "Image view per mip");
  }
  return output;
}

VkSampleCountFlagBits Texture::VkSampleCount() const { return msaaSamples_; }

VkImageView Texture::createImageView(const Context& context, VkImageViewType viewType,
                                     VkFormat format, uint32_t numMipLevels,
                                     uint32_t layers, const std::string& name) {
  // ASSERT(isDepth() ^ isStencil(),
  //        "It's illegal to create an image view with both the depth and
  //        stencil bits. You " "can only use one");

  const VkImageAspectFlags aspectMask =
      isDepth() ? VK_IMAGE_ASPECT_DEPTH_BIT
                : (isStencil() ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_COLOR_BIT);
  const VkImageViewCreateInfo imageViewInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .flags = /*usageFlags_ &
               VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT ?
                   VK_IMAGE_VIEW_CREATE_FRAGMENT_DENSITY_MAP_DYNAMIC_BIT_EXT
                   :*/
      VkImageViewCreateFlags(0),
      .image = image_,
      .viewType = viewType,
      .format = format,
      .components =
          {
              .r = VK_COMPONENT_SWIZZLE_IDENTITY,
              .g = VK_COMPONENT_SWIZZLE_IDENTITY,
              .b = VK_COMPONENT_SWIZZLE_IDENTITY,
              .a = VK_COMPONENT_SWIZZLE_IDENTITY,
          },
      .subresourceRange = {
          .aspectMask = aspectMask,
          .baseMipLevel = 0,
          .levelCount = numMipLevels,
          .baseArrayLayer = 0,
          .layerCount = multiview_ ? VK_REMAINING_ARRAY_LAYERS : layers,
      }};

  VkImageView imageView{VK_NULL_HANDLE};
  VK_CHECK(vkCreateImageView(context_.device(), &imageViewInfo, nullptr, &imageView));
  context.setVkObjectname(imageView, VK_OBJECT_TYPE_IMAGE_VIEW, "Image view: " + name);

  return imageView;
}

}  // namespace VulkanCore
