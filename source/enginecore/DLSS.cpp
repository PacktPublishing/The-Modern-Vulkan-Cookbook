#include "DLSS.hpp"

#include <nvsdk_ngx.h>
#include <nvsdk_ngx_defs.h>
#include <nvsdk_ngx_helpers.h>
#include <nvsdk_ngx_helpers_vk.h>
#include <nvsdk_ngx_params.h>
#include <nvsdk_ngx_vk.h>

#include "vulkancore/Utility.hpp"

NVSDK_NGX_Application_Identifier const& appIdentifier() {
  static NVSDK_NGX_Application_Identifier applicationId{};
  applicationId.IdentifierType = NVSDK_NGX_Application_Identifier_Type_Application_Id;
  applicationId.v.ProjectDesc.EngineType = NVSDK_NGX_ENGINE_TYPE_CUSTOM;
  applicationId.v.ProjectDesc.ProjectId = "DLSS-Demo";
  applicationId.v.ProjectDesc.EngineVersion = "1.0.0";
  return applicationId;
}

static void NVSDK_CONV logCallback(const char* message,
                                   NVSDK_NGX_Logging_Level loggingLevel,
                                   NVSDK_NGX_Feature sourceComponent) {
  std::cerr << "DLSS Callback " << message;
}

namespace EngineCore {
DLSS::DLSS(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device) {
  NVSDK_NGX_FeatureCommonInfo featureCommonInfo = {};
  featureCommonInfo.LoggingInfo.LoggingCallback = logCallback;
  featureCommonInfo.LoggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_VERBOSE;
  featureCommonInfo.LoggingInfo.DisableOtherLoggingSinks = true;

  NVSDK_NGX_Result initResult =
      NVSDK_NGX_VULKAN_Init(appIdentifier().v.ApplicationId, L".", instance,
                            physicalDevice, device, nullptr, nullptr, &featureCommonInfo);
  if (NVSDK_NGX_FAILED(initResult)) {
    supported_ = false;
    return;
  }

  auto result = NVSDK_NGX_VULKAN_GetCapabilityParameters(&paramsDLSS_);

  int dlssAvailable = 0;

  NVSDK_NGX_Result dlssCheckSupportResult =
      paramsDLSS_->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &dlssAvailable);
  if (NVSDK_NGX_FAILED(dlssCheckSupportResult)) {
    supported_ = false;
    return;
  }

  supported_ = dlssAvailable;
}

void DLSS::init(int currentWidth, int currentHeight, float upScaleFactor,
                VulkanCore::CommandQueueManager& commandQueueManager) {
  upScaleFactor_ = upScaleFactor;
  NVSDK_NGX_PerfQuality_Value dlssQuality = NVSDK_NGX_PerfQuality_Value_MaxQuality;

  uint32_t optimalRenderWidth, optimalRenderHeight;
  float recommendedSharpness;

  uint32_t minRenderWidth, minRenderHeight;
  uint32_t maxRenderWidth, maxRenderHeight;

  NVSDK_NGX_Result result = NGX_DLSS_GET_OPTIMAL_SETTINGS(
      paramsDLSS_, currentWidth, currentHeight, dlssQuality, &optimalRenderWidth,
      &optimalRenderHeight, &minRenderWidth, &minRenderHeight, &maxRenderWidth,
      &maxRenderHeight, &recommendedSharpness);

  int dlssCreateFeatureFlags = NVSDK_NGX_DLSS_Feature_Flags_None;

  // Motion vectors are typically calculated at the same resolution as the input color
  // frame (i.e. at the render resolution). If the rendering engine supports calculating
  // motion vectors at the display / output resolution and dilating the motion vectors,
  // DLSS can accept those by setting the flag to "0". This is preferred, though uncommon,
  // and can result in higher quality antialiasing of moving objects and less blurring of
  // small objects and thin details. For clarity, if standard input resolution motion
  // vectors are sent they do not need to be dilated, DLSS dilates them internally. If
  // display resolution motion vectors are sent, they must be dilated.
  dlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;

  // Set this flag to "1" when the motion vectors do include sub-pixel jitter. DLSS then
  // internally subtracts jitter from the motion vectors using the jitter offset values
  // that are provided during the Evaluate call. When set to "0", DLSS uses the motion
  // vectors directly without any adjustment.
  // dlssCreateFeatureFlags |=
  //    motionVectorsAreJittered ? NVSDK_NGX_DLSS_Feature_Flags_MVJittered : 0;

  /*dlssCreateFeatureFlags |= isHDR ? NVSDK_NGX_DLSS_Feature_Flags_IsHDR : 0;*/

  // require in case of inverse z
  // dlssCreateFeatureFlags |= inverseZ ? NVSDK_NGX_DLSS_Feature_Flags_DepthInverted : 0;

  dlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_DoSharpening;

  // We don't use auto-exposure, for now.
  // dlssCreateFeatureFlags |= enableAutoExposure ?
  // NVSDK_NGX_DLSS_Feature_Flags_AutoExposure : 0;

  NVSDK_NGX_DLSS_Create_Params dlssCreateParams{
      .Feature =
          {
              .InWidth = (unsigned int)(currentWidth),
              .InHeight = (unsigned int)(currentHeight),
              .InTargetWidth = (unsigned int)(currentWidth * upScaleFactor),
              .InTargetHeight = (unsigned int)(currentHeight * upScaleFactor),
              .InPerfQualityValue = NVSDK_NGX_PerfQuality_Value_MaxQuality,
          },
      .InFeatureCreateFlags = dlssCreateFeatureFlags,
  };

  auto commmandBuffer = commandQueueManager.getCmdBufferToBegin();

  constexpr unsigned int creationNodeMask = 1;
  constexpr unsigned int visibilityNodeMask = 1;

  NVSDK_NGX_Result createDlssResult =
      NGX_VULKAN_CREATE_DLSS_EXT(commmandBuffer, creationNodeMask, visibilityNodeMask,
                                 &dlssFeatureHandle_, paramsDLSS_, &dlssCreateParams);

  ASSERT(createDlssResult == NVSDK_NGX_Result_Success,
         "Failed to create NVSDK NGX DLSS feature");

  commandQueueManager.endCmdBuffer(commmandBuffer);

  VkSubmitInfo submitInfo{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &commmandBuffer,
  };
  commandQueueManager.submit(&submitInfo);

  commandQueueManager.waitUntilSubmitIsComplete();
}

