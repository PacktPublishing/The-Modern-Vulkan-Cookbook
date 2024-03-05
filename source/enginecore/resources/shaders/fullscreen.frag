#version 460
layout(set = 0, binding = 0) uniform sampler2D texSampler;
layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

const float nearDistance = 10.0f;
const float farDistance = 4000.0f;

float calculateLinearDepth(float depth) {
  return (2.0 * nearDistance) /
         (farDistance + nearDistance - depth * (farDistance - nearDistance));
}

struct FullScreenPushConst {
  vec4 showAsDepth;
};

// Push constants.
layout(push_constant) uniform constants {
  FullScreenPushConst pushConstData;
};

void main() {
  outColor = texture(texSampler, fragTexCoord);
  if (pushConstData.showAsDepth.x != 0) {
    outColor = vec4(vec3(calculateLinearDepth(outColor.r)), 1.0);
  }
}
