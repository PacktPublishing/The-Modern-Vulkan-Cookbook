#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_debug_printf : enable
#extension GL_GOOGLE_include_directive : require
#include "Common.glsl"

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out flat uint outflatMeshId;
layout(location = 2) out flat int outflatMaterialId;

void main() {
  uint index =
      indexAlias[INDICIES_INDEX].indices[gl_BaseVertex + gl_VertexIndex];

  Vertex vertex = vertexAlias[VERTEX_INDEX].vertices[gl_VertexIndex];

  vec3 position = vec3(vertex.posX, vertex.posY, vertex.posZ);
  vec3 normal = vec3(vertex.normalX, vertex.normalY, vertex.normalZ);
  vec2 uv = vec2(vertex.uvX, vertex.uvY);
  gl_Position = MVP.projection * MVP.view * MVP.model * vec4(position, 1.0);
  outTexCoord = uv;
  outflatMeshId =
      indirectDrawAlias[INDIRECT_DRAW_INDEX].meshDraws[gl_DrawID].meshId;
  outflatMaterialId = vertex.material;
}
