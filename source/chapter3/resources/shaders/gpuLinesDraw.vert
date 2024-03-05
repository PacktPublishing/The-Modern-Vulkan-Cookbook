#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_debug_printf : enable

#extension GL_GOOGLE_include_directive : require
#include "CommonStructs.glsl"

layout(set = 0, binding = 0) uniform Transforms {
  mat4 model;
  mat4 view;
  mat4 projection;
  mat4 prevView;
  mat4 jitterMat;
}
MVP;

struct Line {
  vec3 v0;
  vec4 c0;
  vec3 v1;
  vec4 c1;
};

struct VkDrawIndirectCommand {
  uint vertexCount;
  uint instanceCount;
  uint firstVertex;
  uint firstInstance;
};

layout(set = 1, binding = 0) readonly buffer GPULinesBuffer {
  // uint size;
  // uint pad0;
  // uint pad1;
  // uint pad2;
  // VkDrawIndirectCommand cmd;
  Line lines[];
}
lineBuffer;

layout(location = 0) out vec4 outColor;

void main() {
  if (gl_VertexIndex == 0) {
    vec3 vertex = lineBuffer.lines[gl_InstanceIndex].v0;
    gl_Position =
        (MVP.projection * MVP.view * MVP.model * vec4(vertex, 1.0)).xyww;

    outColor = lineBuffer.lines[gl_InstanceIndex].c0;
  } else {
    vec3 vertex = lineBuffer.lines[gl_InstanceIndex].v1;
    gl_Position =
        (MVP.projection * MVP.view * MVP.model * vec4(vertex, 1.0)).xyww;

    outColor = lineBuffer.lines[gl_InstanceIndex].c1;
  }
}
