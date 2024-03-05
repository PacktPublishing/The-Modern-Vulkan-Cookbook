#version 460
#extension GL_GOOGLE_include_directive : require

layout(set = 1, binding = 0) uniform ObjectProperties {
  vec4 color;
  mat4 model;
}
objectProperties;

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in float inViewSpaceDepth;

layout(location = 0) out vec4 outColor;

void main() {
  outColor = objectProperties.color;
}
