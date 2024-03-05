
#ifndef SHADER_INDIRECT_COMMON_GLSL
#define SHADER_INDIRECT_COMMON_GLSL

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

layout(set = 0, binding = 0) uniform Transforms {
  mat4 model;
  mat4 view;
  mat4 projection;
  mat4 prevView;
  mat4 jitterMat;
}
MVP;

layout(set = 1, binding = 0) uniform texture2D BindlessImage2D[];
layout(set = 2, binding = 0) uniform sampler BindlessSampler[];

layout(set = 3, binding = 0) readonly buffer VertexBuffer {
  Vertex vertices[];
}
vertexAlias[4];

layout(set = 3, binding = 0) readonly buffer IndexBuffer {
  uint indices[];
}
indexAlias[4];

layout(set = 3, binding = 0) readonly buffer IndirectDrawDataAndMeshDataBuffer {
  IndirectDrawDataAndMeshData meshDraws[];
}
indirectDrawAlias[4];

layout(set = 3, binding = 0) readonly buffer MaterialBufferForAllMesh {
  MaterialData materials[];
}
materialDataAlias[4];

const int VERTEX_INDEX = 0;
const int INDICIES_INDEX = 1;
const int INDIRECT_DRAW_INDEX = 2;
const int MATERIAL_DATA_INDEX = 3;

#endif
