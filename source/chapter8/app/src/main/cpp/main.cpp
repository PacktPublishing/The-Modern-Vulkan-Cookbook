#include <jni.h>

#include <unistd.h>
#include <span>

#include <android/log.h>
#include <android/looper.h>
#include <android/asset_manager_jni.h>
#include <android_native_app_glue.h>

#include "enginecore/Camera.hpp"
#include "vulkancore/RenderPass.hpp"
#include "enginecore/RingBuffer.hpp"
#include "vulkancore/Texture.hpp"

// This will include volk/vulkan for us
#include "vulkancore/Context.hpp"
#include "vulkancore/Framebuffer.hpp"
#include "vulkancore/RenderPass.hpp"
#include "vulkancore/Texture.hpp"

#include "enginecore/GLBLoader.hpp"

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "OXRContext.h"

struct Transforms {
    glm::aligned_mat4 mvp_left;
    glm::aligned_mat4 mvp_right;
};

extern "C" {

void handleInitWindow(const struct android_app *app) {
  auto ctx = static_cast<OXR::Context *>(app->userData);
  if (ctx) {
    ctx->setNativeWindow(app->window);
  }
}
void handleTermWindow(const struct android_app *app) {
  auto ctx = static_cast<OXR::Context *>(app->userData);
  if (ctx) {
    ctx->setNativeWindow(nullptr);
  }
}

void handleResume(const struct android_app *app) {
  auto ctx = static_cast<OXR::Context *>(app->userData);
  if (ctx) {
    ctx->setResumed(true);
  }
}

void handlePause(const struct android_app *app) {
  auto ctx = static_cast<OXR::Context *>(app->userData);
  if (ctx) {
    ctx->setResumed(false);
  }
}

void handleDestroy(const struct android_app *app) {
  auto ctx = static_cast<OXR::Context *>(app->userData);
  if (ctx) {
    ctx->setNativeWindow(nullptr);
  }
}


/*!
 * Handles commands sent to this Android application
 * @param pApp the app the commands are coming from
 * @param cmd the command to handle
 */
void handle_cmd(android_app *app, int32_t cmd) {
  switch (cmd) {
    case APP_CMD_INIT_WINDOW:
      LOGI("APP_CMD_INIT_WINDOW");
      handleInitWindow(app);
      break;
    case APP_CMD_TERM_WINDOW:
      LOGI("APP_CMD_TERM_WINDOW");
      handleTermWindow(app);
      break;
    case APP_CMD_RESUME:
      LOGI("APP_CMD_RESUME");
      handleResume(app);
      break;
    case APP_CMD_PAUSE:
      LOGI("APP_CMD_PAUSE");
      handlePause(app);
      break;
    case APP_CMD_STOP:
      LOGI("APP_CMD_PAUSE");
      break;
    case APP_CMD_DESTROY:
      LOGI("APP_CMD_DESTROY");
      handleDestroy(app);
      break;
  }
}

/*!
 * Enable the motion events you want to handle; not handled events are
 * passed back to OS for further processing. For this example case,
 * only pointer and joystick devices are enabled.
 *
 * @param motionEvent the newly arrived GameActivityMotionEvent.
 * @return true if the event is from a pointer or joystick device,
 *         false for all other input devices.
 */
//bool motion_event_filter_func(const GameActivityMotionEvent *motionEvent) {
//    auto sourceClass = motionEvent->source & AINPUT_SOURCE_CLASS_MASK;
//    return (sourceClass == AINPUT_SOURCE_CLASS_POINTER ||
//            sourceClass == AINPUT_SOURCE_CLASS_JOYSTICK);
//}

/*!
 * This the main entry point for a native activity
 */
void android_main(struct android_app *pApp) {
  JNIEnv *Env;
  pApp->activity->vm->AttachCurrentThread(&Env, nullptr);

//#ifdef ATTACH_DEBUGGER
//  sleep(20);
//#endif
  app_dummy();

  auto contextClass = Env->GetObjectClass(pApp->activity->clazz);
  const jmethodID getAssetsMethod = Env->GetMethodID(contextClass, "getAssets",
                                                     "()Landroid/content/res/AssetManager;");
  const jobject AssetManagerObject = Env->CallObjectMethod((jobject) pApp->activity->clazz,
                                                           getAssetsMethod);

  AAssetManager *assetMgr = AAssetManager_fromJava(Env, AssetManagerObject);

  AAsset *vs_shader = AAssetManager_open(assetMgr, "shaders/indirectdrawMVvert.spv",
                                         AASSET_MODE_BUFFER);
  size_t vs_shader_length = AAsset_getLength(vs_shader);
  char *vs_shader_content = new char[vs_shader_length + 1];
  AAsset_read(vs_shader, vs_shader_content, vs_shader_length);

  AAsset *fs_shader = AAssetManager_open(assetMgr, "shaders/indirectdrawMVfrag.spv",
                                         AASSET_MODE_BUFFER);
  size_t fs_shader_length = AAsset_getLength(fs_shader);
  char *fs_shader_content = new char[fs_shader_length + 1];
  AAsset_read(fs_shader, fs_shader_content, fs_shader_length);

  std::vector<char> vsShaderData(vs_shader_content, vs_shader_content + vs_shader_length);
  std::vector<char> fsShaderData(fs_shader_content, fs_shader_content + fs_shader_length);

  OXR::Context oxrContext(pApp);
  pApp->userData = &oxrContext;
  pApp->onAppCmd = handle_cmd; // Register an event handler for Android events

  oxrContext.initializeExtensions();
  oxrContext.createInstance();
  oxrContext.systemInfo();
  oxrContext.enumerateViewConfigurations();
  oxrContext.initGraphics();

  const std::vector<std::string> validationLayers = {"VK_LAYER_KHRONOS_validation"};
  VulkanCore::Context::enableDefaultFeatures();
  VulkanCore::Context::enableIndirectRenderingFeature();
  VulkanCore::Context::enable16bitFloatFeature();
  VulkanCore::Context::enableSynchronization2Feature();  // needed for acquire/release
  VulkanCore::Context::enableMultiView();
  VulkanCore::Context::enableFragmentDensityMapFeatures();

  VulkanCore::Context vkContext(VkApplicationInfo{
                                    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                    .pApplicationName = "Modern Vulkan Cookbook - OpenXR Example",
                                    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                    .apiVersion = VK_API_VERSION_1_2,
                                },
                                validationLayers, // Validation layer
                                oxrContext.vkInstanceExtensions(), // Instance extensions
                                true,
                                "OpenXR Vulkan Context");

  assert(vkContext.instance() != VK_NULL_HANDLE);

  const auto vkPhysicalDevice = oxrContext.findVkGraphicsDevice(vkContext.instance());
  vkContext.createVkDevice(vkPhysicalDevice, oxrContext.vkDeviceExtensions(),
                           VK_QUEUE_GRAPHICS_BIT);

  oxrContext.initializeSession(vkContext.instance(), vkContext.physicalDevice().vkPhysicalDevice(),
                               vkContext.device(),
                               vkContext.physicalDevice().graphicsFamilyIndex().value());

  oxrContext.enumerateReferenceSpaces();
  oxrContext.createSwapchains(vkContext);
  oxrContext.createSpaces();
  oxrContext.setInitialized();

  const auto deviceExtensions = vkContext.physicalDevice().extensions();
  const bool isFDMSupported = vkContext.physicalDevice().isFragmentDensityMapSupported();
  const bool isFDMOffsetSupported = vkContext.physicalDevice().isFragmentDensityMapOffsetSupported();

  // Create Command Manager
  auto commandMgr = vkContext.createGraphicsCommandQueue(3, 3);

  // Dynamic foveation
  // 3 or 6 maps, it depends if we're using single pass stereo or not
  const uint32_t numberOfFramebuffers = oxrContext.swapchain(0)->numImages() *
                                        (OXR::Context::kUseSinglePassStereo ? 1
                                                                            : OXR::Context::kNumViews);
  LOGI("Number of framebuffers: %d", numberOfFramebuffers);
  const glm::vec2 fdmTileSize = glm::vec2(std::max(32u,
                                                   vkContext.physicalDevice().fragmentDensityMapProperties().minFragmentDensityTexelSize.width),
                                          std::max(32u,
                                                   vkContext.physicalDevice().fragmentDensityMapProperties().minFragmentDensityTexelSize.height));
  std::vector<std::shared_ptr<VulkanCore::Texture>> fragmentDensityMap(numberOfFramebuffers);
#if defined(VK_EXT_fragment_density_map)
  if (isFDMSupported || isFDMOffsetSupported) {
    const bool extensionsEnabled = (std::find(deviceExtensions.begin(), deviceExtensions.end(),
                                              std::string(
                                                  VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME)) !=
                                    deviceExtensions.end()) ||
                                   (std::find(deviceExtensions.begin(), deviceExtensions.end(),
                                              std::string(
                                                  VK_QCOM_FRAGMENT_DENSITY_MAP_OFFSET_EXTENSION_NAME)) !=
                                    deviceExtensions.end());
    if (extensionsEnabled) {
      const glm::vec2 mapSize = glm::vec2(std::ceilf(
                                              oxrContext.swapchain(0)->viewport().recommendedImageRectWidth / fdmTileSize.x),
                                          std::ceilf(oxrContext.swapchain(
                                              0)->viewport().recommendedImageRectHeight /
                                                     fdmTileSize.y));

      // One layer. Pre-filled with half-density (128)
      std::vector<uint8_t> fdmData(mapSize.x * mapSize.y * 2, 32);
      constexpr uint16_t high_res_radius = 2;
      const glm::vec2 center = mapSize / 2.f;
      for (uint32_t x = 0; x < mapSize.x; ++x) {
        for (uint32_t y = 0; y < mapSize.y; ++y) {
          const float length = glm::length(glm::vec2(x, y) - center);
          if (length < high_res_radius) {
            const uint32_t index = (y * mapSize.x * 2) + x * 2;
            fdmData[index] = 255; // full density
            fdmData[index + 1] = 255; // full density
          }
        }
      }

      // create the density map
      for (uint32_t fdmIndex = 0; fdmIndex < numberOfFramebuffers; ++fdmIndex) {
        fragmentDensityMap[fdmIndex] =
            std::make_shared<VulkanCore::Texture>(vkContext, VK_IMAGE_TYPE_2D,
                                                  VK_FORMAT_R8G8_UNORM,
#if defined(VK_QCOM_fragment_density_map_offset)
                                                  isFDMOffsetSupported
                                                  ? VK_IMAGE_CREATE_FRAGMENT_DENSITY_MAP_OFFSET_BIT_QCOM
                                                  : static_cast<VkImageCreateFlags>(0),
#else
                static_cast<VkImageCreateFlags>(0),
#endif
                                                  VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT,
                                                  VkExtent3D{
                                                      static_cast<uint32_t>(mapSize.x),
                                                      static_cast<uint32_t>(mapSize.y),
                                                      1}, 1, 2,
                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                  false, VK_SAMPLE_COUNT_1_BIT,
                                                  "fragment density map", true,
                                                  VK_IMAGE_TILING_LINEAR);


        // Upload FDM data
        {
          auto textureUploadStagingBuffer = vkContext.createStagingBuffer(
              fragmentDensityMap[fdmIndex]->vkDeviceSize(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              "FDM data upload staging buffer");

          const auto commandBuffer = commandMgr.getCmdBufferToBegin();
          fragmentDensityMap[fdmIndex]->uploadOnly(commandBuffer, textureUploadStagingBuffer.get(),
                                                   fdmData.data(), 0);
          //fragmentDensityMap->uploadOnly(commandBuffer, textureUploadStagingBuffer.get(), fdmData.data(), 1);
          //fragmentDensityMap->transitionImageLayout(commandBuffer, VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT);
          // commandMgr.disposeWhenSubmitCompletes(std::move(textureUploadStagingBuffer));
          commandMgr.endCmdBuffer(commandBuffer);

          VkPipelineStageFlags flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
          const auto submitInfo =
              vkContext.swapchain()->createSubmitInfo(&commandBuffer, &flags, false, false);
          commandMgr.submit(&submitInfo);
          commandMgr.waitUntilSubmitIsComplete();


          const auto commandBuffer2 = commandMgr.getCmdBufferToBegin();
          //fragmentDensityMap->uploadOnly(commandBuffer, textureUploadStagingBuffer.get(), fdmData.data(), 0);
          fragmentDensityMap[fdmIndex]->uploadOnly(commandBuffer2, textureUploadStagingBuffer.get(),
                                                   fdmData.data(), 1);
          fragmentDensityMap[fdmIndex]->transitionImageLayout(commandBuffer2,
                                                              VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT);
          commandMgr.disposeWhenSubmitCompletes(std::move(textureUploadStagingBuffer));
          commandMgr.endCmdBuffer(commandBuffer2);

          const auto submitInfo2 =
              vkContext.swapchain()->createSubmitInfo(&commandBuffer2, &flags, false, false);
          commandMgr.submit(&submitInfo2);
          commandMgr.waitUntilSubmitIsComplete();

        }
      }
    }
  }
#endif

  const auto vertexShaderModule = vkContext.createShaderModule(vsShaderData, "main",
                                                               VK_SHADER_STAGE_VERTEX_BIT,
                                                               "main vertex shader");

  const auto fragShaderModule = vkContext.createShaderModule(fsShaderData, "main",
                                                             VK_SHADER_STAGE_FRAGMENT_BIT,
                                                             "main fragment shader");

  // Load GLB
  AAsset *glb_asset = AAssetManager_open(assetMgr, "gltf/smallbistro.glb", AASSET_MODE_BUFFER);
  size_t glb_size = AAsset_getLength(glb_asset);
  std::vector<char> glb_content;// = new char[vs_shader_length + 1];
  glb_content.resize(glb_size);
  AAsset_read(glb_asset, glb_content.data(), glb_size);

  EngineCore::GLBLoader glbLoader;
  std::shared_ptr<EngineCore::Model> model = glbLoader.load(glb_content);

  auto emptyTexture =
      vkContext.createTexture(VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, 0,
                              VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                              VkExtent3D{
                                  .width = static_cast<uint32_t>(1),
                                  .height = static_cast<uint32_t>(1),
                                  .depth = static_cast<uint32_t>(1.0),
                              },
                              1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false,
                              VK_SAMPLE_COUNT_1_BIT, "empty");

  uint32_t numMeshes = 0;
  std::vector<std::shared_ptr<VulkanCore::Buffer>> buffers;
  std::vector<std::shared_ptr<VulkanCore::Texture>> textures;
  std::vector<std::shared_ptr<VulkanCore::Sampler>> samplers;
  samplers.emplace_back(vkContext.createSampler(
      VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT,
      VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, 10.0f,
      "empty"));

  {
    const auto commandBuffer = commandMgr.getCmdBufferToBegin();
    EngineCore::convertModel2OneBuffer(vkContext, commandMgr, commandBuffer,
                                       *model.get(), buffers, textures, samplers, true);
    // textures.resize(model->textures.size(), emptyTexture);
    numMeshes = model->meshes.size();

    commandMgr.endCmdBuffer(commandBuffer);

    VkPipelineStageFlags flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
    const auto submitInfo =
        vkContext.swapchain()->createSubmitInfo(&commandBuffer, &flags, false, false);
    commandMgr.submit(&submitInfo);
    commandMgr.waitUntilSubmitIsComplete();
  }

  constexpr uint32_t CAMERA_SET = 0;
  constexpr uint32_t TEXTURES_AND_SAMPLER_SET = 1;
  constexpr uint32_t STORAGE_BUFFER_SET =
      2;  // storing vertex/index/indirect/material buffer in array
  constexpr uint32_t BINDING_0 = 0;
  constexpr uint32_t BINDING_1 = 1;
  constexpr uint32_t BINDING_2 = 2;
  constexpr uint32_t BINDING_3 = 3;
  const std::vector<VulkanCore::Pipeline::SetDescriptor> setLayout = {
      {
          .set_ = CAMERA_SET,  // set number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding{
                      BINDING_0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
              },
      },
      {
          .set_ = TEXTURES_AND_SAMPLER_SET,  // set number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding{
                      BINDING_0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, uint32_t(textures.size()),
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
                  VkDescriptorSetLayoutBinding{
                      BINDING_1, VK_DESCRIPTOR_TYPE_SAMPLER, uint32_t(samplers.size()),
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
              },
      },
      {
          .set_ = STORAGE_BUFFER_SET,
          .bindings_ =
              {
                  VkDescriptorSetLayoutBinding{
                      BINDING_0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4,
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
              },
      },
  };

  glm::vec4 eyeGazeCursorPosition = {
      oxrContext.swapchain(0)->viewport().recommendedImageRectWidth / 2.f,
      oxrContext.swapchain(0)->viewport().recommendedImageRectHeight / 2.f,
      oxrContext.swapchain(0)->viewport().recommendedImageRectWidth / 2.f,
      oxrContext.swapchain(0)->viewport().recommendedImageRectHeight / 2.f};

  std::vector<VkPushConstantRange> pushConstantRanges;
  pushConstantRanges.emplace_back(VkPushConstantRange{
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof(glm::vec4),
  });

  const VulkanCore::Pipeline::GraphicsPipelineDescriptor gpDesc = {
      .sets_ = setLayout,
      .vertexShader_ = vertexShaderModule,
      .fragmentShader_ = fragShaderModule,
      .pushConstants_ = pushConstantRanges,
      .dynamicStates_ = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR},
      //VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE},
      .colorTextureFormats = {VK_FORMAT_R8G8B8A8_UNORM},
      .depthTextureFormat = VK_FORMAT_D24_UNORM_S8_UINT,
      .sampleCount = VK_SAMPLE_COUNT_1_BIT,
      .cullMode = VK_CULL_MODE_NONE,
      .viewport = VkExtent2D{oxrContext.swapchain(0)->viewport().recommendedImageRectWidth,
                             oxrContext.swapchain(
                                 0)->viewport().recommendedImageRectHeight},//context.swapchain()->extent(),
      .depthTestEnable = true,
      .depthWriteEnable = true,
      .depthCompareOperation = VK_COMPARE_OP_LESS,
  };

  // Vulkan objects initialization *****************************************************************

  // Create Framebuffers
  std::vector<std::shared_ptr<VulkanCore::Framebuffer>> swapchain_framebuffers(
      numberOfFramebuffers);

  // Create Render Pass
  std::shared_ptr<VulkanCore::RenderPass> renderPass;
#if defined(VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME)
  if (isFDMSupported || isFDMOffsetSupported) {
    renderPass = std::make_shared<VulkanCore::RenderPass>(vkContext, std::vector<VkFormat>(
                                                              {VK_FORMAT_R8G8B8A8_UNORM,
                                                               VK_FORMAT_R8G8_UNORM,
                                                               VK_FORMAT_D24_UNORM_S8_UINT,
                                                              }),
                                                          std::vector<VkImageLayout>(
                                                              {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                               VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT,
                                                               VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                                              }),
                                                          std::vector<VkImageLayout>(
                                                              {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                               VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT,
                                                               VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                                              }),
                                                          std::vector<VkAttachmentLoadOp>(
                                                              {VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                               VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                               VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                              }),
                                                          std::vector<VkAttachmentStoreOp>(
                                                              {VK_ATTACHMENT_STORE_OP_STORE,
                                                               VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                               VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                              }),
                                                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                          std::vector<uint32_t>(),
                                                          2, 1, UINT32_MAX,
                                                          VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                          VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                          OXR::Context::kUseSinglePassStereo,
                                                          "OpenXR Main");
  } else {
#endif
    renderPass = std::make_shared<VulkanCore::RenderPass>(vkContext, std::vector<VkFormat>(
                                                              {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_D24_UNORM_S8_UINT}),
                                                          std::vector<VkImageLayout>(
                                                              {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                               VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}),
                                                          std::vector<VkImageLayout>(
                                                              {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                               VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}),
                                                          std::vector<VkAttachmentLoadOp>(
                                                              {VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                               VK_ATTACHMENT_LOAD_OP_CLEAR}),
                                                          std::vector<VkAttachmentStoreOp>(
                                                              {VK_ATTACHMENT_STORE_OP_STORE,
                                                               VK_ATTACHMENT_STORE_OP_DONT_CARE}),
                                                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                          std::vector<uint32_t>(),
                                                          1, UINT32_MAX,
                                                          VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                          VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                          OXR::Context::kUseSinglePassStereo,
                                                          "OpenXR Main");
#if defined(VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME)
  }
#endif

  const VkViewport viewport = {
      .x = 0.0f,
      .y = static_cast<float>(oxrContext.swapchain(0)->colorTexture(0)->vkExtents().height),
      .width = static_cast<float>(oxrContext.swapchain(0)->colorTexture(0)->vkExtents().width),
      .height = -static_cast<float>(oxrContext.swapchain(0)->colorTexture(0)->vkExtents().height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };

  // bistro
  // EngineCore::Camera camera(glm::vec3(-9.f, 2.f, 2.f));
  // ducky duck
  EngineCore::Camera camera(glm::vec3(72.5f, 139.6f, -223.15f));
  Transforms transform = {
      .mvp_left = glm::mat4(0.01f),
      .mvp_right = glm::mat4(0.01f),
  };
  EngineCore::RingBuffer cameraBuffer(3, vkContext, sizeof(Transforms));

  // Create Graphics Pipeline
#pragma region Pipeline initialization
  auto pipeline = vkContext.createGraphicsPipeline(
      gpDesc,
      renderPass->vkRenderPass(), "main");
  pipeline->allocateDescriptors({
                                    {.set_ = CAMERA_SET, .count_ = 3},
                                    {.set_ = TEXTURES_AND_SAMPLER_SET, .count_ = 1},
                                    {.set_ = STORAGE_BUFFER_SET, .count_ = 1},
                                });
  pipeline->bindResource(CAMERA_SET, BINDING_0, 0, cameraBuffer.buffer(0), 0,
                         sizeof(Transforms), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  pipeline->bindResource(CAMERA_SET, BINDING_0, 1, cameraBuffer.buffer(1), 0,
                         sizeof(Transforms), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  pipeline->bindResource(CAMERA_SET, BINDING_0, 2, cameraBuffer.buffer(2), 0,
                         sizeof(Transforms), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  pipeline->bindResource(STORAGE_BUFFER_SET, BINDING_0, 0,
                         {buffers[0], buffers[1], buffers[3],
                          buffers[2]},  // vertex, index, indirect, material
                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  pipeline->bindResource(TEXTURES_AND_SAMPLER_SET, BINDING_0, 0, std::span(textures));
  pipeline->bindResource(TEXTURES_AND_SAMPLER_SET, BINDING_1, 0, std::span(samplers));
#pragma endregion
  // Set input event filters (set it to NULL if the app wants to process all inputs).
  // Note that for key inputs, this example uses the default default_key_filter()
  // implemented in android_native_app_glue.c.
  // android_app_set_motion_event_filter(pApp, motion_event_filter_func);
  const auto extents3D = oxrContext.swapchain(0)->colorTexture(0)->vkExtents();
  VkExtent2D extents2D = {extents3D.width, extents3D.height};
  const VkRect2D renderArea = {.offset = {.x = 0, .y = 0}, .extent = extents2D};

  float r = 0.f, g = 0.0f, b = 0.45f;
  const std::array<VkClearValue, 3> clearValues = {VkClearValue{.color = {r, g, b, 0.0f}},
                                                   VkClearValue{.color = {r, g, b, 0.0f}},
                                                   VkClearValue{.depthStencil = {1.0f}},
  };

  // This sets up a typical game/event loop. It will run until the app is destroyed.
  size_t frame = 0;
  do {
    for (;;) {
      int events;
      android_poll_source *pSource = nullptr;
      // If the timeout is zero, returns immediately without blocking.
      // If the timeout is negative, waits indefinitely until an event appears.
      const int timeout =
          (!oxrContext.resumed() && !oxrContext.sessionActive() && pApp->destroyRequested == 0) ? -1
                                                                                                : 0;
      // Process all pending events before running game logic.
      if (ALooper_pollAll(timeout, nullptr, &events, (void **) &pSource) >= 0) {
        if (pSource) {
          pSource->process(pApp, pSource);
        }
      } else {
        break;
      }
    }

    oxrContext.handleXrEvents();
    if (/*!oxrContext.resumed() ||*/ !oxrContext.sessionActive()) {
      continue;
    }

    auto frameState = oxrContext.beginFrame();
    if (frameState.shouldRender == XR_FALSE) {
      oxrContext.endFrame(frameState);
      continue;
    }

    // render
    auto commandBuffer = commandMgr.getCmdBufferToBegin();

    const auto numViews = OXR::Context::kUseSinglePassStereo ? 1 : OXR::Context::kNumViews;
    const auto numSwapchainImages = oxrContext.swapchain(0)->numImages();
    for (uint32_t i = 0; i < numViews; ++i) {
      auto texture = oxrContext.swapchain(i)->getSurfaceTextures();
      const auto swapchainImageIndex = oxrContext.swapchain(i)->currentImageIndex();
      const auto swapchainImageIndexFB = numSwapchainImages * i + swapchainImageIndex;
      LOGI("View: %d - Swapchain Image Index: %d - Swapchain Image Index FB: %d - Frame: %zu", i,
           swapchainImageIndex, swapchainImageIndexFB, frame);

      texture.color_->transitionImageLayout(commandBuffer,
                                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      texture.depth_->transitionImageLayout(commandBuffer,
                                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

      // Create the framebuffer the first time we get here, once for each
      // swapchain image
      if (swapchain_framebuffers[swapchainImageIndexFB] == nullptr) {
#if defined(VK_EXT_fragment_density_map)
        if (isFDMSupported || isFDMOffsetSupported) {
          swapchain_framebuffers[swapchainImageIndexFB] = vkContext.createFramebuffer(
              renderPass->vkRenderPass(),
              {texture.color_, fragmentDensityMap[swapchainImageIndexFB]}, texture.depth_,
              nullptr);
        } else {
#endif
          swapchain_framebuffers[swapchainImageIndexFB] = vkContext.createFramebuffer(
              renderPass->vkRenderPass(),
              {texture.color_}, texture.depth_,
              nullptr);
#if defined(VK_EXT_fragment_density_map)
        }
#endif
      }

      // Begin Render Pass
      const VkRenderPassBeginInfo renderpassInfo = {
          .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
          .renderPass = renderPass->vkRenderPass(),
          .framebuffer = swapchain_framebuffers[swapchainImageIndexFB]->vkFramebuffer(),
          .renderArea = {.offset =
              {
                  0,
                  0,
              },
              .extent =
                  {
                      .width = texture.color_->vkExtents().width,
                      .height = texture.depth_->vkExtents().height,
                  }},
          .clearValueCount = static_cast<uint32_t>(clearValues.size()),
          .pClearValues = clearValues.data(),
      };

      if (isFDMOffsetSupported) {
        const VkSubpassBeginInfo subpassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO,
            .contents = VK_SUBPASS_CONTENTS_INLINE,
        };
        vkCmdBeginRenderPass2KHR(commandBuffer, &renderpassInfo, &subpassBeginInfo);
      } else {
        vkCmdBeginRenderPass(commandBuffer, &renderpassInfo, VK_SUBPASS_CONTENTS_INLINE);
      }

#pragma region Dynamic States
      { ;
        const VkViewport viewport2 = {
            .x = 0.0f,
            .y = static_cast<float>(texture.color_->vkExtents().height),
            .width = static_cast<float>(texture.color_->vkExtents().width),
            .height = -static_cast<float>(texture.color_->vkExtents().height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport2);
        const VkRect2D scissor = {
            .offset =
                {
                    0,
                    0,
                },
            .extent = VkExtent2D{texture.color_->vkExtents().width,
                                 texture.color_->vkExtents().height},
        };
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        // vkCmdSetDepthTestEnable(commandBuffer, VK_TRUE);
      }
#pragma endregion

      // all other properties of transform are set to identity
      transform.mvp_left = oxrContext.mvp(i);
      transform.mvp_right = oxrContext.mvp(1);
      cameraBuffer.buffer()->copyDataToBuffer(&transform, sizeof(Transforms));

      if (i == 0) {
        pipeline->bind(commandBuffer);
        pipeline->bindDescriptorSets(commandBuffer,
                                     {
                                         {.set = CAMERA_SET, .bindIdx = (uint32_t) swapchainImageIndex},
                                         {.set = TEXTURES_AND_SAMPLER_SET, .bindIdx = 0},
                                         {.set = STORAGE_BUFFER_SET, .bindIdx = 0},
                                     });
        pipeline->updateDescriptorSets();
      }

      const auto eyeGazeScreenPosLeft = oxrContext.eyeGazeScreenPos(
          0);//oxrContext.eyeGazeScreenPosNDC();
      const auto eyeGazeScreenPosRight = oxrContext.eyeGazeScreenPos(
          1);//oxrContext.eyeGazeScreenPosNDC();
      eyeGazeCursorPosition.x = eyeGazeScreenPosLeft.x;
      eyeGazeCursorPosition.y = eyeGazeScreenPosLeft.y;
      eyeGazeCursorPosition.z = eyeGazeScreenPosRight.x;
      eyeGazeCursorPosition.w = eyeGazeScreenPosRight.y;

      vkCmdPushConstants(
          commandBuffer,
          pipeline->vkPipelineLayout(),
          VK_SHADER_STAGE_FRAGMENT_BIT,
          0,
          sizeof(glm::vec4),
          &eyeGazeCursorPosition);

      vkCmdBindIndexBuffer(commandBuffer, buffers[1]->vkBuffer(), 0, VK_INDEX_TYPE_UINT32);

      vkCmdDrawIndexedIndirect(commandBuffer, buffers[3]->vkBuffer(), 0, numMeshes,
                               sizeof(EngineCore::IndirectDrawCommandAndMeshData));
      // vkCmdEndRenderPass(commandBuffer);

#if defined(VK_QCOM_fragment_density_map_offset)
      if (isFDMOffsetSupported) {
        const glm::vec2 swapchainImageCenter = glm::vec2(oxrContext.swapchain(0)->viewport().recommendedImageRectWidth / 2.f,
                                                         oxrContext.swapchain(0)->viewport().recommendedImageRectHeight / 2.f);
        const glm::vec2 offsetInPixelsLeft = glm::vec2(eyeGazeScreenPosLeft) - swapchainImageCenter;
        const glm::vec2 offsetInPixelsRight = glm::vec2(eyeGazeScreenPosRight) - swapchainImageCenter;
        const glm::vec2 fdmOffsetGranularity = glm::vec2(vkContext.physicalDevice().fragmentDensityMapOffsetProperties().fragmentDensityOffsetGranularity.width,
                                                         vkContext.physicalDevice().fragmentDensityMapOffsetProperties().fragmentDensityOffsetGranularity.height);

        const glm::vec2 offsetLeft = offsetInPixelsLeft;
        const glm::vec2 offsetRight = offsetInPixelsRight;

        const std::array<VkOffset2D, 2> offsets = {
            VkOffset2D{static_cast<int32_t>(offsetLeft.x), static_cast<int32_t>(offsetLeft.y)},
            VkOffset2D{static_cast<int32_t>(offsetRight.x), static_cast<int32_t>(offsetRight.y)}};
        const VkSubpassFragmentDensityMapOffsetEndInfoQCOM offsetInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBPASS_FRAGMENT_DENSITY_MAP_OFFSET_END_INFO_QCOM,
            .fragmentDensityOffsetCount = offsets.size(),    // 1 for each layer/multiview view
            .pFragmentDensityOffsets = offsets.data(), // aligned to fragmentDensityOffsetGranularity
        };
        const VkSubpassEndInfo subpassEndInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBPASS_END_INFO,
            .pNext = &offsetInfo,
        };
        vkCmdEndRenderPass2KHR(commandBuffer, &subpassEndInfo);
      } else {
#endif
        vkCmdEndRenderPass(commandBuffer);

#if defined(VK_QCOM_fragment_density_map_offset)
      }
#endif

    } // end of for loop for kNumViews

    // commandMgr.endCmdBuffer(commandBuffer);
    const auto result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
      LOGE("Error submitting command buffer: %d on frame %zu", result, frame);
      break;
    }
    constexpr VkPipelineStageFlags flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const auto submitInfo = vkContext.swapchain()->createSubmitInfo(&commandBuffer, &flags, false,
                                                                    false);

    commandMgr.submit(&submitInfo);
    commandMgr.goToNextCmdBuffer();

    oxrContext.swapchain(0)->releaseSwapchainImages();
    if (!OXR::Context::kUseSinglePassStereo) {
      oxrContext.swapchain(1)->releaseSwapchainImages();
    }

    oxrContext.endFrame(frameState);

    cameraBuffer.moveToNextBuffer();

    frame++;
  } while (!pApp->destroyRequested);

  pApp->activity->vm->DetachCurrentThread();
}
}