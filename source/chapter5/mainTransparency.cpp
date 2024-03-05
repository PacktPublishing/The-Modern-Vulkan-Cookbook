#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <stb_image.h>

#include <array>
#include <filesystem>
#include <gli/gli.hpp>
#include <glm/glm.hpp>
#include <tracy/Tracy.hpp>

#include "enginecore/Camera.hpp"
#include "enginecore/FPSCounter.hpp"
#include "enginecore/GLBLoader.hpp"
#include "enginecore/GLFWUtils.hpp"
#include "enginecore/ImguiManager.hpp"
#include "enginecore/Model.hpp"
#include "enginecore/RingBuffer.hpp"
#include "enginecore/passes/DepthPeeling.hpp"
#include "enginecore/passes/DualDepthPeeling.hpp"
#include "enginecore/passes/FullScreenColorNBlendPass.hpp"
#include "enginecore/passes/FullScreenPass.hpp"
#include "enginecore/passes/OitLinkedListPass.hpp"
#include "enginecore/passes/OitWeightedPass.hpp"
#include "imgui.h"
#include "vulkancore/Buffer.hpp"
#include "vulkancore/CommandQueueManager.hpp"
#include "vulkancore/Context.hpp"
#include "vulkancore/DynamicRendering.hpp"
#include "vulkancore/Pipeline.hpp"
#include "vulkancore/RenderPass.hpp"
#include "vulkancore/Sampler.hpp"
#include "vulkancore/Texture.hpp"

// clang-format off
#include <tracy/TracyVulkan.hpp>
// clang-format on

// Enum for technique
enum Technique {
  DepthPeelingAlgo,
  DualDepthPeelingAlgo,
  LinkedListAlgo,
  WeightedBlendAlgo,
  TechniqueCount
};

// char array for displaying in ImGui
const char* techniqueNames[] = {
    "DepthPeeling",
    "Dual Depth Peeling",
    "LinkedList",
    "WeightedBlend",
};

struct ObjectProperties {
  glm::vec4 color;
  glm::mat4 modelMat;
};

GLFWwindow* window_ = nullptr;
EngineCore::Camera camera(glm::vec3(-1.17f, 1.6f, 8.7f), glm::vec3(0.0f, 0.0f, 0.0f),
                          glm::vec3(0.0f, 1.0f, 0.0f), .01, 10.0f);
