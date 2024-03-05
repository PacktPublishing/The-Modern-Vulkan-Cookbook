#ifndef BRDF_COMMON_GLSL
#define BRDF_COMMON_GLSL

// Calculates Fresnel-Schlick approximation.
// Determines how much light is reflected vs refracted based on the view angle.
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// GGX/Trowbridge-Reitz
// Determines the distribution of microfacets on the surface.
// It models how rough or smooth a surface appears.
float distributionGGX(vec3 N, vec3 H, float roughness) {
  float a = roughness * roughness;
  float a2 = a * a;
  float NdotH = max(dot(N, H), 0.0);
  float NdotH2 = NdotH * NdotH;

  float nom = a2;
  float denom = (NdotH2 * (a2 - 1.0) + 1.0);
  denom = 3.14159265359 * denom * denom;

  return nom / denom;
}

// Calculates geometric attenuation.
// It represents the probability that light isn't blocked by microfacets.
float geometrySchlickGGX(float NdotV, float roughness) {
  float r = (roughness + 1.0) * 0.5;
  float r2 = r * r;

  float nom = NdotV;
  float denom = NdotV * (1.0 - r2) + r2;

  return nom / denom;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
  float NdotV = max(dot(N, V), 0.0);
  float NdotL = max(dot(N, L), 0.0);
  float ggx2 = geometrySchlickGGX(NdotV, roughness);
  float ggx1 = geometrySchlickGGX(NdotL, roughness);

  return ggx1 * ggx2;
}

#endif
