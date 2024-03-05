#include "AsyncDataUploader.hpp"

#include "vulkancore/Texture.hpp"

namespace EngineCore {

AsyncDataUploader::AsyncDataUploader(VulkanCore::Context& context,
                                     std::function<void(int, int)> textureReadyCallback)
    : context_(context),
      transferCommandQueueMgr_(context.createTransferCommandQueue(
          1, 1, "secondary thread transfer command queue")),
      graphicsCommandQueueMgr_(context.createGraphicsCommandQueue(
          1, 1, "secondary thread graphics command queue", 1)),
      textureReadyCallback_(textureReadyCallback) {}

AsyncDataUploader::~AsyncDataUploader() {
  closeThreads_ = true;
  textureGPUDataUploadThread_.join();
  textureMipGenThread_.join();

  for (auto& semaphore : semaphores_) {
    vkDestroySemaphore(context_.device(), semaphore, nullptr);
  }
}

void AsyncDataUploader::startProcessing() {
  // should be able to replace this with BS_Thread_pool
  textureGPUDataUploadThread_ = std::thread([this]() {
    while (!closeThreads_) {
      if (textureLoadTasks_.size() > 0) {
        // pop &  do stuff
        auto textureLoadTask = textureLoadTasks_.front();
        textureLoadTasks_.pop_front();
        auto textureUploadStagingBuffer = context_.createStagingBuffer(
            textureLoadTask.texture->vkDeviceSize(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            "Async texture upload staging buffer");

        const auto commandBuffer = transferCommandQueueMgr_.getCmdBufferToBegin();

        textureLoadTask.texture->uploadOnly(
            commandBuffer, textureUploadStagingBuffer.get(), textureLoadTask.data);

        textureLoadTask.texture->addReleaseBarrier(
            commandBuffer, transferCommandQueueMgr_.queueFamilyIndex(),
            graphicsCommandQueueMgr_.queueFamilyIndex());

        transferCommandQueueMgr_.endCmdBuffer(commandBuffer);

        transferCommandQueueMgr_.disposeWhenSubmitCompletes(
            std::move(textureUploadStagingBuffer));

        VkSemaphore graphicsSemaphore;
        const VkSemaphoreCreateInfo semaphoreInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };
        VK_CHECK(vkCreateSemaphore(context_.device(), &semaphoreInfo, nullptr,
                                   &graphicsSemaphore));

        VkPipelineStageFlags flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
        auto submitInfo =
            context_.swapchain()->createSubmitInfo(&commandBuffer, &flags, false, false);
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &graphicsSemaphore;
        transferCommandQueueMgr_.submit(&submitInfo);

        textureMipGenerationTasks_.push_back(
            {textureLoadTask.texture, graphicsSemaphore, textureLoadTask.index});
      }
    }
  });

  textureMipGenThread_ = std::thread([this]() {
    while (!closeThreads_) {
      if (textureMipGenerationTasks_.size() > 0) {
        // pop &  do stuff
        auto task = textureMipGenerationTasks_.front();
        textureMipGenerationTasks_.pop_front();

        auto commandBuffer = graphicsCommandQueueMgr_.getCmdBufferToBegin();
        task.texture->addAcquireBarrier(commandBuffer,
                                        transferCommandQueueMgr_.queueFamilyIndex(),
                                        graphicsCommandQueueMgr_.queueFamilyIndex());
        task.texture->generateMips(commandBuffer);

        graphicsCommandQueueMgr_.endCmdBuffer(commandBuffer);
        VkPipelineStageFlags flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        auto submitInfo =
            context_.swapchain()->createSubmitInfo(&commandBuffer, &flags, false, false);
        submitInfo.pWaitSemaphores = &task.graphicsSemaphore;
        submitInfo.waitSemaphoreCount = 1;
        graphicsCommandQueueMgr_.submit(&submitInfo);

        semaphores_.push_back(task.graphicsSemaphore);

        textureReadyCallback_(task.index, 0);
      }
    }
  });
}

void AsyncDataUploader::queueTextureUploadTasks(const TextureLoadTask& textureLoadTask) {
  textureLoadTasks_.push_back(textureLoadTask);
}

}  // namespace EngineCore
