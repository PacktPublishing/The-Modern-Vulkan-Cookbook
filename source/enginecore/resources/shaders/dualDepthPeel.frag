#version 460

layout(set = 1, binding = 0) uniform ObjectProperties {
  vec4 color;
  mat4 model;
}
objectProperties;

layout(set = 2, binding = 0) uniform sampler2D depth;
layout(set = 2, binding = 1) uniform sampler2D frontColor;

layout(location = 0) out vec2 depthMinMax;
layout(location = 1) out vec4 frontColorOut;
layout(location = 2) out vec4 backColorOut;

const float MAX_DEPTH = 99999.0;

void main() {
  float fragDepth = gl_FragCoord.z;

  vec2 lastDepth = texture(depth, gl_FragCoord.xy / textureSize(depth, 0)).rg;

  depthMinMax.rg = vec2(-MAX_DEPTH);
  frontColorOut = vec4(0.0f);
  backColorOut = vec4(0.0f);

  float nearestDepth = -lastDepth.x;
  float furthestDepth = lastDepth.y;

  if (fragDepth < nearestDepth || fragDepth > furthestDepth) {
    return;
  }

  if (fragDepth > nearestDepth && fragDepth < furthestDepth) {
    depthMinMax = vec2(-fragDepth, fragDepth);
    return;
  }

  vec4 color = objectProperties.color;

  if (fragDepth == nearestDepth) {
    frontColorOut = vec4(color.rgb * color.a, color.a);
  } else {
    backColorOut = color;
  }
}
