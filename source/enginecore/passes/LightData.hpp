#pragma once
#include <gli/gli.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/type_aligned.hpp>

#include "enginecore/Camera.hpp"

struct LightData {
  void initCam() {
    lightCam = EngineCore::Camera(glm::vec3(lightPos.x, lightPos.y, lightPos.z));
    lightVP = lightCam.getProjectMatrix() * lightCam.viewMatrix();
    lightDir = glm::vec4(lightCam.direction(), 1.0);
  }
  LightData() {}
  void recalculateLightVP() {
    lightVP = lightCam.getProjectMatrix() * lightCam.viewMatrix();
  }
  void setLightPos(glm::vec3 pos) {
    // if (lightPos.x != pos.x || lightPos.y != pos.y || lightPos.z != pos.z) {
    lightPos.x = pos.x;
    lightPos.y = pos.y;
    lightPos.z = pos.z;
    lightCam.setPos(pos);
    lightVP = lightCam.getProjectMatrix() * lightCam.viewMatrix();
    // }
  }
  void setLightDir(glm::vec3 dir) {
    // if (lightDir.x != dir.x || lightDir.y != dir.y || lightDir.z != dir.z) {
    
    lightCam.setEulerAngles(dir);
    lightDir = glm::vec4(glm::normalize(lightCam.direction()), 1.0);
    lightVP = lightCam.getProjectMatrix() * lightCam.viewMatrix();
    // }
  }

  void setLightColor(glm::vec3 color) {
    lightColor.x = color.x;
    lightColor.y = color.y;
    lightColor.z = color.z;
  }
  void setAmbientColor(glm::vec3 color) {
    ambientColor.x = color.x;
    ambientColor.y = color.y;
    ambientColor.z = color.z;
  }

  glm::aligned_vec4 lightPos = glm::vec4(-9.0, 2.0, 2.0, 1.0);
  glm::aligned_vec4 lightDir = glm::vec4(0.0, 1.0, 0.0, 1.0);
  glm::aligned_vec4 lightColor = {};
  glm::aligned_vec4 ambientColor = {};
  glm::aligned_mat4 lightVP;
  float innerAngle = 0.523599f;  // 30 degree
  float outerAngle = 1.22173f;   // 70 degree
  EngineCore::Camera lightCam;
};
