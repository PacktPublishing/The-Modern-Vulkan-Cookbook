#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtx/type_aligned.hpp>
#include <memory>
#include <string>
#include <vector>

#include "glm/gtx/euler_angles.hpp"
#include "vulkancore/Common.hpp"

namespace EngineCore {
struct Vertex {
  glm::vec3 pos;
  glm::vec3 normal = glm::vec3();
  glm::vec4 tangent = glm::vec4();
  glm::vec2 texCoord = glm::vec2();
  glm::vec2 texCoord1 = glm::vec2();
  uint32_t material;

  void applyTransform(const glm::mat4& m) {
    auto newp = m * glm::vec4(pos, 1.0);
    pos = glm::vec3(newp.x, newp.y, newp.z);
    glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(m));
    normal = normalMatrix * normal;
    tangent = glm::inverseTranspose(m) * tangent;
  }
};

struct Vertex16Bit {
  glm::uint16_t posx;
  glm::uint16_t posy;
  glm::uint16_t posz;
  glm::uint16_t normalx;
  glm::uint16_t normaly;
  glm::uint16_t normalz;
  glm::uint16_t tangentx;
  glm::uint16_t tangenty;
  glm::uint16_t tangentz;
  glm::uint16_t tangentw;
  glm::uint16_t texcoordu;
  glm::uint16_t texcoordv;
  glm::uint16_t texcoord1u;
  glm::uint16_t texcoord1v;
  uint32_t material;
};

Vertex16Bit to16bitVertex(const Vertex& v);

struct Mesh {
  using Indices = uint32_t;
  std::vector<Vertex> vertices = {};
  std::vector<Vertex16Bit> vertices16bit = {};
  std::vector<Indices> indices = {};
  glm::vec3 minAABB = glm::vec3(999999, 999999, 999999);
  glm::vec3 maxAABB = glm::vec3(-999999, -999999, -999999);
  glm::vec3 extents;
  glm::vec3 center;
  int32_t material = -1;
};

struct Material {
  int basecolorTextureId = -1;
  int basecolorSamplerId = -1;
  int metallicRoughnessTextureId = -1;
  int metallicRoughnessSamplerId = -1;
  int normalTextureTextureId = -1;
  int normalTextureSamplerId = -1;
  int emissiveTextureId = -1;
  int emissiveSamplerId = -1;
  float metallicFactor = 1.0;
  float roughnessFactor = 1.0;
  glm::vec2 padding;  // needed to make sure its aligned since materials will be passed as
                      // array of struct
  glm::vec4 basecolor;
  // Todo add more properties such as occlusion texture, occlusion strength,
  // metallicFactor, roughnessFactor etc based upon what shader can consume
};

struct stbImageData {
  stbImageData(const std::vector<uint8_t>& imageData, bool useFloat = false);
  stbImageData(const std::vector<char>& imageData, bool useFloat = false);
  ~stbImageData();
  void* data = nullptr;
  int width = 0;
  int height = 0;
  int channels = 0;
};

struct IndirectDrawDataAndMeshData {
  uint32_t indexCount;
  uint32_t instanceCount;
  uint32_t firstIndex;
  uint32_t vertexOffset;
  uint32_t firstInstance;

  uint32_t meshId;
  int materialIndex;
  // in future we can model mat etc, things specific to Mesh
};

struct Model {
  std::vector<Mesh> meshes;
  std::vector<Material> materials;
  std::vector<std::unique_ptr<stbImageData>> textures;

  std::vector<IndirectDrawDataAndMeshData> indirectDrawDataSet;

  uint32_t totalVertexSize = 0;
  uint32_t totalIndexSize = 0;
  uint32_t indexCount = 0;
};
}  // namespace EngineCore
