#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require
#include "CommonStructs.glsl"
#include "IndirectCommon.glsl"

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in flat uint inflatMeshId;
layout(location = 2) in flat int inflatMaterialId;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec4 inTangent;
layout(location = 5) in vec3 inModelSpacePos;
layout(location = 6) in vec4 inClipSpacePos;
layout(location = 7) in vec4 inPrevClipSpacePos;

// base color & specular store as rgba8, rest as rgba16s
layout(location = 0) out vec4 outgBufferBaseColor;
layout(location = 1) out vec4 outgBufferWorldNormal;
layout(location = 2) out vec4 outgBufferEmissive;
layout(location = 3) out vec4 outgBufferSpecular;
layout(location = 4) out vec4 outgBufferPosition;
// rg32s
layout(location = 5) out vec2 outgBufferVelocity;

void main() {
  int basecolorIndex = -1;
  int normalIndex = -1;
  int metallicRoughnessIndex = -1;
  int emissiveIndex = -1;

  uint samplerIndex = 0;

  float metallicFactor = 1.0;
  float roughnessFactor = 1.0;

  if (inflatMaterialId != -1) {
    MaterialData mat =
        materialDataAlias[MATERIAL_DATA_INDEX].materials[inflatMaterialId];
    basecolorIndex = mat.basecolorIndex;
    normalIndex = mat.normalIndex;
    metallicRoughnessIndex = mat.metallicRoughnessIndex;
    emissiveIndex = mat.emissiveIndex;
    metallicFactor = mat.metallicFactor;
    roughnessFactor = mat.roughnessFactor;
  }

  if (basecolorIndex != -1) {
    outgBufferBaseColor = texture(sampler2D(BindlessImage2D[basecolorIndex],
                                            BindlessSampler[samplerIndex]),
                                  inTexCoord);
  } else {
    outgBufferBaseColor = vec4(0.5, .5, 0.5, 1.0);
  }

  const vec3 n = normalize(inNormal);
  const vec3 t = normalize(inTangent.xyz);
  const vec3 b = normalize(cross(n, t) * inTangent.w);
  const mat3 tbn = mat3(t, b, n);

  if (normalIndex != -1) {
    vec4 normalTexSampled = texture(
        sampler2D(BindlessImage2D[normalIndex], BindlessSampler[samplerIndex]),
        inTexCoord);

    vec3 normalTan = normalTexSampled.xyz;
    normalTan.y = 1.0f - normalTan.y;
    normalTan.xy = normalTan.xy * 2.0f - vec2(1.0f);

    normalTan.z = sqrt(max(0.0f, 1.0f - dot(normalTan.xy, normalTan.xy)));

    outgBufferWorldNormal.rgb = normalize(tbn * normalize(normalTan));
  } else {
    outgBufferWorldNormal.rgb = n;
  }

  if (metallicRoughnessIndex != -1) {
    vec4 metallicRoughnessTexSampled =
        texture(sampler2D(BindlessImage2D[metallicRoughnessIndex],
                          BindlessSampler[samplerIndex]),
                inTexCoord);

    float specular =
        metallicRoughnessTexSampled
            .b;  // gltf  b stores metalic, while g is roughness, ra isn't used.
    float roughness = metallicRoughnessTexSampled.g;

    outgBufferSpecular.r = specular * metallicFactor;
    outgBufferSpecular.g = roughness * roughnessFactor;
  } else {
    outgBufferSpecular.r = metallicFactor;
    outgBufferSpecular.g = roughnessFactor;
  }

  outgBufferSpecular.b = 0.0;
  outgBufferSpecular.a = 1.0;

  if (emissiveIndex != -1) {
    vec4 emissiveTexSampled = texture(sampler2D(BindlessImage2D[emissiveIndex],
                                                BindlessSampler[samplerIndex]),
                                      inTexCoord);
    outgBufferEmissive.rgb = emissiveTexSampled.rgb;
  } else {
    outgBufferEmissive.rgb = vec3(0.0);
  }

  outgBufferPosition = vec4(inModelSpacePos.xyz, 1.0);

  {
    vec2 a = (inClipSpacePos.xy / inClipSpacePos.w);
    a = (a + 1.0f) / 2.0f;
    a.y = 1.0 - a.y;
    vec2 b = (inPrevClipSpacePos.xy / inPrevClipSpacePos.w);
    b = (b + 1.0f) / 2.0f;
    b.y = 1.0 - b.y;
    outgBufferVelocity = (a - b);
  }
}
