#include "Camera.hpp"

#include "vulkancore/Utility.hpp"

namespace {
static float VanDerCorputGenerator(size_t base, size_t index) {
  float ret = 0.0f;
  float denominator = float(base);
  while (index > 0) {
    size_t multiplier = index % base;
    ret += float(multiplier) / denominator;
    index = index / base;
    denominator *= base;
  }
  return ret;
}

}  // namespace

namespace EngineCore {

Camera::Camera(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up,
               float near, float far, float aspect)
    : position_{position},
      up_{up},
      target_{target},
      orientation_(glm::lookAt(position_, target, up)),
      nearP_{near},
      farP_{far},
      aspect_{aspect} {
  projectMatrix_ = glm::perspective<double>(glm::radians(fov_), aspect_, nearP_, farP_);
  ASSERT(!glm::all(glm::isnan(orientation_)), "Orientation messed up");
}

std::array<glm::vec4, 6> Camera::calculateFrustumPlanes() {
  std::array<glm::vec4, 6> planes;

  const auto forwardVector = glm::normalize(glm::cross(right(), up()));

  const auto tanFovYHalf = glm::tan(glm::radians(fov_) * 0.5);

  const float nearPlaneHalfHeight = nearP_ * tanFovYHalf;
  const float farPlaneHalfHeight = farP_ * tanFovYHalf;

  const glm::vec3 nearPlaneHalfHeightVec = nearPlaneHalfHeight * up();
  const glm::vec3 nearPlaneHalfWidthVec = nearPlaneHalfHeight * aspect_ * right();

  const glm::vec3 farPlaneHalfHeightVec = farPlaneHalfHeight * up();
  const glm::vec3 farPlaneHalfWidthVec = farPlaneHalfHeight * aspect_ * right();

  const glm::vec3 nearCameraPosition = position() + forwardVector * nearP_;
  const glm::vec3 farCameraPosition = position() + forwardVector * farP_;

  const glm::vec3 nearTopRight =
      nearCameraPosition + nearPlaneHalfWidthVec + nearPlaneHalfHeightVec;
  const glm::vec3 nearBottomRight =
      nearCameraPosition + nearPlaneHalfWidthVec - nearPlaneHalfHeightVec;
  const glm::vec3 nearTopLeft =
      nearCameraPosition - nearPlaneHalfWidthVec + nearPlaneHalfHeightVec;
  const glm::vec3 nearBottomLeft =
      nearCameraPosition - nearPlaneHalfWidthVec - nearPlaneHalfHeightVec;

  const glm::vec3 farTopRight =
      farCameraPosition + farPlaneHalfWidthVec + farPlaneHalfHeightVec;
  const glm::vec3 farBottomRight =
      farCameraPosition + farPlaneHalfWidthVec - farPlaneHalfHeightVec;
  const glm::vec3 farTopLeft =
      farCameraPosition - farPlaneHalfWidthVec + farPlaneHalfHeightVec;
  const glm::vec3 farBottomLeft =
      farCameraPosition - farPlaneHalfWidthVec - farPlaneHalfHeightVec;

  auto getNormal = [](const glm::vec3& corner, const glm::vec3& point1,
                      const glm::vec3& point2) {
    const glm::vec3 dir0 = point1 - corner;
    const glm::vec3 dir1 = point2 - corner;
    const glm::vec3 crossDir = glm::cross(dir0, dir1);
    return glm::normalize(crossDir);
  };

  // left
  const glm::vec3 leftNormal = getNormal(farBottomLeft, nearBottomLeft, farTopLeft);
  planes[0] = glm::vec4(leftNormal, -glm::dot(leftNormal, farBottomLeft));

  // down
  const glm::vec3 downNormal = getNormal(farBottomRight, nearBottomRight, farBottomLeft);
  planes[1] = glm::vec4(downNormal, -glm::dot(downNormal, farBottomRight));

  // right
  const glm::vec3 rightNormal = getNormal(farTopRight, nearTopRight, farBottomRight);
  planes[2] = glm::vec4(rightNormal, -glm::dot(rightNormal, farTopRight));

  // top
  const glm::vec3 topNormal = getNormal(farTopLeft, nearTopLeft, farTopRight);
  planes[3] = glm::vec4(topNormal, -glm::dot(topNormal, farTopLeft));

  // front
  const glm::vec3 frontNormal = getNormal(nearTopRight, nearTopLeft, nearBottomRight);
  planes[4] = glm::vec4(frontNormal, -glm::dot(frontNormal, nearTopRight));

  // back
  const glm::vec3 backNormal = getNormal(farTopRight, farBottomRight, farTopLeft);
  planes[5] = glm::vec4(backNormal, -glm::dot(backNormal, farTopRight));

  return planes;
}

void Camera::move(const glm::vec3& direction, float increment) {
  isDirty_ = true;
  position_ = position_ + (direction * increment * kSpeedKey);
}

void Camera::setPos(const glm::vec3& position) {
  isDirty_ = true;
  position_ = position;
}

void Camera::setUpVector(const glm::vec3& up) {
  isDirty_ = true;
  up_ = normalize(up);
}

void Camera::rotate(const glm::vec2& delta, double deltaT) {
  const glm::quat deltaQuat =
      glm::quat(glm::vec3(-deltaT * delta.y, deltaT * delta.x, 0.0f));
  orientation_ = deltaQuat * orientation_;
  orientation_ = normalize(orientation_);

  {
    isDirty_ = true;
    const glm::mat4 view = glm::mat4_cast(orientation_);
    const glm::vec3 dir = -glm::vec3(view[0][2], view[1][2], view[2][2]);
    orientation_ = glm::quat(glm::lookAt(position_, position_ + dir, up_));
  }
  ASSERT(!glm::all(glm::isnan(orientation_)), "Orientation messed up");
}

glm::mat4 Camera::getProjectMatrix() const { return projectMatrix_; }

glm::mat4 Camera::viewMatrix() const {
  const glm::mat4 t = glm::translate(glm::mat4(1.0f), -position_);
  const glm::mat4 r = glm::mat4_cast(orientation_);
  return r * t;
}

glm::vec3 Camera::direction() const {
  const auto view = glm::mat4_cast(orientation_);  // viewMatrix();
  return glm::vec3(view[0][2], view[1][2], view[2][2]);
}

glm::vec3 Camera::eulerAngles() const {
  glm::vec3 eulerAngles = glm::eulerAngles(orientation_);
  eulerAngles = glm::degrees(eulerAngles);
  return eulerAngles;
}

void Camera::setEulerAngles(const glm::vec3& dir) {
  glm::vec3 eulerAngles = glm::radians(dir);
  orientation_ = glm::quat(eulerAngles);
  isDirty_ = true;
}

glm::vec3 Camera::right() const {
  const auto view = glm::mat4_cast(orientation_);  // viewMatrix();
  return glm::vec3(view[0][0], view[1][0], view[2][0]);
}

glm::vec3 Camera::up() const { return glm::normalize(glm::cross(right(), direction())); }

glm::vec3 Camera::position() const { return position_; }

bool Camera::isDirty() const { return isDirty_; }

void Camera::setNotDirty() { isDirty_ = false; }

void Camera::updateJitterMat(uint32_t frameIndex, int numSamples, int width, int height) {
  uint32_t index = (frameIndex % numSamples) + 1;
  auto x = VanDerCorputGenerator(2, index) - 0.5f;
  auto y = VanDerCorputGenerator(3, index) - 0.5f;

  float uvOffsetX = float(x) / width;
  float uvOffsetY = float(y) / height;
  float ndcOffsetX = uvOffsetX * 2.0f;
  float ndcOffsetY = uvOffsetY * 2.0f;

  jitterMat_[2][0] = ndcOffsetX;
  jitterMat_[2][1] = ndcOffsetY;
  jitterVal_ = glm::vec2(x, y);
}

}  // namespace EngineCore