int main(int argc, char* argv[]) {
  initWindow(&window_, &camera);

  camera.setEulerAngles(glm::vec3(-3.9f, 1.4f, -.103f));

#pragma region Context initialization
  std::vector<std::string> instExtension = {
      VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
  };

  std::vector<std::string> deviceExtension = {
#if defined(VK_EXT_calibrated_timestamps)
    VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
#endif
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
  };

  std::vector<std::string> validationLayers;
#ifdef _DEBUG
  validationLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif

  VulkanCore::Context::enableDefaultFeatures();
  VulkanCore::Context::enableBufferDeviceAddressFeature();
  VulkanCore::Context::enableDynamicRenderingFeature();
  VulkanCore::Context::enableIndependentBlending();

  VulkanCore::Context context((void*)glfwGetWin32Window(window_),
                              validationLayers,  // layers
                              instExtension,     // instance extensions
                              deviceExtension,   // device extensions
                              0,                 // request a graphics queue only
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
      context.swapchain()->numberImages(), framesInFlight, "main command");

#pragma region Tracy
#if defined(VK_EXT_calibrated_timestamps)
  TracyVkCtx tracyCtx_ = TracyVkContextCalibrated(
      context.physicalDevice().vkPhysicalDevice(), context.device(),
      context.graphicsQueue(), commandMgr.getCmdBuffer(),
      vkGetPhysicalDeviceCalibrateableTimeDomainsEXT, vkGetCalibratedTimestampsEXT);
#else
  TracyVkCtx tracyCtx_ =
      TracyVkContext(context.physicalDevice().vkPhysicalDevice(), context.device(),
                     context.graphicsQueue(), commandMgr.getCmdBuffer());
#endif
#pragma endregion

  UniformTransforms transform = {.model = glm::mat4(1.0f),
                                 .view = camera.viewMatrix(),
                                 .projection = camera.getProjectMatrix()};

  std::vector<std::shared_ptr<VulkanCore::Buffer>> buffers;
  std::vector<std::shared_ptr<VulkanCore::Texture>> textures;
  std::vector<std::shared_ptr<VulkanCore::Sampler>> samplers;
  EngineCore::RingBuffer cameraBuffer(context.swapchain()->numberImages(), context,
                                      sizeof(UniformTransforms), "Camera Ring Buffer");
  uint32_t numMeshes = 0;
  std::shared_ptr<EngineCore::Model> bistro;

#pragma region Load model
  {
    const auto commandBuffer = commandMgr.getCmdBufferToBegin();
    {
      samplers.emplace_back(context.createSampler(
          VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT,
          VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, 10.0f,
          "default sampler"));

      ZoneScopedN("Model load");
      EngineCore::GLBLoader glbLoader;
      bistro = glbLoader.load("resources/assets/Planes.glb");
      TracyVkZone(tracyCtx_, commandBuffer, "Model upload");
      EngineCore::convertModel2OneMeshPerBuffer(
          context, commandMgr, commandBuffer, *bistro.get(), buffers, textures, samplers);

      if (textures.size() == 0) {
        textures.push_back(context.createTexture(
            VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, 0,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VkExtent3D{
                .width = static_cast<uint32_t>(1),
                .height = static_cast<uint32_t>(1),
                .depth = static_cast<uint32_t>(1.0),
            },
            1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
            "Empty Texture"));
      }

      numMeshes = buffers.size() >> 1;
    }

    TracyVkCollect(tracyCtx_, commandBuffer);
    commandMgr.endCmdBuffer(commandBuffer);

    VkPipelineStageFlags flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
    const auto submitInfo =
        context.swapchain()->createSubmitInfo(&commandBuffer, &flags, false, false);
    commandMgr.submit(&submitInfo);
    commandMgr.waitUntilSubmitIsComplete();
  }
#pragma endregion

#pragma region DepthTexture
  auto depthTexture = context.createTexture(
      VK_IMAGE_TYPE_2D, VK_FORMAT_D24_UNORM_S8_UINT, 0,
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
          VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      {
          .width = context.swapchain()->extent().width,
          .height = context.swapchain()->extent().height,
          .depth = 1,
      },
      1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
      "depth buffer");

  // clear opaque depth, make it 1.0 for now
  {
    const auto commandBuffer = commandMgr.getCmdBufferToBegin();
    const VkClearDepthStencilValue clearDepth = {
        .depth = 1.0f,
    };
    const VkImageSubresourceRange range = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};

    depthTexture->transitionImageLayout(commandBuffer,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkCmdClearDepthStencilImage(commandBuffer, depthTexture->vkImage(),
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearDepth, 1,
                                &range);
    commandMgr.endCmdBuffer(commandBuffer);

    VkPipelineStageFlags flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
    const auto submitInfo =
        context.swapchain()->createSubmitInfo(&commandBuffer, &flags, false, false);
    commandMgr.submit(&submitInfo);
    commandMgr.waitUntilSubmitIsComplete();
  }
#pragma endregion

  EngineCore::RingBuffer objectPropBuffers(numMeshes, context, sizeof(ObjectProperties),
                                           "Object Prop Ring Buffer");

  DepthPeeling depthPeelingPass(&context);
  depthPeelingPass.init(&context, cameraBuffer, objectPropBuffers,
                        sizeof(ObjectProperties), numMeshes, 6, swapChainFormat,
                        depthTexture->vkFormat(), depthTexture);

  DualDepthPeeling dualDepthPeelingPass(&context);
  dualDepthPeelingPass.init(&context, cameraBuffer, objectPropBuffers,
                            sizeof(ObjectProperties), numMeshes, 4, swapChainFormat,
                            depthTexture->vkFormat(), depthTexture);

  OitLinkedListPass oitLLColorPass;
  oitLLColorPass.init(&context, cameraBuffer, objectPropBuffers, sizeof(ObjectProperties),
                      numMeshes, swapChainFormat, depthTexture->vkFormat(), depthTexture);

  OitWeightedPass oitWeightedPass;
  oitWeightedPass.init(&context, cameraBuffer, objectPropBuffers,
                       sizeof(ObjectProperties), numMeshes, swapChainFormat,
                       depthTexture->vkFormat(), depthTexture);

  FullScreenPass fullscreenPass(true);
  fullscreenPass.init(&context, {swapChainFormat});

  auto textureToDisplay = depthPeelingPass.colorTexture();
  fullscreenPass.pipeline()->bindResource(0, 0, 0, {&textureToDisplay, 1}, samplers[0]);

  const auto resourcesFolder = std::filesystem::current_path() / "resources/shaders/";
  constexpr uint32_t CAMERA_SET = 0;
  constexpr uint32_t OBJECT_PROP_SET = 1;
  constexpr uint32_t BINDING_0 = 0;
  constexpr uint32_t BINDING_1 = 1;
  auto vertexShader =
      context.createShaderModule((resourcesFolder / "bindfull.vert").string(),
                                 VK_SHADER_STAGE_VERTEX_BIT, "main vertex");
  auto fragmentShader =
      context.createShaderModule((resourcesFolder / "bindfull.frag").string(),
                                 VK_SHADER_STAGE_FRAGMENT_BIT, "main fragment");

  const std::vector<VulkanCore::Pipeline::SetDescriptor> setLayout = {
      {
          .set_ = CAMERA_SET,  // set number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                               VK_SHADER_STAGE_VERTEX_BIT),
              },
      },
      {
          .set_ = OBJECT_PROP_SET,  // set
                                    // number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding(
                      0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
              },
      },
  };

  VkVertexInputBindingDescription bindingDesc = {
      .binding = 0,
      .stride = sizeof(EngineCore::Vertex),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };

  std::vector<std::pair<VkFormat, size_t>> vertexAttributesFormatAndOffset = {
      {VK_FORMAT_R32G32B32_SFLOAT, offsetof(EngineCore::Vertex, pos)},
      {VK_FORMAT_R32G32B32_SFLOAT, offsetof(EngineCore::Vertex, normal)},
      {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(EngineCore::Vertex, tangent)},
      {VK_FORMAT_R32G32_SFLOAT, offsetof(EngineCore::Vertex, texCoord)},
      {VK_FORMAT_R32_SINT, offsetof(EngineCore::Vertex, material)}};

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
      .dynamicStates_ = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR},
      .useDynamicRendering_ = true,
      .colorTextureFormats = {swapChainFormat},
      .depthTextureFormat = depthTexture->vkFormat(),
      .sampleCount = VK_SAMPLE_COUNT_1_BIT,
      .cullMode = VK_CULL_MODE_NONE,
      .viewport = context.swapchain()->extent(),
      .blendEnable = true,
      .depthTestEnable = false,
      .depthWriteEnable = true,
      .depthCompareOperation = VK_COMPARE_OP_LESS,
      .vertexInputCreateInfo =
          {
              .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
              .vertexBindingDescriptionCount = 1u,
              .pVertexBindingDescriptions = &bindingDesc,
              .vertexAttributeDescriptionCount = uint32_t(vertexInputAttributes.size()),
              .pVertexAttributeDescriptions = vertexInputAttributes.data(),
          },
  };

