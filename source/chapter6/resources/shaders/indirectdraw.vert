#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_debug_printf : enable

#extension GL_GOOGLE_include_directive : require
#include "CommonStructs.glsl"
#include "IndirectCommon.glsl"

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out flat uint outflatMeshId;
layout(location = 2) out flat int outflatMaterialId;
layout(location = 3) out vec3 outNormal;
layout(location = 4) out vec3 outPos;

void main() {
  uint index =
      indexAlias[INDICIES_INDEX].indices[gl_BaseVertex + gl_VertexIndex];

  Vertex vertex = vertexAlias[VERTEX_INDEX].vertices[gl_VertexIndex];

  vec3 position = vec3(vertex.posX, vertex.posY, vertex.posZ);
  outNormal = vec3(vertex.normalX, vertex.normalY, vertex.normalZ);
  vec2 uv = vec2(vertex.uvX, vertex.uvY);
  gl_Position = MVP.projection * MVP.view * MVP.model * vec4(position, 1.0);
  outPos = gl_Position.xyz;
  outTexCoord = uv;
  outflatMeshId =
      indirectDrawAlias[INDIRECT_DRAW_INDEX].meshDraws[gl_DrawID].meshId;
  outflatMaterialId = vertex.material;
}
