#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <array>
#include <filesystem>
#include <gli/gli.hpp>
#include <glm/glm.hpp>

#include "FontManager.hpp"
#include "enginecore/Camera.hpp"
#include "enginecore/FPSCounter.hpp"
#include "enginecore/GLBLoader.hpp"
#include "enginecore/GLFWUtils.hpp"
#include "enginecore/Model.hpp"
#include "enginecore/RingBuffer.hpp"
#include "vulkancore/Buffer.hpp"
#include "vulkancore/CommandQueueManager.hpp"
#include "vulkancore/Context.hpp"
#include "vulkancore/Framebuffer.hpp"
#include "vulkancore/Pipeline.hpp"
#include "vulkancore/RenderPass.hpp"
#include "vulkancore/Texture.hpp"

struct CharInstance {
  glm::vec4 bbox;
  uint32_t glyphIndex;
  float sharpness;
};

struct GlyphInfo {
  glm::vec4 bbox;
  glm::uvec4 cellInfo;
};

GLFWwindow* window_ = nullptr;
EngineCore::Camera camera(glm::vec3(0, 100, -370), glm::vec3(0, 50, 0),
                          glm::vec3(0, -1, 0), 0.1f, 1000.f, 1600.f / 1200.f);

int main(int argc, char* argv[]) {
  initWindow(&window_, &camera);

#pragma region Context initialization
  const std::vector<std::string> instExtension = {
      VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
  };

  const std::vector<std::string> deviceExtension = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };

  std::vector<std::string> validationLayers;
#ifdef _DEBUG
  validationLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif

  VulkanCore::Context::enableDefaultFeatures();
  VulkanCore::Context::enableIndirectRenderingFeature();
  VulkanCore::Context::enableBufferDeviceAddressFeature();

  VulkanCore::Context context(
      (void*)glfwGetWin32Window(window_),
      validationLayers,  // layers
      instExtension,     // instance extensions
      deviceExtension,   // device extensions
      VkQueueFlags(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT),
      true);
#pragma endregion

#pragma region Swapchain initialization
  const VkExtent2D extents =
      context.physicalDevice().surfaceCapabilities().minImageExtent;

  const VkFormat swapChainFormat = VK_FORMAT_B8G8R8A8_SRGB;

  context.createSwapchain(swapChainFormat, VK_COLORSPACE_SRGB_NONLINEAR_KHR,
                          VK_PRESENT_MODE_MAILBOX_KHR, extents);

  static const uint32_t framesInFlight = (uint32_t)context.swapchain()->numberImages();
#pragma endregion

  // Create command pools
  auto commandMgr = context.createGraphicsCommandQueue(
      context.swapchain()->numberImages(), framesInFlight,
      "main "
      "comman"
      "d");

  const auto fontsFolder = std::filesystem::path("C:/windows/fonts");

  FontManager fontManager;
  const auto& glyphData = fontManager.loadFont((fontsFolder / "times.ttf").string());

  std::vector<GlyphInfo> glyhInfoData;

  std::vector<uint32_t> cellsData;
  std::vector<glm::vec2> pointsData;

  uint32_t pointOffset = 0;
  uint32_t cellOffset = 0;

  for (const auto& glyph : glyphData) {
    glyhInfoData.push_back({
        glyph.bbox,
        glm::uvec4(pointOffset, cellOffset, glyph.cellX, glyph.cellY),
    });

    cellsData.insert(cellsData.end(), glyph.cellData.begin(), glyph.cellData.end());
    pointsData.insert(pointsData.end(), glyph.points.begin(), glyph.points.end());
    pointOffset += glyph.points.size();
    cellOffset += glyph.cellData.size();
  }

  const std::string textToDisplay = "GPUSDFTEXTDEMO";

  std::vector<CharInstance> charsData(textToDisplay.length());

  int startX = context.swapchain()->extent().width / 6.0;
  const int startY = context.swapchain()->extent().height / 2.0;

  constexpr float scale = .09;

  for (int i = 0; i < textToDisplay.length(); ++i) {
    int glpyIndex = textToDisplay[i] - 'A';
    charsData[i].glyphIndex = glpyIndex;
    charsData[i].sharpness = scale;
    charsData[i].bbox.x = (startX + glyphData[glpyIndex].bbox.x * scale) /
                              (context.swapchain()->extent().width / 2.0) -
                          1.0;

    charsData[i].bbox.y = (startY - glyphData[glpyIndex].bbox.y * scale) /
                              (context.swapchain()->extent().height / 2.0) -
                          1.0;

    charsData[i].bbox.z = (startX + glyphData[glpyIndex].bbox.z * scale) /
                              (context.swapchain()->extent().width / 2.0) -
                          1.0;

    charsData[i].bbox.w = (startY - glyphData[glpyIndex].bbox.w * scale) /
                              (context.swapchain()->extent().height / 2.0) -
                          1.0;

    startX += glyphData[glpyIndex].horizontalAdvance * scale;
  }

  auto glyphInfoBuffer = context.createBuffer(
      sizeof(GlyphInfo) * glyhInfoData.size(),
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, "glyph buffer");

  auto cellsBuffer = context.createBuffer(
      sizeof(uint32_t) * cellsData.size(),
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, "cells buffer");

  auto pointsBuffer = context.createBuffer(
      sizeof(glm::vec2) * pointsData.size(),
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, "points buffer");

  auto charInstanceBuffer = context.createBuffer(
      sizeof(CharInstance) * charsData.size(),
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, "chars buffer");

  const auto commandBuffer = commandMgr.getCmdBufferToBegin();

  context.uploadToGPUBuffer(commandMgr, commandBuffer, glyphInfoBuffer.get(),
                            reinterpret_cast<const void*>(glyhInfoData.data()),
                            sizeof(GlyphInfo) * glyhInfoData.size());

  context.uploadToGPUBuffer(commandMgr, commandBuffer, cellsBuffer.get(),
                            reinterpret_cast<const void*>(cellsData.data()),
                            sizeof(uint32_t) * cellsData.size());

  context.uploadToGPUBuffer(commandMgr, commandBuffer, pointsBuffer.get(),
                            reinterpret_cast<const void*>(pointsData.data()),
                            sizeof(glm::vec2) * pointsData.size());

  context.uploadToGPUBuffer(commandMgr, commandBuffer, charInstanceBuffer.get(),
                            reinterpret_cast<const void*>(charsData.data()),
                            sizeof(CharInstance) * charsData.size());

  commandMgr.endCmdBuffer(commandBuffer);

  VkPipelineStageFlags flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
  const auto submitInfo =
      context.swapchain()->createSubmitInfo(&commandBuffer, &flags, false, false);
  commandMgr.submit(&submitInfo);
  commandMgr.waitUntilSubmitIsComplete();

  constexpr uint32_t GLYPH_INFO_STORAGE_SET = 0;
  constexpr uint32_t CELLS_STORAGE_SET = 1;
  constexpr uint32_t POINTS_STORAGE_SET = 2;
  constexpr uint32_t BINDING_0 = 0;

  std::shared_ptr<VulkanCore::Pipeline> pipeline;