#pragma region Pipeline initialization

  auto pipeline = context.createGraphicsPipeline(gpDesc, VK_NULL_HANDLE,
                                                 "Pipeline Without BaseColorTexture");

  pipeline->allocateDescriptors({
      {.set_ = CAMERA_SET, .count_ = 3},
      {.set_ = OBJECT_PROP_SET, .count_ = numMeshes},
  });

  pipeline->bindResource(CAMERA_SET, BINDING_0, 0, cameraBuffer.buffer(0), 0,
                         sizeof(UniformTransforms), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  pipeline->bindResource(CAMERA_SET, BINDING_0, 1, cameraBuffer.buffer(1), 0,
                         sizeof(UniformTransforms), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  pipeline->bindResource(CAMERA_SET, BINDING_0, 2, cameraBuffer.buffer(2), 0,
                         sizeof(UniformTransforms), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

  for (uint32_t meshIdx = 0; meshIdx < numMeshes; ++meshIdx) {
    pipeline->bindResource(OBJECT_PROP_SET, BINDING_0, meshIdx,
                           objectPropBuffers.buffer(meshIdx), 0, sizeof(ObjectProperties),
                           VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  }

#pragma endregion

  float r = 0.3f, g = 0.3f, b = 0.3f;
  const std::array<VkClearValue, 2> clearValues = {VkClearValue{.color = {r, g, b, 1.0f}},
                                                   VkClearValue{.depthStencil = {1.0f}}};

  const glm::mat4 view = glm::translate(glm::mat4(1.f), {0.f, 0.f, 0.5f});
  auto time = glfwGetTime();

  std::unique_ptr<EngineCore::GUI::ImguiManager> imguiMgr = nullptr;

  TracyPlotConfig("Swapchain image index", tracy::PlotFormatType::Number, true, false,
                  tracy::Color::Aqua);

  constexpr size_t numSamples = 15;
  EngineCore::FPSCounter fps(glfwGetTime(), numSamples);

  while (!glfwWindowShouldClose(window_)) {
    fps.update(glfwGetTime());

    const auto texture = context.swapchain()->acquireImage();
    const auto index = context.swapchain()->currentImageIndex();
    TracyPlot("Swapchain image index", (int64_t)index);

    if (camera.isDirty()) {
      transform.view = camera.viewMatrix();
      camera.setNotDirty();
    }
    cameraBuffer.buffer()->copyDataToBuffer(&transform, sizeof(UniformTransforms));

    glm::mat4 matTranslate = glm::translate(glm::mat4(1.0f), glm::vec3(1, 0, 0));

    static std::vector<std::array<float, 3>> imgui_meshTranslations(numMeshes,
                                                                    {0.0f, 0.0f, 0.0f});

    static std::vector<std::array<float, 4>> imgui_meshColors(numMeshes,
                                                              {0.0f, 0.0f, 0.0f, 0.0f});

    auto commandBuffer = commandMgr.getCmdBufferToBegin();

    static int imgui_meshIndex = 0;

    static Technique imgui_currentTechnique = DepthPeelingAlgo;

    if (!imguiMgr) {
      imguiMgr = std::make_unique<EngineCore::GUI::ImguiManager>(
          window_, context, commandBuffer, swapChainFormat, VK_SAMPLE_COUNT_1_BIT);
      for (uint32_t meshIdx = 0; meshIdx < numMeshes; ++meshIdx) {
        if (bistro->meshes[meshIdx].material != -1) {
          imgui_meshColors[meshIdx][0] =
              bistro->materials[bistro->meshes[meshIdx].material].basecolor.r;
          imgui_meshColors[meshIdx][1] =
              bistro->materials[bistro->meshes[meshIdx].material].basecolor.g;
          imgui_meshColors[meshIdx][2] =
              bistro->materials[bistro->meshes[meshIdx].material].basecolor.b;
          imgui_meshColors[meshIdx][3] =
              bistro->materials[bistro->meshes[meshIdx].material].basecolor.a;
        }
      }
    }

    for (uint32_t meshIdx = 0; meshIdx < numMeshes; ++meshIdx) {
      ObjectProperties prop;
      prop.color = glm::vec4(imgui_meshColors[meshIdx][0], imgui_meshColors[meshIdx][1],
                             imgui_meshColors[meshIdx][2], imgui_meshColors[meshIdx][3]);

      prop.modelMat =
          glm::translate(glm::mat4(1.0f), glm::vec3(imgui_meshTranslations[meshIdx][0],
                                                    imgui_meshTranslations[meshIdx][1],
                                                    imgui_meshTranslations[meshIdx][2]));
      objectPropBuffers.buffer(meshIdx)->copyDataToBuffer(&prop,
                                                          sizeof(ObjectProperties));
    }

    std::shared_ptr<VulkanCore::Texture> ptr;
    if (imgui_currentTechnique == DepthPeelingAlgo) {
      ptr = depthPeelingPass.colorTexture();
      depthPeelingPass.draw(commandBuffer, index, buffers, numMeshes);
    } else if (imgui_currentTechnique == DualDepthPeelingAlgo) {
      ptr = dualDepthPeelingPass.colorTexture();
      dualDepthPeelingPass.draw(commandBuffer, index, buffers, numMeshes);
    } else if (imgui_currentTechnique == LinkedListAlgo) {
      ptr = oitLLColorPass.colorTexture();
      oitLLColorPass.draw(commandBuffer, index, buffers, numMeshes);
    } else if (imgui_currentTechnique == WeightedBlendAlgo) {
      ptr = oitWeightedPass.colorTexture();
      oitWeightedPass.draw(commandBuffer, index, buffers, numMeshes);
    }

    fullscreenPass.pipeline()->bindResource(0, 0, 0, {&ptr, 1}, samplers[0]);
    fullscreenPass.render(commandBuffer, index);

#pragma region imgui
    context.beginDebugUtilsLabel(commandBuffer, "Imgui pass", {0.0f, 1.0f, 0.0f, 1.0f});
    if (imguiMgr) {
      imguiMgr->frameBegin();

      ImGui::Text("FPS: %f", fps.last());
      ImGui::PlotLines("FPS", fps.fpsSamples().data(), numSamples);

      imguiMgr->createCameraPosition(camera.position());
      camera.setPos(imguiMgr->cameraPosition());
      imguiMgr->createCameraDir(camera.eulerAngles());
      camera.setEulerAngles(imguiMgr->cameraDir());

      ImGui::SliderInt("Mesh Index", &imgui_meshIndex, 0, numMeshes - 1);
      std::string meshT = "Mesh Translation " + std::to_string(imgui_meshIndex);
      ImGui::SliderFloat3(meshT.c_str(), &imgui_meshTranslations[imgui_meshIndex][0],
                          -10.0f, 10.0f);

      std::string meshC = "Mesh Colors " + std::to_string(imgui_meshIndex);
      ImGui::SliderFloat4(meshC.c_str(), &imgui_meshColors[imgui_meshIndex][0], 0.0f,
                          0.95f);

      int currentItem = static_cast<int>(imgui_currentTechnique);
      ImGui::Combo("OIT Technique", &currentItem, techniqueNames, TechniqueCount);
      imgui_currentTechnique = static_cast<Technique>(currentItem);

      imguiMgr->frameEnd();
    }

    const VulkanCore::DynamicRendering::AttachmentDescription colorAttachmentDesc{
        .imageView = texture->vkImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveModeFlagBits = VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .attachmentLoadOp =
            VK_ATTACHMENT_LOAD_OP_LOAD,  // load since we want to preserve content
        .attachmentStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = VkClearValue{.color = {0.0f, 0.0f, 0.0f, 0.0f}},
    };

    VulkanCore::DynamicRendering::beginRenderingCmd(
        commandBuffer, texture->vkImage(), 0,
        {{0, 0}, {texture->vkExtents().width, texture->vkExtents().height}}, 1, 0,
        {colorAttachmentDesc}, nullptr, nullptr,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    if (imguiMgr) {
      imguiMgr->recordCommands(commandBuffer);
    }
    VulkanCore::DynamicRendering::endRenderingCmd(commandBuffer, texture->vkImage());
    context.endDebugUtilsLabel(commandBuffer);
#pragma endregion

    TracyVkCollect(tracyCtx_, commandBuffer);

    commandMgr.endCmdBuffer(commandBuffer);

    VkPipelineStageFlags flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const auto submitInfo = context.swapchain()->createSubmitInfo(&commandBuffer, &flags);
    commandMgr.submit(&submitInfo);
    commandMgr.goToNextCmdBuffer();

    context.swapchain()->present();
    glfwPollEvents();

    fps.incFrame();

    cameraBuffer.moveToNextBuffer();

    FrameMarkNamed("main frame");
  }

  vkDeviceWaitIdle(context.device());

  return 0;
}
