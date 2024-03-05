#include <iostream>

namespace EngineCore {

class FPSCounter final {
 public:
  FPSCounter(double now, size_t numSamples = 100)
      : previous_{now}, numSamplesStore_{numSamples} {
    samples_.resize(numSamplesStore_, 0.0f);
  }

  void setSilent(bool silent) { silent_ = silent; }

  void update(double now) {
    const auto delta = now - previous_;
    if (delta > 1) {
      const auto fps = static_cast<double>(frame_ - previousFrame_) / delta;
      if (!silent_) {
        std::cerr << "FPS: " << fps << std::endl;
      }
      previousFrame_ = frame_;
      previous_ = now;

      samples_[sample_ % numSamplesStore_] = fps;
      ++sample_;
    }
  }

  std::vector<float> fpsSamples() {
    std::vector<float> retVal(numSamplesStore_, 0.0f);
    const size_t startFrame =
        sample_ % numSamplesStore_ - std::min(sample_, numSamplesStore_);
    for (uint32_t i = 0; i < numSamplesStore_; ++i) {
      retVal[i] = samples_[(startFrame + i) % numSamplesStore_];
    }
    return retVal;
  }

  float current() const { return samples_[sample_ % numSamplesStore_]; }

  float last() const { return samples_[(sample_ - 1) % numSamplesStore_]; }

  void incFrame() { ++frame_; }

 private:
  double previous_ = 0.0;
  size_t frame_ = 0;
  size_t previousFrame_ = 0;
  size_t sample_ = 0;
  bool silent_ = false;
  size_t numSamplesStore_ = 100;
  std::vector<float> samples_;
};

}  // namespace EngineCore
