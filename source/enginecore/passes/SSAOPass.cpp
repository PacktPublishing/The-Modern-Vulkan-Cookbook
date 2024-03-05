#include "SSAOPass.hpp"

#include <filesystem>

constexpr uint32_t SSAO_OUTPUT_SET = 0;
constexpr uint32_t BINDING_OUT_SSAO = 0;

constexpr uint32_t INPUT_TEXTURES_SET = 1;
constexpr uint32_t BINDING_GBUFFER_DEPTH = 0;

struct PushConst {
  glm::uvec2 resolution;
  uint32_t frameIndex;
};

SSAOPass::SSAOPass() {}

SSAOPass::~SSAOPass() {}

void SSAOPass::init(VulkanCore::Context* context,
                    std::shared_ptr<VulkanCore::Texture> gBufferDepth) {
  context_ = context;
  gBufferDepth_ = gBufferDepth;

  sampler_ = context_->createSampler(
      VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      100.0f, "default sampler");

  outSSAOTexture_ = context_->createTexture(
      VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, 0,
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
      VkExtent3D{
          .width = context_->swapchain()->extent().width,
          .height = context_->swapchain()->extent().height,
          .depth = 1u,
      },
                              1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true,
                              VK_SAMPLE_COUNT_1_BIT, "SSAO texture");

  const auto resourcesFolder = std::filesystem::current_path() / "resources/shaders/";

  auto shader =
      context->createShaderModule((resourcesFolder / "ssao.comp").string(),
                                  VK_SHADER_STAGE_COMPUTE_BIT, "SSAO compute shader");

  const std::vector<VulkanCore::Pipeline::SetDescriptor> setLayout = {
      {
          .set_ = SSAO_OUTPUT_SET,
          .bindings_ =
              {
                  VkDescriptorSetLayoutBinding{BINDING_OUT_SSAO,
                                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                                               VK_SHADER_STAGE_COMPUTE_BIT},
              },
      },
      {
          .set_ = INPUT_TEXTURES_SET,
          .bindings_ =
              {
                  VkDescriptorSetLayoutBinding{BINDING_GBUFFER_DEPTH,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               1, VK_SHADER_STAGE_COMPUTE_BIT},
              },
      },
  };
  std::vector<VkPushConstantRange> pushConstants = {
      VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          .offset = 0,
          .size = sizeof(PushConst),
      },
  };

  const VulkanCore::Pipeline::ComputePipelineDescriptor desc = {
      .sets_ = setLayout,
      .computeShader_ = shader,
      .pushConstants_ = pushConstants,
  };
  pipeline_ = context->createComputePipeline(desc, "main");

  pipeline_->allocateDescriptors({
      {.set_ = SSAO_OUTPUT_SET, .count_ = 1},
      {.set_ = INPUT_TEXTURES_SET, .count_ = 1},
  });

  pipeline_->bindResource(SSAO_OUTPUT_SET, BINDING_OUT_SSAO, 0, outSSAOTexture_,
                          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

  pipeline_->bindResource(INPUT_TEXTURES_SET, BINDING_GBUFFER_DEPTH, 0, gBufferDepth_,
                          sampler_);
}

void SSAOPass::run(VkCommandBuffer cmd) {
  context_->beginDebugUtilsLabel(cmd, "SSAO Pass", {0.5f, 0.5f, 0.0f, 1.0f});

  pipeline_->bind(cmd);

  PushConst pushConst{
      .resolution = glm::uvec2(context_->swapchain()->extent().width,
                               context_->swapchain()->extent().height),
      .frameIndex = 0,
  };

  pipeline_->updatePushConstant(cmd, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(PushConst),
                                &pushConst);

  pipeline_->bindDescriptorSets(
      cmd, {
               {.set = SSAO_OUTPUT_SET, .bindIdx = BINDING_OUT_SSAO},
               {.set = INPUT_TEXTURES_SET, .bindIdx = BINDING_GBUFFER_DEPTH},
           });
  pipeline_->updateDescriptorSets();

  outSSAOTexture_->transitionImageLayout(cmd, VK_IMAGE_LAYOUT_GENERAL);

  vkCmdDispatch(cmd, pushConst.resolution.x / 16 + 1, pushConst.resolution.y / 16 + 1, 1);

  outSSAOTexture_->transitionImageLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  context_->endDebugUtilsLabel(cmd);
}
