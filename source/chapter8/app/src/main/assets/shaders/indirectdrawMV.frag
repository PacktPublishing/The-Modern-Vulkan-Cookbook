#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_multiview : require
#include "Common.glsl"

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in flat uint inflatMeshId;
layout(location = 2) in flat int inflatMaterialId;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform constants {
  float xl;
  float yl;
  float xr;
  float yr;
}
PushConstants;

void main() {
  int basecolorIndex = -1;
  uint basecolorSamplerIndex = 0;

  if (inflatMaterialId != -1) {
    MaterialData mat =
        materialDataAlias[MATERIAL_DATA_INDEX].materials[inflatMaterialId];
    basecolorIndex = mat.basecolorIndex;
    basecolorSamplerIndex = mat.basecolorSamplerIndex;
  }

  vec4 preliminaryColorOutput;
  if (basecolorIndex != -1) {
    preliminaryColorOutput =
        texture(sampler2D(BindlessImage2D[basecolorIndex],
                          BindlessSampler[basecolorSamplerIndex]),
                inTexCoord);
  } else {
    preliminaryColorOutput = vec4(0.5, 0.5, 0.5, 1.0);
  }

  const float eyeGazeCursorRadius = 10;  // in pixels
  vec2 eyeGazePos = vec2(PushConstants.xl, PushConstants.yl);
  if (gl_ViewIndex == 1) {
    eyeGazePos = vec2(PushConstants.xr, PushConstants.yr);
  }
  vec2 fragmentCoords = vec2(gl_FragCoord.xy);
  float distance = length(eyeGazePos - fragmentCoords);
  if (distance < eyeGazeCursorRadius) {
    outColor = mix(vec4(1, 1, 1, 1), preliminaryColorOutput, .5);
    outColor.a = 1;
  } else {
    outColor = preliminaryColorOutput;
  }
}
