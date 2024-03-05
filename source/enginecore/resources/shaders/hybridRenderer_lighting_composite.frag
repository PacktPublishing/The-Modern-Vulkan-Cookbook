#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

const float nearDistance = 10.0f;
const float farDistance = 4000.0f;

layout(set = 0, binding = 0) uniform sampler2D gBufferWorldNormal;
layout(set = 0, binding = 1) uniform sampler2D
    gBufferSpecular;  // .r is metallic, .g is roughness
layout(set = 0, binding = 2) uniform sampler2D gBufferBaseColor;
layout(set = 0, binding = 3) uniform sampler2D gBufferPosition;
layout(set = 0, binding = 4) uniform sampler2D rayTracedShadowAndAOTexture;
// layout(set = 0, binding = 6)uniform sampler2D rayTracedReflectionTexture;

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

float calculateLinearDepth(float depth) {
  return (2.0 * nearDistance) /
         (farDistance + nearDistance - depth * (farDistance - nearDistance));
}

#include "brdf.glsl"

void main() {
  vec4 worldPos = texture(gBufferPosition, fragTexCoord);

  vec3 basecolor = texture(gBufferBaseColor, fragTexCoord).rgb;

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
  vec3 lightToFragmentDir = normalize(lightToFragment);
  float cosTheta = dot(-lightToFragmentDir, lightDirection);
  float spotAttenuation =
      smoothstep(lightData.outerConeAngle, lightData.innerConeAngle, cosTheta);
  vec3 lightIntensity =
      spotAttenuation * attenuation * lightData.lightColor.rgb;

  // Final light contribution
  vec3 finalColor = (NdotL * (lightIntensity) * (diffuse + specular)) + ambient;

  // apply shadow & AO texture
  float inShadow = texture(rayTracedShadowAndAOTexture, fragTexCoord).r;
  float ao = texture(rayTracedShadowAndAOTexture, fragTexCoord).g;

  // finalColor *= ao;
  finalColor *= inShadow;

  outColor = vec4(finalColor, 1.0);
}
