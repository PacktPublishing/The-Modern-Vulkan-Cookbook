#ifndef SHADER_COMMON_STRUCTS_GLSL
#define SHADER_COMMON_STRUCTS_GLSL

// we could use vec3 etc but that can cause alignment issue, so we prefer float
struct Vertex {
  float posX;
  float posY;
  float posZ;
  float normalX;
  float normalY;
  float normalZ;
  float tangentX;
  float tangentY;
  float tangentZ;
  float tangentW;
  float uvX;
  float uvY;
  float uvX2;
  float uvY2;
  int material;
};

// basically mesh data & data required by vkCmdDrawIndexedIndirect command
// (first 5 fields are read by the device from a buffer during execution) in
// theory they could be kept seperately as well
struct IndirectDrawDataAndMeshData {
  uint indexCount;
  uint instanceCount;
  uint firstIndex;
  uint vertexOffset;
  uint firstInstance;

  uint meshId;
  int materialIndex;
  // in future we can model mat etc, things specific to Mesh
};

struct MaterialData {
  int basecolorIndex;
  int basecolorSamplerIndex;
  int metallicRoughnessIndex;
  int metallicRoughnessSamplerIndex;
  int normalIndex;
  int normalSamplerIndex;
  int emissiveIndex;
  int emissiveSamplerIndex;
  float metallicFactor;
  float roughnessFactor;
  vec2 padding;
  vec4 basecolor;
};

struct MeshBboxData {
  vec4 centerPos;
  vec4 extents;
};

struct IndirectDrawCount {
  uint count;
};

struct CullingPushConstants {
  uint count;
};

#endif
