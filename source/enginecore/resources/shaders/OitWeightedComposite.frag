#version 460
#extension GL_GOOGLE_include_directive : require

layout(set = 0, binding = 0) uniform sampler2D colorData;
layout(set = 0, binding = 1) uniform sampler2D alphaData;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
  // Get weighted color data
  vec4 accumulateColor = texture(colorData, fragTexCoord);
  // alpha for given fragment
  float alpha = texture(alphaData, fragTexCoord).r;

  outColor = vec4(accumulateColor.rgb / max(accumulateColor.a, .0001), alpha);
}
