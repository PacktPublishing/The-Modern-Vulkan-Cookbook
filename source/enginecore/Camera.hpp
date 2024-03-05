#pragma once

#include <array>
#include <glm/glm.hpp>
#include <glm/gtx/type_aligned.hpp>

struct UniformTransforms {
  glm::aligned_mat4 model;
  glm::aligned_mat4 view;
  glm::aligned_mat4 projection;
  glm::aligned_mat4 prevViewMat = glm::mat4(1.0);
  glm::aligned_mat4 jitter = glm::mat4(1.0);
};

namespace EngineCore {

class Camera final {
 public:
  explicit Camera(const glm::vec3& position = glm::vec3(-9.f, 2.f, 2.f),
                  const glm::vec3& target = glm::vec3(0.0f, 0.0f, 0.0f),
                  const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f), float near = .1f,
                  float far = 4000.f, float aspect = 800.f / 600.f);

  std::array<glm::vec4, 6> calculateFrustumPlanes();

  void move(const glm::vec3& direction, float increment);

  void setPos(const glm::vec3& position);

  void setUpVector(const glm::vec3& up);

  void rotate(const glm::vec2& delta, double deltaT = kSpeed);

  glm::mat4 getProjectMatrix() const;

  glm::mat4 viewMatrix() const;

  glm::vec3 direction() const;

  glm::vec3 eulerAngles() const;

  void setEulerAngles(const glm::vec3& dir);

  glm::vec3 right() const;

  glm::vec3 up() const;

  glm::vec3 position() const;

  bool isDirty() const;

  void setNotDirty();

  void updateJitterMat(uint32_t frameIndex, int numSamples, int width, int height);

  glm::mat4 jitterMat() const { return jitterMat_; }

  glm::vec2 jitterInPixelSpace() const { return jitterVal_; }

  glm::vec2 jitterInNDCSpace() const {
    return glm::vec2(jitterMat_[2][0], jitterMat_[2][1]);
  }

 private:
  glm::vec3 position_;
  glm::vec3 up_;
  glm::vec3 target_;
  glm::quat orientation_;
  glm::mat4 projectMatrix_{1.0f};
  glm::mat4 jitterMat_{1.0f};
  glm::vec2 jitterVal_;
  float nearP_ = 10.f;
  float farP_ = 4000.0f;
  float fov_ = 60.0f;
  float aspect_ = 800.0 / 600.0;

  bool isDirty_ = true;
  static constexpr float kSpeed = 4.0f;
  static constexpr float kSpeedKey = 0.3f;
};

}  // namespace EngineCore
