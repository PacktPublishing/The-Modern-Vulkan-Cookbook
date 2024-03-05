#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require
#include "Common.glsl"

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in flat uint inflatMeshId;
layout(location = 2) in flat int inflatMaterialId;

layout(location = 0) out vec4 outColor;

void main() {
  int basecolorIndex = -1;
  uint basecolorSamplerIndex = 0;

  if (inflatMaterialId != -1) {
    MaterialData mat =
        materialDataAlias[MATERIAL_DATA_INDEX].materials[inflatMaterialId];
    basecolorIndex = mat.basecolorIndex;
    basecolorSamplerIndex = mat.basecolorSamplerIndex;
  }

  if (basecolorIndex != -1) {
    outColor = texture(sampler2D(BindlessImage2D[basecolorIndex],
                                 BindlessSampler[basecolorSamplerIndex]),
                       inTexCoord);
  } else {
    outColor = vec4(0.5, .5, 0.5, 1.0);
  }
}
