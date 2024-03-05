#version 460

layout(set = 1, binding = 0) uniform ObjectProperties {
  vec4 color;
  mat4 model;
}
objectProperties;

layout(set = 2, binding = 0) uniform sampler2D depth;
layout(set = 2, binding = 1) uniform sampler2D opaque;
layout(set = 2, binding = 2) uniform sampler2D temporaryColor;

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;

layout(location = 0) out vec4 outColor;

void main() {
  float fragDepth = gl_FragCoord.z;

  float peelDepth = texture(depth, gl_FragCoord.xy / textureSize(depth, 0)).r;
  float opaqueDepth =
      texture(opaque, gl_FragCoord.xy / textureSize(opaque, 0)).r;

  if (fragDepth <= peelDepth) {
    discard;
  }

  vec4 tmpColor =
      texture(temporaryColor, gl_FragCoord.xy / textureSize(temporaryColor, 0));
  vec3 mulTmpColor = tmpColor.xyz * tmpColor.a;
  vec3 mulObjProp = objectProperties.color.xyz * (1.0 - tmpColor.a);

  outColor = vec4(
      tmpColor.a * (objectProperties.color.a * objectProperties.color.rgb) +
          tmpColor.rgb,
      (1 - objectProperties.color.a) * tmpColor.a);
}
