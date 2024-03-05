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
layout(location = 4) out vec4 outTangent;
layout(location = 5) out vec3 outModelSpacePos;
layout(location = 6) out vec4 outClipSpacePos;
layout(location = 7) out vec4 outPrevClipSpacePos;

struct GBufferPushConstants {
  uint applyJitter;
};

layout(push_constant) uniform constants {
  GBufferPushConstants gbufferConstData;
};

void main() {
  uint index =
      indexAlias[INDICIES_INDEX].indices[gl_BaseVertex + gl_VertexIndex];
  debugPrintfEXT(
      "gl_BaseVertex = %d gl_VertexIndex = %d, gl_BaseInstance = %d, index = "
      "%d, gl_InstanceIndex = %d",
      gl_BaseVertex, gl_VertexIndex, gl_BaseInstance, index, gl_InstanceIndex);

  // uint index = indexAlias[INDICIES_INDEX].indices[gl_BaseVertex +
  // gl_VertexIndex];
  Vertex vertex = vertexAlias[VERTEX_INDEX]
                      .vertices[/*gl_InstanceIndex + */ gl_VertexIndex];
  // debugPrintfEXT("gl_VertexIndex = %d", gl_VertexIndex);

  vec3 position = vec3(vertex.posX, vertex.posY, vertex.posZ);
  vec3 normal = vec3(vertex.normalX, vertex.normalY, vertex.normalZ);
  vec2 uv = vec2(vertex.uvX, vertex.uvY);
  if (gbufferConstData.applyJitter == 0) {
    gl_Position = MVP.projection * MVP.view * MVP.model * vec4(position, 1.0);
  } else {
    gl_Position = MVP.projection * MVP.view * MVP.model * MVP.jitterMat *
                  vec4(position, 1.0);
  }
  outTexCoord = uv;
  outflatMeshId =
      indirectDrawAlias[INDIRECT_DRAW_INDEX].meshDraws[gl_DrawID].meshId;
  outflatMaterialId = vertex.material;
  outNormal = normal;
  outTangent =
      vec4(vertex.tangentX, vertex.tangentY, vertex.tangentZ, vertex.tangentW);
  outModelSpacePos = position;
  outClipSpacePos = MVP.projection * MVP.view * MVP.model * vec4(position, 1.0);
  outPrevClipSpacePos =
      MVP.projection * MVP.prevView * MVP.model * vec4(position, 1.0);
}
