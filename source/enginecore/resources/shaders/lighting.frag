#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

layout(set = 0, binding = 0) uniform sampler2D gBufferWorldNormal;
layout(set = 0, binding = 1) uniform sampler2D gBufferSpecular;
layout(set = 0, binding = 2) uniform sampler2D gBufferBaseColor;
layout(set = 0, binding = 3) uniform sampler2D gBufferDepth;
layout(set = 0, binding = 4) uniform sampler2D gBufferPosition;
layout(set = 0, binding = 5) uniform sampler2D ambientOcclusion;

// #define USESAMPLERFORSHADOW // To Enable it, enable also in LightingPass.cpp

#if defined(USESAMPLERFORSHADOW)
layout(set = 0, binding = 6) uniform sampler2D shadowMap;
#else
layout(set = 0, binding = 6) uniform sampler2DShadow shadowMap;
#endif

layout(set = 1, binding = 0) uniform Transforms {
  mat4 viewProj;
  mat4 viewProjInv;
  mat4 viewInv;
}
cameraData;

layout(set = 1, binding = 1) uniform Lights {
  vec4 lightPos;
  vec4 lightDir;
  vec4 lightColor;
  vec4 ambientColor;  // environment light color
  mat4 lightVP;
  float innerConeAngle;
  float outerConeAngle;
}
lightData;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

#include "brdf.glsl"

vec3 generateWorldPositionFromDepth(vec2 texturePos, float depth) {
  texturePos.y = 1.0f - texturePos.y;
  vec4 ndc = vec4((texturePos * 2.0) - 1.0, depth, 1.0);
  vec4 worldPosition = cameraData.viewProjInv * ndc;
  worldPosition /= worldPosition.w;
  return worldPosition.xyz;
}

const float nearDistance = 10.0f;
const float farDistance = 4000.0f;

float calculateLinearDepth(float depth) {
  return (2.0 * nearDistance) /
         (farDistance + nearDistance - depth * (farDistance - nearDistance));
}

float computeShadow(vec4 clipSpaceCoordWrtLight) {
  vec3 ndcCoordWrtLight = clipSpaceCoordWrtLight.xyz / clipSpaceCoordWrtLight.w;

  vec3 zeroToOneCoordWrtLight = ndcCoordWrtLight;

  // z in vulkan is already in 0 to 1 space
  zeroToOneCoordWrtLight.xy = (zeroToOneCoordWrtLight.xy + 1.0) / 2.0;

  // y needs to be inverted in vulkan
  zeroToOneCoordWrtLight.y = 1.0 - zeroToOneCoordWrtLight.y;

  const float depthBias = 0.00005;
  zeroToOneCoordWrtLight.z = zeroToOneCoordWrtLight.z - depthBias;

#if defined(USESAMPLERFORSHADOW)
  float depthFromShadowMap = texture(shadowMap, zeroToOneCoordWrtLight.xy).x;
  return step(zeroToOneCoordWrtLight.z, depthFromShadowMap);
#else
  return texture(shadowMap,
                 zeroToOneCoordWrtLight);  // if using sampler2DShadow
#endif
}

float PCF(vec4 clipSpaceCoordWrtLight) {
  vec2 texCoord = clipSpaceCoordWrtLight.xy / clipSpaceCoordWrtLight.w;
  texCoord = texCoord * .5 + .5;
  texCoord.y = 1.0 - texCoord.y;
  if (texCoord.x > 1.0 || texCoord.y > 1.0 || texCoord.x < 0.0 ||
      texCoord.y < 0.0) {
    return 1.0;
  }

  vec2 texSize = textureSize(shadowMap, 0);

  float result = 0.0;
  vec2 offset = (1.0 / texSize) * clipSpaceCoordWrtLight.w;

  for (float i = -1.5; i <= 1.5; i += 1.0) {
    for (float j = -1.5; j <= 1.5; j += 1.0) {
      result += computeShadow(clipSpaceCoordWrtLight +
                              vec4(vec2(i, j) * offset, 0.0, 0.0));
    }
  }
  return result / 16.0;
}

void main() {
  float depth = texture(gBufferDepth, fragTexCoord).r;
  vec4 worldPos = texture(gBufferPosition, fragTexCoord);

  vec3 basecolor = texture(gBufferBaseColor, fragTexCoord).rgb;

  if (worldPos.x == 0.0 && worldPos.y == 0.0 && worldPos.z == 0.0 &&
      worldPos.w == 0.0) {
    outColor = vec4(basecolor, 1.0);
    return;
  }

  vec3 camPos = cameraData.viewInv[3].xyz;
  vec3 V = normalize(camPos - worldPos.xyz);

  vec2 gbufferSpecularData = texture(gBufferSpecular, fragTexCoord).rg;
  float metallic = gbufferSpecularData.r;
  float roughness = gbufferSpecularData.g;
  vec4 gbufferNormalData = texture(gBufferWorldNormal, fragTexCoord);
  vec3 N = normalize(gbufferNormalData.xyz);

  vec3 F0 = vec3(0.04);
  F0 = mix(F0, basecolor, metallic);
  vec3 L = normalize(lightData.lightDir.xyz -
                     worldPos.xyz);  // Using spotlight direction
  vec3 H = normalize(V + L);

  vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
  float D = distributionGGX(N, H, roughness);
  float G = geometrySmith(N, V, L, roughness);

  vec3 nominator = D * G * F;
  float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
  vec3 specular = nominator / denominator;

  vec3 kS = F;
  vec3 kD = vec3(1.0) - kS;
  kD *= 1.0 - metallic;

  float NdotL = max(dot(N, L), 0.0);
  vec3 diffuse = kD * basecolor / 3.14159265359;

  vec3 ambient = lightData.ambientColor.rgb * basecolor;

  // Spotlight calculations
  vec3 lightToFragment = lightData.lightPos.xyz - worldPos.xyz;
  vec3 lightDirection = normalize(-lightData.lightDir.xyz);
  float distanceToLight = length(lightToFragment);
  float attenuation = 1.0 / (1.0 + 0.1 * distanceToLight +
                             0.01 * distanceToLight * distanceToLight);
  vec3 lightDir = normalize(lightToFragment);
  float cosTheta = dot(-lightDir, lightDirection);
  float spotAttenuation =
      smoothstep(lightData.outerConeAngle, lightData.innerConeAngle, cosTheta);
  vec3 lightIntensity =
      spotAttenuation * attenuation * lightData.lightColor.rgb;

  // Final light contribution
  vec3 finalColor = (NdotL * (lightIntensity) * (diffuse + specular)) + ambient;

  float ao = texture(ambientOcclusion, fragTexCoord).r;
  finalColor *= ao;

  outColor = vec4(finalColor, 1.0);

  vec4 clipSpaceCoordWrtLight = lightData.lightVP * vec4(worldPos.xyz, 1.0f);
  float vis = PCF(clipSpaceCoordWrtLight);

  if (vis <= .001) {
    vis = .3;
  }

  outColor.xyz *= vis;
}