void DLSS::render(VkCommandBuffer commandBuffer, VulkanCore::Texture& inColorTexture,
                  VulkanCore::Texture& inDepthTexture,
                  VulkanCore::Texture& inMotionVectorTexture,
                  VulkanCore::Texture& outColorTexture, glm::vec2 cameraJitter) {
  NVSDK_NGX_Resource_VK inColorResource = NVSDK_NGX_Create_ImageView_Resource_VK(
      inColorTexture.vkImageView(), inColorTexture.vkImage(),
      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}, VK_FORMAT_UNDEFINED,
      inColorTexture.vkExtents().width, inColorTexture.vkExtents().height, true);

  NVSDK_NGX_Resource_VK outColorResource = NVSDK_NGX_Create_ImageView_Resource_VK(
      outColorTexture.vkImageView(), outColorTexture.vkImage(),
      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}, VK_FORMAT_UNDEFINED,
      outColorTexture.vkExtents().width, outColorTexture.vkExtents().height, true);

  NVSDK_NGX_Resource_VK depthResource = NVSDK_NGX_Create_ImageView_Resource_VK(
      inDepthTexture.vkImageView(), inDepthTexture.vkImage(),
      {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1}, VK_FORMAT_UNDEFINED,
      inDepthTexture.vkExtents().width, inDepthTexture.vkExtents().height, true);

  NVSDK_NGX_Resource_VK motionVectorResource = NVSDK_NGX_Create_ImageView_Resource_VK(
      inMotionVectorTexture.vkImageView(), inMotionVectorTexture.vkImage(),
      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}, VK_FORMAT_UNDEFINED,
      inMotionVectorTexture.vkExtents().width, inMotionVectorTexture.vkExtents().height,
      true);

  outColorTexture.transitionImageLayout(commandBuffer, VK_IMAGE_LAYOUT_GENERAL);

  NVSDK_NGX_VK_DLSS_Eval_Params evalParams = {
      .Feature =
          {
              .pInColor = &inColorResource,
              .pInOutput = &outColorResource,
              .InSharpness = 1.0,
          },
      .pInDepth = &depthResource,
      .pInMotionVectors = &motionVectorResource,
      .InJitterOffsetX = cameraJitter.x,
      .InJitterOffsetY = cameraJitter.y,
      .InRenderSubrectDimensions =
          {
              .Width = static_cast<unsigned int>(inColorTexture.vkExtents().width),
              .Height = static_cast<unsigned int>(inColorTexture.vkExtents().height),
          },
      .InReset = 0,
      .InMVScaleX = -1.0f * inColorResource.Resource.ImageViewInfo.Width,
      .InMVScaleY = -1.0f * inColorResource.Resource.ImageViewInfo.Height,
      .pInExposureTexture = nullptr,
  };

  NVSDK_NGX_Result result = NGX_VULKAN_EVALUATE_DLSS_EXT(
      commandBuffer, dlssFeatureHandle_, paramsDLSS_, &evalParams);

  ASSERT(result == NVSDK_NGX_Result_Success, "Failed to evaluate DLSS feature");

  if (result != NVSDK_NGX_Result_Success) {
    auto store = GetNGXResultAsString(result);
  }

  outColorTexture.transitionImageLayout(commandBuffer,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

DLSS::~DLSS() {
  if (supported_) {
    NVSDK_NGX_VULKAN_DestroyParameters(paramsDLSS_);
    NVSDK_NGX_VULKAN_Shutdown1(nullptr);
  }
}

void DLSS::requiredExtensions(std::vector<std::string>& instanceExtensions,
                              std::vector<std::string>& deviceExtensions) {
  unsigned int instanceExtCount;
  const char** instanceExt;
  unsigned int deviceExtCount;
  const char** deviceExt;
  auto result = NVSDK_NGX_VULKAN_RequiredExtensions(&instanceExtCount, &instanceExt,
                                                    &deviceExtCount, &deviceExt);

  if (result != NVSDK_NGX_Result_Success) {
    // assert?
    return;
  }

  for (int i = 0; i < instanceExtCount; ++i) {
    if (std::find(instanceExtensions.begin(), instanceExtensions.end(), instanceExt[i]) ==
        instanceExtensions.end()) {
      instanceExtensions.push_back(instanceExt[i]);
    }
  }
  for (int i = 0; i < deviceExtCount; ++i) {
    if (std::find(deviceExtensions.begin(), deviceExtensions.end(), deviceExt[i]) ==
        deviceExtensions.end()) {
      deviceExtensions.push_back(deviceExt[i]);
      if (deviceExtensions.back() ==
          "VK_EXT_buffer_device_address") {  // we are using 1.3, this extension has been
                                             // promoted
        deviceExtensions.pop_back();
      }
    }
  }
}
}  // namespace EngineCore
