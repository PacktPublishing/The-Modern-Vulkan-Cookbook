#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <stb_image.h>

#include <array>
#include <filesystem>
#include <gli/gli.hpp>
#include <glm/glm.hpp>

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
#include "vulkancore/Sampler.hpp"
#include "vulkancore/Texture.hpp"

GLFWwindow* window_ = nullptr;
EngineCore::Camera camera(glm::vec3(0, 100, -370), glm::vec3(0, 50, 0),
                          glm::vec3(0, 1, 0), 0.1f, 1000.f, 1600.f / 1200.f);

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

  const VkFormat swapChainFormat = VK_FORMAT_B8G8R8A8_UNORM;

  context.createSwapchain(swapChainFormat, VK_COLORSPACE_SRGB_NONLINEAR_KHR,
                          VK_PRESENT_MODE_MAILBOX_KHR, extents);

  static const uint32_t framesInFlight = (uint32_t)context.swapchain()->numberImages();
#pragma endregion

  // Create command pools
  auto commandMgr = context.createGraphicsCommandQueue(
      context.swapchain()->numberImages(), framesInFlight, "main");

  UniformTransforms transform = {.model = glm::mat4(1.0f),
                                 .view = camera.viewMatrix(),
                                 .projection = camera.getProjectMatrix()};

  constexpr uint32_t CAMERA_SET = 0;
  constexpr uint32_t TEXTURES_SET = 1;
  constexpr uint32_t SAMPLER_SET = 2;
  // vertex/index/indirect/material
  constexpr uint32_t STORAGE_BUFFER_SET = 3;
  constexpr uint32_t GPU_LINE_BUFFER_SET = 4;
  constexpr uint32_t BINDING_0 = 0;
  constexpr uint32_t BINDING_1 = 1;
  constexpr uint32_t BINDING_2 = 2;
  constexpr uint32_t BINDING_3 = 3;

  std::vector<std::shared_ptr<VulkanCore::Buffer>> buffers;
  std::vector<std::shared_ptr<VulkanCore::Texture>> textures;
  std::vector<std::shared_ptr<VulkanCore::Sampler>> samplers;
  EngineCore::RingBuffer cameraBuffer(context.swapchain()->numberImages(), context,
                                      sizeof(UniformTransforms));
  uint32_t numMeshes = 0;

#pragma region Load model
  {
    const auto commandBuffer = commandMgr.getCmdBufferToBegin();
    {
      samplers.emplace_back(context.createSampler(
          VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT,
          VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, 10.0f,
          "default sampler"));

      EngineCore::GLBLoader glbLoader;
      std::shared_ptr<EngineCore::Model> duck =
          glbLoader.load("resources/assets/Duck.glb");

      EngineCore::convertModel2OneBuffer(context, commandMgr, commandBuffer, *duck.get(),
                                         buffers, textures, samplers);
      numMeshes = duck->meshes.size();
    }

    commandMgr.endCmdBuffer(commandBuffer);

    VkPipelineStageFlags flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
    const auto submitInfo =
        context.swapchain()->createSubmitInfo(&commandBuffer, &flags, false, false);
    commandMgr.submit(&submitInfo);
    commandMgr.waitUntilSubmitIsComplete();
  }
#pragma endregion

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

#pragma region Shaders
  const auto resourcesFolder = std::filesystem::current_path() / "resources/shaders/";

  const auto vertexShader =
      context.createShaderModule((resourcesFolder / "gpuLines.vert").string(),
                                 VK_SHADER_STAGE_VERTEX_BIT, "main vertex");
  const auto fragmentShader =
      context.createShaderModule((resourcesFolder / "gpuLines.frag").string(),
                                 VK_SHADER_STAGE_FRAGMENT_BIT, "main fragment");

  const auto vertexShaderGPULines =
      context.createShaderModule((resourcesFolder / "gpuLinesDraw.vert").string(),
                                 VK_SHADER_STAGE_VERTEX_BIT, "main vertex");
  const auto fragmentShaderGPULines =
      context.createShaderModule((resourcesFolder / "gpuLinesDraw.frag").string(),
                                 VK_SHADER_STAGE_FRAGMENT_BIT, "main fragment");

#pragma endregion