#pragma region DepthTexture
  auto depthTexture =
      context.createTexture(VK_IMAGE_TYPE_2D, VK_FORMAT_D24_UNORM_S8_UINT, 0,
                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                            {
                                .width = context.swapchain()->extent().width,
                                .height = context.swapchain()->extent().height,
                                .depth = 1,
                            },
                            1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false,
                            VK_SAMPLE_COUNT_1_BIT, "depth buffer");
#pragma endregion

  const auto shadersFolder = std::filesystem::current_path() / "resources/shaders/";

  auto vertexShader = context.createShaderModule(
      (shadersFolder.string() + "font.vert"), VK_SHADER_STAGE_VERTEX_BIT, "main vertex");
  auto fragmentShader =
      context.createShaderModule((shadersFolder.string() + "font.frag"),
                                 VK_SHADER_STAGE_FRAGMENT_BIT, "main fragment");

  const std::vector<VulkanCore::Pipeline::SetDescriptor> setLayout = {
      {
          .set_ = GLYPH_INFO_STORAGE_SET,  // set number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding(BINDING_0,
                                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                               VK_SHADER_STAGE_VERTEX_BIT),
              },
      },
      {
          .set_ = CELLS_STORAGE_SET,  // set number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding(BINDING_0,
                                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                               VK_SHADER_STAGE_FRAGMENT_BIT),
              },
      },
      {
          .set_ = POINTS_STORAGE_SET,  // set number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding(BINDING_0,
                                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                               VK_SHADER_STAGE_FRAGMENT_BIT),
              },
      },
  };

  const VkVertexInputBindingDescription bindingDesc = {
      .binding = 0,
      .stride = sizeof(CharInstance),
      .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
  };

  const std::vector<std::pair<VkFormat, size_t>> vertexAttributesFormatAndOffset = {
      {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(CharInstance, bbox)},
      {VK_FORMAT_R32_UINT, offsetof(CharInstance, glyphIndex)},
      {VK_FORMAT_R32_SFLOAT, offsetof(CharInstance, sharpness)}};

  std::vector<VkVertexInputAttributeDescription> vertexInputAttributes;

  for (uint32_t i = 0; i < vertexAttributesFormatAndOffset.size(); ++i) {
    auto [format, offset] = vertexAttributesFormatAndOffset[i];
    vertexInputAttributes.push_back(VkVertexInputAttributeDescription{
        .location = i,
        .binding = 0,
        .format = format,
        .offset = uint32_t(offset),
    });
  }

  const VulkanCore::Pipeline::GraphicsPipelineDescriptor gpDesc = {
      .sets_ = setLayout,
      .vertexShader_ = vertexShader,
      .fragmentShader_ = fragmentShader,
      .colorTextureFormats = {swapChainFormat},
      .depthTextureFormat = depthTexture->vkFormat(),
      .primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
      .sampleCount = VK_SAMPLE_COUNT_1_BIT,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .viewport = context.swapchain()->extent(),
      .blendEnable = true,
      .depthTestEnable = false,
      .depthWriteEnable = false,
      .vertexInputCreateInfo =
          {
              .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
              .vertexBindingDescriptionCount = 1u,
              .pVertexBindingDescriptions = &bindingDesc,
              .vertexAttributeDescriptionCount = uint32_t(vertexInputAttributes.size()),
              .pVertexAttributeDescriptions = vertexInputAttributes.data(),
          },
  };

  std::vector<std::unique_ptr<VulkanCore::Framebuffer>> swapchain_framebuffers(
      context.swapchain()->numberImages());

