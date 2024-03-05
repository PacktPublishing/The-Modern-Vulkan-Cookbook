#include "TAAComputePass.hpp"

#include <filesystem>

#include "vulkancore/Context.hpp"
#include "vulkancore/Texture.hpp"

constexpr uint32_t OUTPUT_IMG_SET = 0;
constexpr uint32_t OUTPUT_IMAGE_BINDING = 0;

constexpr uint32_t INPUT_DATA_SET = 1;
constexpr uint32_t INPUT_DEPTH_BUFFER_BINDING = 0;
constexpr uint32_t INPUT_HISTORY_BUFFER_BINDING = 1;
constexpr uint32_t INPUT_VELOCITY_BUFFER_BINDING = 2;
constexpr uint32_t INPUT_COLOR_BUFFER_BINDING = 3;

void TAAComputePass::init(VulkanCore::Context* context,
                          std::shared_ptr<VulkanCore::Texture> depthTexture,
                          std::shared_ptr<VulkanCore::Texture> velocityTexture,
                          std::shared_ptr<VulkanCore::Texture> colorTexture) {
  context_ = context;

  depthTexture_ = depthTexture;
  velocityTexture_ = velocityTexture;
  colorTexture_ = colorTexture;

  sampler_ = context_->createSampler(
      VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      100.0f, "default sampler");

  pointSampler_ = context_->createSampler(
      VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      100.0f, "default point sampler");

  outColorTexture_ =
      context_->createTexture(VK_IMAGE_TYPE_2D, VK_FORMAT_R16G16B16A16_SFLOAT, 0,
                              VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                              VkExtent3D{
                                  .width = context_->swapchain()->extent().width,
                                  .height = context_->swapchain()->extent().height,
                                  .depth = 1u,
                              },
                              1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true,
                              VK_SAMPLE_COUNT_1_BIT, "Output TAA Pass ColorTexture");

  historyTexture_ =
      context_->createTexture(VK_IMAGE_TYPE_2D, VK_FORMAT_R16G16B16A16_SFLOAT, 0,
                              VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                              VkExtent3D{
                                  .width = context_->swapchain()->extent().width,
                                  .height = context_->swapchain()->extent().height,
                                  .depth = 1u,
                              },
                              1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true,
                              VK_SAMPLE_COUNT_1_BIT, "TAA Pass HistoryTexture");

  const auto resourcesFolder = std::filesystem::current_path() / "resources/shaders/";

  auto shader =
      context->createShaderModule((resourcesFolder / "taaresolve.comp").string(),
                                  VK_SHADER_STAGE_COMPUTE_BIT, "TAA Compute Shader");

  const std::vector<VulkanCore::Pipeline::SetDescriptor> setLayout = {
      {
          .set_ = OUTPUT_IMG_SET,
          .bindings_ =
              {
                  VkDescriptorSetLayoutBinding{OUTPUT_IMAGE_BINDING,
                                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                                               VK_SHADER_STAGE_COMPUTE_BIT},
              },
      },
      {
          .set_ = INPUT_DATA_SET,
          .bindings_ =
              {
                  VkDescriptorSetLayoutBinding{INPUT_DEPTH_BUFFER_BINDING,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               1, VK_SHADER_STAGE_COMPUTE_BIT},
                  VkDescriptorSetLayoutBinding{INPUT_HISTORY_BUFFER_BINDING,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               1, VK_SHADER_STAGE_COMPUTE_BIT},
                  VkDescriptorSetLayoutBinding{INPUT_VELOCITY_BUFFER_BINDING,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               1, VK_SHADER_STAGE_COMPUTE_BIT},
                  VkDescriptorSetLayoutBinding{INPUT_COLOR_BUFFER_BINDING,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               1, VK_SHADER_STAGE_COMPUTE_BIT},
              },
      },
  };
  std::vector<VkPushConstantRange> pushConstants = {
      VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          .offset = 0,
          .size = sizeof(TAAPushConstants),
      },
  };
  const VulkanCore::Pipeline::ComputePipelineDescriptor desc = {
      .sets_ = setLayout,
      .computeShader_ = shader,
      .pushConstants_ = pushConstants,
  };
  pipeline_ = context->createComputePipeline(desc, "TAA Pipeline");

  pipeline_->allocateDescriptors({
      {.set_ = OUTPUT_IMG_SET, .count_ = 1},
      {.set_ = INPUT_DATA_SET, .count_ = 1},
  });

  pipeline_->bindResource(OUTPUT_IMG_SET, OUTPUT_IMAGE_BINDING, 0, outColorTexture_,
                          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

  pipeline_->bindResource(INPUT_DATA_SET, INPUT_DEPTH_BUFFER_BINDING, 0, depthTexture_,
                          pointSampler_);

  pipeline_->bindResource(INPUT_DATA_SET, INPUT_HISTORY_BUFFER_BINDING, 0,
                          historyTexture_, sampler_);

  pipeline_->bindResource(INPUT_DATA_SET, INPUT_VELOCITY_BUFFER_BINDING, 0,
                          velocityTexture_, sampler_);

  pipeline_->bindResource(INPUT_DATA_SET, INPUT_COLOR_BUFFER_BINDING, 0, colorTexture_,
                          pointSampler_);

  initSharpenPipeline();
}

