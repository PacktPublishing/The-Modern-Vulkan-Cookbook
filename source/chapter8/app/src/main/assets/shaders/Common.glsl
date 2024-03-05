
#ifndef SHADER_INDIRECT_COMMON_GLSL
#define SHADER_INDIRECT_COMMON_GLSL

#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
#extension GL_EXT_shader_16bit_storage : require

// we could use vec3 etc but that can cause alignment issue, so we prefer float
struct Vertex {
  float16_t posX;
  float16_t posY;
  float16_t posZ;
  float16_t normalX;
  float16_t normalY;
  float16_t normalZ;
  float16_t tangentX;
  float16_t tangentY;
  float16_t tangentZ;
  float16_t tangentW;
  float16_t uvX;
  float16_t uvY;
  float16_t uvX2;
  float16_t uvY2;
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

layout(set = 0, binding = 0) uniform Transforms {
  mat4 mvp_left;
  mat4 mvp_right;
}
MVP;

layout(set = 1, binding = 0) uniform texture2D BindlessImage2D[];
layout(set = 1, binding = 1) uniform sampler BindlessSampler[];

layout(set = 2, binding = 0) readonly buffer VertexBuffer {
  Vertex vertices[];
}
vertexAlias[4];

layout(set = 2, binding = 0) readonly buffer IndexBuffer {
  uint indices[];
}
indexAlias[4];

layout(set = 2, binding = 0) readonly buffer IndirectDrawDataAndMeshDataBuffer {
  IndirectDrawDataAndMeshData meshDraws[];
}
indirectDrawAlias[4];

layout(set = 2, binding = 0) readonly buffer MaterialBufferForAllMesh {
  MaterialData materials[];
}
materialDataAlias[4];

const int VERTEX_INDEX = 0;
const int INDICIES_INDEX = 1;
const int INDIRECT_DRAW_INDEX = 2;
const int MATERIAL_DATA_INDEX = 3;

#endif
