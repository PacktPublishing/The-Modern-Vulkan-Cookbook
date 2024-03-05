#version 460
#extension GL_GOOGLE_include_directive : require
#include "Common.glsl"

layout(constant_id = 0) const uint texturesNotPresent = 0;

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform texture2D baseColorMap;
layout(set = 1, binding = 1) uniform sampler baseColorSampler;

void main() {
  if (texturesNotPresent == 1) {
    outColor = vec4(0.5, .5, 0.5,
                    1.0);  // just show grey color if texture isn't available
  } else {
    outColor = texture(sampler2D(baseColorMap, baseColorSampler), inTexCoord);
  }
}