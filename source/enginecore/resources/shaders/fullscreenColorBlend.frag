#version 460

layout(set = 0, binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
  vec4 srcColor = texture(texSampler, fragTexCoord);

  outColor = inColor * srcColor.a + srcColor;
}