#pragma region Descriptor Set Layout and Pipeline Descriptor
  const std::vector<VulkanCore::Pipeline::SetDescriptor> setLayout = {
      {
          .set_ = CAMERA_SET,  // set number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding(
                      0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
              },
      },
      {
          .set_ = TEXTURES_SET,  // set number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding(
                      0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000,
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
              },
      },
      {
          .set_ = SAMPLER_SET,  // set number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding(
                      0, VK_DESCRIPTOR_TYPE_SAMPLER, 1000,
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
              },
      },
      {
          .set_ = STORAGE_BUFFER_SET,
          .bindings_ =
              {
                  VkDescriptorSetLayoutBinding(
                      0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4,
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
              },
      },
      {
          .set_ = GPU_LINE_BUFFER_SET,
          .bindings_ =
              {
                  VkDescriptorSetLayoutBinding(
                      0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
              },
      }};

  const VkViewport viewport = {
      .x = 0.f,
      .y = static_cast<float>(context.swapchain()->extent().height),
      .width = static_cast<float>(context.swapchain()->extent().width),
      .height = -static_cast<float>(context.swapchain()->extent().height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };

  const VulkanCore::Pipeline::GraphicsPipelineDescriptor gpDescMain = {
      .sets_ = setLayout,
      .vertexShader_ = vertexShader,
      .fragmentShader_ = fragmentShader,
      .dynamicStates_ = {VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE},
      .colorTextureFormats = {swapChainFormat},
      .depthTextureFormat = depthTexture->vkFormat(),
      .sampleCount = VK_SAMPLE_COUNT_1_BIT,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .viewport = viewport,
      .depthWriteEnable = true,
      .depthCompareOperation = VK_COMPARE_OP_LESS,
  };

  const std::vector<VulkanCore::Pipeline::SetDescriptor> setLayoutGPULines = {
      {
          .set_ = CAMERA_SET,
          .bindings_ =
              {
                  VkDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                               VK_SHADER_STAGE_VERTEX_BIT),
              },
      },
      {
          .set_ = 1,
          .bindings_ =
              {
                  VkDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                               VK_SHADER_STAGE_VERTEX_BIT),
              },
      }};

  const VulkanCore::Pipeline::GraphicsPipelineDescriptor gpDescLines = {
      .sets_ = setLayoutGPULines,
      .vertexShader_ = vertexShaderGPULines,
      .fragmentShader_ = fragmentShaderGPULines,
      .dynamicStates_ = {VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE},
      .colorTextureFormats = {swapChainFormat},
      .depthTextureFormat = depthTexture->vkFormat(),
      .primitiveTopology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
      .sampleCount = VK_SAMPLE_COUNT_1_BIT,
      .cullMode = VK_CULL_MODE_NONE,
      .viewport = viewport,
      .depthWriteEnable = false,
      .depthCompareOperation = VK_COMPARE_OP_LESS,
  };
#pragma endregion

#pragma region Render Pass Initialization
  const std::shared_ptr<VulkanCore::RenderPass> renderPassMain = context.createRenderPass(
      {context.swapchain()->texture(0), depthTexture},
      {VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_CLEAR},
      {VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_STORE_OP_DONT_CARE},
      {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
       VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
      VK_PIPELINE_BIND_POINT_GRAPHICS, {}, "main");

  const auto renderPassGPULines = VulkanCore::RenderPass(
      context,
      {context.swapchain()->texture(0)->vkFormat(), depthTexture->vkFormat()},  // formats
      {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
       VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},  // initial layouts
      {VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
       VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},                // final layouts
      {VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_CLEAR},         // loadOp
      {VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_STORE_OP_DONT_CARE},  // storeOp
      VK_PIPELINE_BIND_POINT_GRAPHICS, std::vector<uint32_t>(), 1, UINT32_MAX,
      VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, false,
      "GPU lines");
#pragma endregion

#pragma region Framebuffers Initialization
  std::vector<std::unique_ptr<VulkanCore::Framebuffer>> swapchain_main(
      context.swapchain()->numberImages());
  std::vector<std::unique_ptr<VulkanCore::Framebuffer>> swapchain_framebuffers(
      context.swapchain()->numberImages());

  for (size_t index = 0; index < context.swapchain()->numberImages(); ++index) {
    swapchain_main[index] = context.createFramebuffer(
        renderPassMain->vkRenderPass(),
        {context.swapchain()->texture(index), depthTexture}, nullptr, nullptr,
        "main framebuffer" + std::to_string(swapchain_framebuffers.size()));

    swapchain_framebuffers[index] = context.createFramebuffer(
        renderPassGPULines.vkRenderPass(),
        {context.swapchain()->texture(index), depthTexture}, nullptr, nullptr,
        "swapchain framebuffer" + std::to_string(swapchain_framebuffers.size()));
  }
#pragma endregion

#pragma region GPU Lines
  constexpr uint32_t kNumLines = 65'536;
  struct Line {
    glm::vec4 v0_;  // vec4 b/c of alignment
    glm::vec4 color0_;
    glm::vec4 v1_;  // vec4 b/c of alignment
    glm::vec4 color1_;
  };
  struct Header {
    uint32_t maxNumlines_;
    uint32_t padding0 = 0u;
    uint32_t padding1 = 0u;
    uint32_t padding2 = 0u;
    VkDrawIndirectCommand cmd_;
  };
  struct GPULineBuffer {
    Header header_;
    Line lines_[kNumLines];
  };
  constexpr size_t kGPULinesBufferSize = sizeof(Header) + sizeof(Line) * kNumLines;
  std::shared_ptr<VulkanCore::Buffer> gpuLineBuffer;
  gpuLineBuffer = context.createBuffer(
      kGPULinesBufferSize,
      VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
          VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      static_cast<VmaMemoryUsage>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT), "GPU Lines");

  {
    constexpr Header data = {
        .maxNumlines_ = kNumLines,
        .cmd_ =
            VkDrawIndirectCommand{
                .vertexCount = 2,
            },
    };
    auto cmdBuffer = commandMgr.getCmdBufferToBegin();
    context.uploadToGPUBuffer(commandMgr, cmdBuffer, gpuLineBuffer.get(), &data,
                              sizeof(Header), 0);
    commandMgr.endCmdBuffer(cmdBuffer);
    const VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmdBuffer,
    };
    commandMgr.submit(&submitInfo);
  }