#pragma region Render Pass Initialization
  std::shared_ptr<VulkanCore::RenderPass> renderPass = context.createRenderPass(
      {context.swapchain()->texture(0), depthTexture},
      {VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_CLEAR},
      {VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_STORE_OP_DONT_CARE},
      {VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
      VK_PIPELINE_BIND_POINT_GRAPHICS, {}, "swapchain render pass");
#pragma endregion

#pragma region Swapchain Framebuffers Initialization
  for (size_t index = 0; index < context.swapchain()->numberImages(); ++index) {
    swapchain_framebuffers[index] = context.createFramebuffer(
        renderPass->vkRenderPass(), {context.swapchain()->texture(index), depthTexture},
        nullptr, nullptr,
        "swapchain framebuffer " + std::to_string(swapchain_framebuffers.size()));
  }
#pragma endregion

#pragma region Pipeline initialization
  pipeline = context.createGraphicsPipeline(gpDesc, renderPass->vkRenderPass(), "main");

  pipeline->allocateDescriptors({
      {.set_ = GLYPH_INFO_STORAGE_SET, .count_ = 1},
      {.set_ = CELLS_STORAGE_SET, .count_ = 1},
      {.set_ = POINTS_STORAGE_SET, .count_ = 1},
  });

  pipeline->bindResource(GLYPH_INFO_STORAGE_SET, BINDING_0, 0, glyphInfoBuffer, 0,
                         glyphInfoBuffer->size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

  pipeline->bindResource(CELLS_STORAGE_SET, BINDING_0, 0, cellsBuffer, 0,
                         cellsBuffer->size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

  pipeline->bindResource(POINTS_STORAGE_SET, BINDING_0, 0, pointsBuffer, 0,
                         pointsBuffer->size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
#pragma endregion

  const std::array<VkClearValue, 2> clearValues = {
      VkClearValue{.color = {0.8f, 0.7f, 0.78f, 0.0f}},
      VkClearValue{.depthStencil = {1.0f}}};

  // FPS Counter
  EngineCore::FPSCounter fps(glfwGetTime());

  while (!glfwWindowShouldClose(window_)) {
    fps.update(glfwGetTime());

    commandMgr.waitUntilSubmitIsComplete();
    const auto texture = context.swapchain()->acquireImage();
    const auto index = context.swapchain()->currentImageIndex();

    auto commandBuffer = commandMgr.getCmdBufferToBegin();

    const VkRenderPassBeginInfo renderpassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass->vkRenderPass(),
        .framebuffer = swapchain_framebuffers[index]->vkFramebuffer(),
        .renderArea = {.offset = {0, 0},
                       .extent =
                           {
                               .width = texture->vkExtents().width,
                               .height = texture->vkExtents().height,
                           }},
        .clearValueCount = static_cast<uint32_t>(clearValues.size()),
        .pClearValues = clearValues.data(),
    };

    vkCmdBeginRenderPass(commandBuffer, &renderpassInfo, VK_SUBPASS_CONTENTS_INLINE);

#pragma region Render

    pipeline->bind(commandBuffer);

    pipeline->bindVertexBuffer(commandBuffer, charInstanceBuffer->vkBuffer());

    pipeline->bindDescriptorSets(commandBuffer,
                                 {
                                     {
                                         .set = GLYPH_INFO_STORAGE_SET,
                                         .bindIdx = 0,
                                     },
                                     {.set = CELLS_STORAGE_SET, .bindIdx = 0},
                                     {.set = POINTS_STORAGE_SET, .bindIdx = 0},
                                 });
    pipeline->updateDescriptorSets();

    // 4 vertex (Quad) & X (charData) instances
    vkCmdDraw(commandBuffer, 4, charsData.size(), 0, 0);

#pragma endregion

    vkCmdEndRenderPass(commandBuffer);

    commandMgr.endCmdBuffer(commandBuffer);

    VkPipelineStageFlags flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const auto submitInfo = context.swapchain()->createSubmitInfo(&commandBuffer, &flags);
    commandMgr.submit(&submitInfo);
    commandMgr.goToNextCmdBuffer();

    context.swapchain()->present();
    glfwPollEvents();

    // Increment frame number
    fps.incFrame();
  }

  vkDeviceWaitIdle(context.device());

  return 0;
}
