#include "LightingPassHybridRenderer.hpp"

#include <filesystem>

constexpr uint32_t GBUFFERDATA_SET = 0;

constexpr uint32_t BINDING_WORLDNORMAL = 0;
constexpr uint32_t BINDING_SPECULAR = 1;
constexpr uint32_t BINDING_BASECOLOR = 2;
constexpr uint32_t BINDING_POSITION = 3;
constexpr uint32_t BINDING_RAYTRACEDSHADOW = 4;

constexpr uint32_t TRANSFORM_LIGHT_DATA_SET = 1;
constexpr uint32_t BINDING_TRANSFORM = 0;
constexpr uint32_t BINDING_LIGHT = 1;

struct Transforms {
  glm::aligned_mat4 viewProj;
  glm::aligned_mat4 viewProjInv;
  glm::aligned_mat4 viewInv;
};

LightingPassHybridRenderer::LightingPassHybridRenderer() {}

void LightingPassHybridRenderer::init(
    VulkanCore::Context* context, std::shared_ptr<VulkanCore::Texture> gBufferNormal,
    std::shared_ptr<VulkanCore::Texture> gBufferSpecular,
    std::shared_ptr<VulkanCore::Texture> gBufferBaseColor,
    std::shared_ptr<VulkanCore::Texture> gBufferPosition,
    std::shared_ptr<VulkanCore::Texture> shadowRayTraced) {
  context_ = context;
  width_ = context->swapchain()->extent().width;
  height_ = context->swapchain()->extent().height;
  gBufferNormal_ = gBufferNormal;
  gBufferSpecular_ = gBufferSpecular;
  gBufferBaseColor_ = gBufferBaseColor;
  gBufferPosition_ = gBufferPosition;
  shadowRayTraced_ = shadowRayTraced;

  sampler_ = context_->createSampler(
      VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      100.0f, "lighting pass default sampler");

  outLightingTexture_ =
      context->createTexture(VK_IMAGE_TYPE_2D, VK_FORMAT_B8G8R8A8_UNORM, 0,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                             {
                                 .width = context->swapchain()->extent().width,
                                 .height = context->swapchain()->extent().height,
                                 .depth = 1,
                             },
                             1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false,
                             VK_SAMPLE_COUNT_1_BIT, "Lighting Pass HDR Buffer");

  cameraBuffer_ = context_->createPersistentBuffer(
      sizeof(Transforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      "LightingPass CameraData Uniform buffer");

  lightBuffer_ = context_->createPersistentBuffer(
      sizeof(LightData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      "LightingPass LightDaat Uniform buffer");

  renderPass_ = context->createRenderPass(
      {outLightingTexture_}, {VK_ATTACHMENT_LOAD_OP_CLEAR},
      {VK_ATTACHMENT_STORE_OP_STORE},
      // final layout for all attachments
      {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}, VK_PIPELINE_BIND_POINT_GRAPHICS, {},
      "LightingPass RenderPass");

  frameBuffer_ =
      context->createFramebuffer(renderPass_->vkRenderPass(), {outLightingTexture_},
                                 nullptr, nullptr, "LightingPass framebuffer");

  const auto resourcesFolder = std::filesystem::current_path() / "resources/shaders/";

  auto vertexShader =
      context->createShaderModule((resourcesFolder / "fullscreen.vert").string(),
                                  VK_SHADER_STAGE_VERTEX_BIT, "lighting vertex");

  auto fragmentShader = context->createShaderModule(
      (resourcesFolder / "hybridRenderer_lighting_composite.frag").string(),
      VK_SHADER_STAGE_FRAGMENT_BIT, "lighting fragment");

  const std::vector<VulkanCore::Pipeline::SetDescriptor> setLayout = {
      {
          .set_ = GBUFFERDATA_SET,  // set number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding{BINDING_WORLDNORMAL,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               1, VK_SHADER_STAGE_FRAGMENT_BIT},
                  VkDescriptorSetLayoutBinding{BINDING_SPECULAR,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               1, VK_SHADER_STAGE_FRAGMENT_BIT},
                  VkDescriptorSetLayoutBinding{BINDING_BASECOLOR,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               1, VK_SHADER_STAGE_FRAGMENT_BIT},
                  VkDescriptorSetLayoutBinding{BINDING_POSITION,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               1, VK_SHADER_STAGE_FRAGMENT_BIT},
                  VkDescriptorSetLayoutBinding{BINDING_RAYTRACEDSHADOW,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               1, VK_SHADER_STAGE_FRAGMENT_BIT},
              },
      },
      {
          .set_ = TRANSFORM_LIGHT_DATA_SET,  // set number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding{BINDING_TRANSFORM,
                                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                               VK_SHADER_STAGE_FRAGMENT_BIT},
                  VkDescriptorSetLayoutBinding{BINDING_LIGHT,
                                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                               VK_SHADER_STAGE_FRAGMENT_BIT},
              },
      },
  };
  const VulkanCore::Pipeline::GraphicsPipelineDescriptor gpDesc = {
      .sets_ = setLayout,
      .vertexShader_ = vertexShader,
      .fragmentShader_ = fragmentShader,
      .dynamicStates_ = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR},
      .colorTextureFormats = {VK_FORMAT_B8G8R8A8_UNORM},
      .primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
      .sampleCount = VK_SAMPLE_COUNT_1_BIT,
      .cullMode = VK_CULL_MODE_NONE,
      .viewport = context->swapchain()->extent(),
      .depthTestEnable = false,
      .depthWriteEnable = false,
      .depthCompareOperation = VK_COMPARE_OP_ALWAYS,
  };

  pipeline_ = context->createGraphicsPipeline(gpDesc, renderPass_->vkRenderPass(),
                                              "Lighting pipeline");

  pipeline_->allocateDescriptors({
      {.set_ = GBUFFERDATA_SET, .count_ = 1},
      {.set_ = TRANSFORM_LIGHT_DATA_SET, .count_ = 1},
  });

  pipeline_->bindResource(GBUFFERDATA_SET, BINDING_WORLDNORMAL, 0, gBufferNormal_,
                          sampler_);

  pipeline_->bindResource(GBUFFERDATA_SET, BINDING_SPECULAR, 0, gBufferSpecular_,
                          sampler_);

  pipeline_->bindResource(GBUFFERDATA_SET, BINDING_BASECOLOR, 0, gBufferBaseColor_,
                          sampler_);

  pipeline_->bindResource(GBUFFERDATA_SET, BINDING_POSITION, 0, gBufferPosition_,
                          sampler_);

  pipeline_->bindResource(GBUFFERDATA_SET, BINDING_RAYTRACEDSHADOW, 0, shadowRayTraced_,
                          sampler_);

  pipeline_->bindResource(TRANSFORM_LIGHT_DATA_SET, BINDING_TRANSFORM, 0, cameraBuffer_,
                          0, sizeof(Transforms), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

  pipeline_->bindResource(TRANSFORM_LIGHT_DATA_SET, BINDING_LIGHT, 0, lightBuffer_, 0,
                          sizeof(LightData), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

void LightingPassHybridRenderer::render(VkCommandBuffer commandBuffer, uint32_t index,
                                        const LightData& data, const glm::mat4& viewMat,
                                        const glm::mat4& projMat) {
  glm::mat4 viewProjMat = projMat * viewMat;
  Transforms transform;
  transform.viewProj = viewProjMat;
  transform.viewProjInv = glm::inverse(viewProjMat);
  transform.viewInv = glm::inverse(viewMat);
  cameraBuffer_->copyDataToBuffer(&transform, sizeof(Transforms));

  lightBuffer_->copyDataToBuffer(&data, sizeof(LightData));

  const std::array<VkClearValue, 1> clearValues = {
      VkClearValue{.color = {0.0, 1.0, 0.0, 0.0f}}};

  const VkRenderPassBeginInfo renderpassInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = renderPass_->vkRenderPass(),
      .framebuffer = frameBuffer_->vkFramebuffer(),
      .renderArea = {.offset =
                         {
                             0,
                             0,
                         },
                     .extent =
                         {
                             .width = width_,
                             .height = height_,
                         }},
      .clearValueCount = static_cast<uint32_t>(clearValues.size()),
      .pClearValues = clearValues.data(),
  };

  context_->beginDebugUtilsLabel(commandBuffer, "Hybrid Lighting Pass",
                                 {0.0f, 0.0f, 1.0f, 1.0f});

  vkCmdBeginRenderPass(commandBuffer, &renderpassInfo, VK_SUBPASS_CONTENTS_INLINE);

  const VkViewport viewport = {
      .x = 0.0f,
      .y = static_cast<float>(height_),
      .width = static_cast<float>(width_),
      .height = -static_cast<float>(height_),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
  const VkRect2D scissor = {
      .offset =
          {
              0,
              0,
          },
      .extent = VkExtent2D{width_, height_},
  };
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

  pipeline_->bind(commandBuffer);
  pipeline_->bindDescriptorSets(
      commandBuffer, {
                         {.set = GBUFFERDATA_SET, .bindIdx = (uint32_t)0},
                         {.set = TRANSFORM_LIGHT_DATA_SET, .bindIdx = (uint32_t)0},
                     });
  pipeline_->updateDescriptorSets();

  vkCmdDraw(commandBuffer, 4, 1, 0, 0);

  vkCmdEndRenderPass(commandBuffer);
  context_->endDebugUtilsLabel(commandBuffer);

  outLightingTexture_->setImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