#pragma endregion

#pragma region Pipeline and Descriptors initialization
  auto pipelineMain =
      context.createGraphicsPipeline(gpDescMain, renderPassMain->vkRenderPass(), "main");
  auto pipelineGPULines = context.createGraphicsPipeline(
      gpDescLines, renderPassGPULines.vkRenderPass(), "GPU Lines");

  pipelineMain->allocateDescriptors({
      {.set_ = CAMERA_SET, .count_ = 3, .name_ = "camera"},
      {.set_ = TEXTURES_SET, .count_ = 1, .name_ = "textures"},
      {.set_ = SAMPLER_SET, .count_ = 1, .name_ = "samplers"},
      {.set_ = STORAGE_BUFFER_SET, .count_ = 1, .name_ = "buffer"},
      {.set_ = GPU_LINE_BUFFER_SET, .count_ = 1, .name_ = "GPU lines buffer write"},
  });
  pipelineGPULines->allocateDescriptors({
      {.set_ = CAMERA_SET, .count_ = 3, .name_ = "camera"},
      {.set_ = 1, .count_ = 1, .name_ = "GPU lines buffer read"},
  });

  pipelineMain->bindResource(CAMERA_SET, BINDING_0, 0, cameraBuffer.buffer(0), 0,
                             sizeof(UniformTransforms),
                             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  pipelineGPULines->bindResource(CAMERA_SET, BINDING_0, 0, cameraBuffer.buffer(0), 0,
                                 sizeof(UniformTransforms),
                                 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  pipelineMain->bindResource(CAMERA_SET, BINDING_0, 1, cameraBuffer.buffer(1), 0,
                             sizeof(UniformTransforms),
                             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  pipelineGPULines->bindResource(CAMERA_SET, BINDING_0, 1, cameraBuffer.buffer(1), 0,
                                 sizeof(UniformTransforms),
                                 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  pipelineMain->bindResource(CAMERA_SET, BINDING_0, 2, cameraBuffer.buffer(2), 0,
                             sizeof(UniformTransforms),
                             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  pipelineGPULines->bindResource(CAMERA_SET, BINDING_0, 2, cameraBuffer.buffer(2), 0,
                                 sizeof(UniformTransforms),
                                 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  pipelineMain->bindResource(STORAGE_BUFFER_SET, BINDING_0, 0,
                             {
                                 buffers[0],  // vertex
                                 buffers[1],  // index
                                 buffers[3],  // indirect
                                 buffers[2],  // material
                             },
                             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  pipelineMain->bindResource(TEXTURES_SET, BINDING_0, 0,
                             {
                                 textures.begin(),
                                 textures.end(),
                             });
  pipelineMain->bindResource(SAMPLER_SET, BINDING_0, 0, {samplers.begin(), 1});
  pipelineMain->bindResource(GPU_LINE_BUFFER_SET, 0, 0, {gpuLineBuffer},
                             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  pipelineGPULines->bindResource(1, 0, 0, {gpuLineBuffer}, sizeof(Header),
                                 sizeof(GPULineBuffer) - sizeof(Header),
                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

#pragma endregion

  const std::array<VkClearValue, 2> clearValues = {
      VkClearValue{.color = {0.8f, 0.7f, 0.78f, 0.0f}},
      VkClearValue{.depthStencil = {1.0f}}};

  const glm::mat4 view = glm::translate(glm::mat4(1.f), {0.f, 0.f, 0.5f});

  // FPS Counter
  EngineCore::FPSCounter fps(glfwGetTime());

  while (!glfwWindowShouldClose(window_)) {
    fps.update(glfwGetTime());

    const auto texture = context.swapchain()->acquireImage();
    const auto index = context.swapchain()->currentImageIndex();

    auto commandBuffer = commandMgr.getCmdBufferToBegin();

    const VkRenderPassBeginInfo renderpassInfoMain = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPassMain->vkRenderPass(),
        .framebuffer = swapchain_main[index]->vkFramebuffer(),
        .renderArea = {.offset = {0, 0},
                       .extent = {.width = texture->vkExtents().width,
                                  .height = texture->vkExtents().height}},
        .clearValueCount = static_cast<uint32_t>(clearValues.size()),
        .pClearValues = clearValues.data(),
    };

    const VkRenderPassBeginInfo renderpassInfoLines = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPassGPULines.vkRenderPass(),
        .framebuffer = swapchain_framebuffers[index]->vkFramebuffer(),
        .renderArea = {.offset = {0, 0},
                       .extent = {.width = texture->vkExtents().width,
                                  .height = texture->vkExtents().height}},
        .clearValueCount = static_cast<uint32_t>(clearValues.size()),
        .pClearValues = clearValues.data(),
    };

#pragma region Main Render Pass
    vkCmdBeginRenderPass(commandBuffer, &renderpassInfoMain, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdSetDepthTestEnable(commandBuffer, VK_TRUE);

    pipelineMain->bind(commandBuffer);

    if (camera.isDirty()) {
      transform.view = camera.viewMatrix();
      camera.setNotDirty();
    }
    cameraBuffer.buffer()->copyDataToBuffer(&transform, sizeof(UniformTransforms));

    pipelineMain->bindDescriptorSets(commandBuffer,
                                     {
                                         {.set = CAMERA_SET, .bindIdx = (uint32_t)index},
                                         {.set = TEXTURES_SET, .bindIdx = 0},
                                         {.set = SAMPLER_SET, .bindIdx = 0},
                                         {.set = STORAGE_BUFFER_SET, .bindIdx = 0},
                                         {.set = GPU_LINE_BUFFER_SET, .bindIdx = 0},
                                     });
    pipelineMain->updateDescriptorSets();

    vkCmdBindIndexBuffer(commandBuffer, buffers[1]->vkBuffer(), 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexedIndirect(commandBuffer, buffers[3]->vkBuffer(), 0, numMeshes,
                             sizeof(EngineCore::IndirectDrawCommandAndMeshData));
    vkCmdEndRenderPass(commandBuffer);
#pragma endregion

    const VkBufferMemoryBarrier bufferBarrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = gpuLineBuffer->vkBuffer(),
        .offset = 0,
        .size = VK_WHOLE_SIZE,
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, nullptr, 1,
                         &bufferBarrier, 0, nullptr);

#pragma region GPU Lines Render Pass
    vkCmdBeginRenderPass(commandBuffer, &renderpassInfoLines, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetDepthTestEnable(commandBuffer, VK_FALSE);
    pipelineGPULines->bind(commandBuffer);
    pipelineGPULines->bindDescriptorSets(
        commandBuffer, {
                           {.set = CAMERA_SET, .bindIdx = (uint32_t)index},
                           {.set = 1, .bindIdx = 0},
                       });
    pipelineGPULines->updateDescriptorSets();

    vkCmdDrawIndirect(commandBuffer, gpuLineBuffer->vkBuffer(), sizeof(uint32_t) * 4, 1,
                      sizeof(VkDrawIndirectCommand));

    vkCmdEndRenderPass(commandBuffer);
#pragma endregion

    const VkBufferMemoryBarrier bufferBarrierClear = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = gpuLineBuffer->vkBuffer(),
        .offset = 0,
        .size = VK_WHOLE_SIZE,
    };

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1,
                         &bufferBarrierClear, 0, nullptr);

    // Reset the number of lines in the GPU lines buffer
    vkCmdFillBuffer(commandBuffer, gpuLineBuffer->vkBuffer(), sizeof(uint32_t) * 5,
                    sizeof(uint32_t), 0);

    commandMgr.endCmdBuffer(commandBuffer);

    const VkPipelineStageFlags flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const auto submitInfo = context.swapchain()->createSubmitInfo(&commandBuffer, &flags);
    commandMgr.submit(&submitInfo);
    commandMgr.goToNextCmdBuffer();

    context.swapchain()->present();
    glfwPollEvents();

    cameraBuffer.moveToNextBuffer();

    // Increment frame number
    fps.incFrame();
  }

  vkDeviceWaitIdle(context.device());

  return 0;
}