void TAAComputePass::doAA(VkCommandBuffer cmd, int frameIndex, int isCamMoving) {
  context_->beginDebugUtilsLabel(cmd, "TAA Main pass", {1.0f, 0.0f, 0.0f, 1.0f});

  TAAPushConstants pushConst{
      .isFirstFrame = uint32_t(frameIndex),
      .isCameraMoving = uint32_t(isCamMoving),
  };

  pipeline_->bind(cmd);

  pipeline_->updatePushConstant(cmd, VK_SHADER_STAGE_COMPUTE_BIT,
                                sizeof(TAAPushConstants), &pushConst);

  pipeline_->bindDescriptorSets(cmd, {
                                         {.set = OUTPUT_IMG_SET, .bindIdx = 0},
                                         {.set = INPUT_DATA_SET, .bindIdx = 0},
                                     });
  pipeline_->updateDescriptorSets();

  outColorTexture_->transitionImageLayout(cmd, VK_IMAGE_LAYOUT_GENERAL);
  historyTexture_->transitionImageLayout(cmd, VK_IMAGE_LAYOUT_GENERAL);

  vkCmdDispatch(cmd, (outColorTexture_->vkExtents().width + 15) / 16,
                (outColorTexture_->vkExtents().height + 15) / 16, 1);

  context_->endDebugUtilsLabel(cmd);

  context_->beginDebugUtilsLabel(cmd, "TAA Sharpen pass", {1.0f, 1.0f, 0.0f, 1.0f});

  VkImageMemoryBarrier barriers[2] = {
      {
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
          .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
          .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
          .newLayout = VK_IMAGE_LAYOUT_GENERAL,
          .image = outColorTexture_->vkImage(),
          .subresourceRange =
              {
                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  .levelCount = 1,
                  .layerCount = 1,
              },
      },
      {

          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
          .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
          .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
          .newLayout = VK_IMAGE_LAYOUT_GENERAL,
          .image = historyTexture_->vkImage(),
          .subresourceRange =
              {
                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  .levelCount = 1,
                  .layerCount = 1,
              },
      },
  };

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 2,
                       barriers);

  colorTexture_->transitionImageLayout(cmd, VK_IMAGE_LAYOUT_GENERAL);

  sharpenPipeline_->bind(cmd);

  sharpenPipeline_->bindDescriptorSets(cmd, {
                                                {.set = 0, .bindIdx = 0},
                                                {.set = 1, .bindIdx = 0},
                                            });
  sharpenPipeline_->updateDescriptorSets();

  vkCmdDispatch(cmd, (outColorTexture_->vkExtents().width + 15) / 16,
                (outColorTexture_->vkExtents().height + 15) / 16, 1);

  colorTexture_->transitionImageLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  outColorTexture_->transitionImageLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  context_->endDebugUtilsLabel(cmd);
}

void TAAComputePass::initSharpenPipeline() {
  const auto resourcesFolder = std::filesystem::current_path() / "resources/shaders/";

  auto shader = context_->createShaderModule(
      (resourcesFolder / "taahistorycopyandsharpen.comp").string(),
      VK_SHADER_STAGE_COMPUTE_BIT, "TAA Sharpen Compute Shader");

  const std::vector<VulkanCore::Pipeline::SetDescriptor> setLayout = {
      {
          .set_ = 0,
          .bindings_ =
              {
                  VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                                               VK_SHADER_STAGE_COMPUTE_BIT},
                  VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                                               VK_SHADER_STAGE_COMPUTE_BIT},
              },
      },
      {
          .set_ = 1,
          .bindings_ =
              {
                  VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                                               VK_SHADER_STAGE_COMPUTE_BIT},
              },
      },
  };

  const VulkanCore::Pipeline::ComputePipelineDescriptor desc = {
      .sets_ = setLayout,
      .computeShader_ = shader,
  };
  sharpenPipeline_ = context_->createComputePipeline(desc, "TAA Sharpen Pipeline");

  sharpenPipeline_->allocateDescriptors({
      {.set_ = 0, .count_ = 1},
      {.set_ = 1, .count_ = 1},
  });

  sharpenPipeline_->bindResource(0, 0, 0, colorTexture_,
                                 VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

  sharpenPipeline_->bindResource(0, 1, 0, historyTexture_,
                                 VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

  sharpenPipeline_->bindResource(1, 0, 0, outColorTexture_,
                                 VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
}
