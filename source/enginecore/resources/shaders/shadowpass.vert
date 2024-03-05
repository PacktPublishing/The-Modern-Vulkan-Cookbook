#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_debug_printf : enable

#extension GL_GOOGLE_include_directive : require
#include "CommonStructs.glsl"
#include "IndirectCommon.glsl"

void main() {
  Vertex vertex = vertexAlias[VERTEX_INDEX].vertices[gl_VertexIndex];
  vec3 position = vec3(vertex.posX, vertex.posY, vertex.posZ);
  gl_Position = MVP.projection * MVP.view * MVP.model * vec4(position, 1.0);
}
