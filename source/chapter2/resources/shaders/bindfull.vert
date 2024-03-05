#version 460
#extension GL_EXT_buffer_reference2 : require

layout(set = 0, binding = 0) uniform Transforms {
  mat4 model;
  mat4 view;
  mat4 projection;
  mat4 prevView;
  mat4 jitterMat;
}
MVP;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec2 inTexCoord2;
layout(location = 5) in int inMaterial;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outWorldNormal;
layout(location = 2) out vec4 outTangent;
void main() {
  gl_Position = MVP.projection * MVP.view * MVP.model * vec4(inPosition, 1.0);
  outTexCoord = inTexCoord;
  outWorldNormal = inNormal;
  outTangent = inTangent;
}
