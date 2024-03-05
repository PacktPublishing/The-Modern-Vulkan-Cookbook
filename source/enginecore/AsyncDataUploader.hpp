#pragma once

#include <functional>
#include <mutex>
#include <queue>
#include <thread>

#include "vulkancore/CommandQueueManager.hpp"
#include "vulkancore/Context.hpp"

namespace EngineCore {

template <typename T>
class SharedQueue {
 public:
  SharedQueue();
  ~SharedQueue();

  T& front();
  void pop_front();

  void push_back(const T& item);
  void push_back(T&& item);

  int size();

 private:
  std::deque<T> queue_;
  std::mutex mutex_;
  std::condition_variable cond_;
};

template <typename T>
SharedQueue<T>::SharedQueue() {}

template <typename T>
SharedQueue<T>::~SharedQueue() {}

template <typename T>
T& SharedQueue<T>::front() {
  std::unique_lock<std::mutex> mlock(mutex_);
  while (queue_.empty()) {
    cond_.wait(mlock);
  }
  return queue_.front();
}

template <typename T>
void SharedQueue<T>::pop_front() {
  std::unique_lock<std::mutex> mlock(mutex_);
  while (queue_.empty()) {
    cond_.wait(mlock);
  }
  queue_.pop_front();
}

template <typename T>
void SharedQueue<T>::push_back(const T& item) {
  std::unique_lock<std::mutex> mlock(mutex_);
  queue_.push_back(item);
  mlock.unlock();      // unlock before notificiation to minimize mutex con
  cond_.notify_one();  // notify one waiting thread
}

template <typename T>
void SharedQueue<T>::push_back(T&& item) {
  std::unique_lock<std::mutex> mlock(mutex_);
  queue_.push_back(std::move(item));
  mlock.unlock();      // unlock before notificiation to minimize mutex con
  cond_.notify_one();  // notify one waiting thread
}

template <typename T>
int SharedQueue<T>::size() {
  std::unique_lock<std::mutex> mlock(mutex_);
  int size = queue_.size();
  mlock.unlock();
  return size;
}

// currently mostly use to upload textures data using transfer queue, but can be
// used to upload other data async as well
class AsyncDataUploader {
 public:
  AsyncDataUploader(VulkanCore::Context& context,
                    std::function<void(int, int)> textureReadyCallback);
  ~AsyncDataUploader();
  struct TextureLoadTask {
    VulkanCore::Texture* texture;
    void* data;
    int index;
    int modelIndex;
  };

  struct TextureMipGenTask {
    VulkanCore::Texture* texture;
    VkSemaphore graphicsSemaphore;
    int index;
  };

  void startProcessing();

  void queueTextureUploadTasks(const TextureLoadTask& textureLoadTask);

 private:
  VulkanCore::Context& context_;
  VulkanCore::CommandQueueManager transferCommandQueueMgr_;
  VulkanCore::CommandQueueManager graphicsCommandQueueMgr_;

  SharedQueue<TextureLoadTask> textureLoadTasks_;
  SharedQueue<TextureMipGenTask> textureMipGenerationTasks_;

  std::function<void(int, int)> textureReadyCallback_;
  std::thread textureGPUDataUploadThread_;
  std::thread textureMipGenThread_;
  bool closeThreads_ = false;
  std::vector<VkSemaphore> semaphores_;
};
}  // namespace EngineCore
