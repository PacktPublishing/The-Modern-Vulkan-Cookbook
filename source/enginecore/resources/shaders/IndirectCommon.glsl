
#ifndef SHADER_INDIRECT_COMMON_GLSL
#define SHADER_INDIRECT_COMMON_GLSL

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
