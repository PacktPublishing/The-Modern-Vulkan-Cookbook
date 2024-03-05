#version 460
#extension GL_EXT_buffer_reference2 : require

layout(push_constant) uniform Transforms {
  mat4 model;
}
PushConstants;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 uvCoords;

vec2 positions[3] = vec2[](vec2(0.0, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5));

vec2 texCoords[3] = vec2[](vec2(0.0, 0.0), vec2(0.5, 1.0), vec2(1.0, 0.0));

vec3 colors[3] =
    vec3[](vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0));

vec2 offset[2] = vec2[](vec2(-0.25, 0.0), vec2(0.25, 0.0));

void main() {
  gl_Position =
      PushConstants.model * vec4(positions[gl_VertexIndex], 0.0, 1.0) +
      vec4(offset[gl_InstanceIndex], 0.0, 0.0);
  uvCoords = texCoords[gl_VertexIndex];
  outColor = vec4(colors[gl_VertexIndex], 1.0);
}
