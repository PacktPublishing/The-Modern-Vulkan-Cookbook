/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */

//-------------------------------------------------------------------------------------------------
// This fils has all the Gltf sampling and evaluation methods

/*
----------------------------------------------

GLTF PBR functions

Modified version from
https://github.com/nvpro-samples/vk_raytrace/blob/18a26b733ceba7b37eff1848881c55cb23a662ff/shaders/pbr_gltf.glsl

*/

#ifndef PBR_GLTF_GLSL
#define PBR_GLTF_GLSL 1

const float M_PI = 3.14159265358979323846;          // pi
const float M_TWO_PI = 6.28318530717958648;         // 2*pi
const float M_PI_2 = 1.57079632679489661923;        // pi/2
const float M_PI_4 = 0.785398163397448309616;       // pi/4
const float M_1_OVER_PI = 0.318309886183790671538;  // 1/pi
const float M_2_OVER_PI = 0.636619772367581343076;  // 2/pi

// https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments
// AppendixB
vec3 BRDF_lambertian(vec3 f0,
                     vec3 f90,
                     vec3 diffuseColor,
                     float VdotH,
                     float metallic) {
  // see
  // https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
  // return (1.0 - F_Schlick(f0, f90, VdotH)) * (diffuseColor / M_PI);

  return (1.0 - metallic) * (diffuseColor / M_PI);
}

vec3 EvalDiffuseGltf(vec3 basecolor,
                     float metallic,
                     vec3 f0,
                     vec3 f90,
                     vec3 V,
                     vec3 N,
                     vec3 L,
                     vec3 H,
                     out float pdf) {
  pdf = 0;
  float NdotV = dot(N, V);
  float NdotL = dot(N, L);

  if (NdotL < 0.0 || NdotV < 0.0)
    return vec3(0.0);

  NdotL = clamp(NdotL, 0.001, 1.0);
  NdotV = clamp(abs(NdotV), 0.001, 1.0);

  float VdotH = dot(V, H);

  pdf = NdotL * M_1_OVER_PI;
  return BRDF_lambertian(f0, f90, basecolor, VdotH, metallic);
}

// The following equation(s) model the distribution of microfacet normals across
// the area being drawn (aka D()) Implementation from "Average Irregularity
// Representation of a Roughened Surface for Ray Reflection" by T. S.
// Trowbridge, and K. P. Reitz Follows the distribution function recommended in
// the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float D_GGX(float NdotH, float alphaRoughness) {
  float alphaRoughnessSq = alphaRoughness * alphaRoughness;
  float f = (NdotH * NdotH) * (alphaRoughnessSq - 1.0) + 1.0;
  return alphaRoughnessSq / (M_PI * f * f);
}

// The following equation models the Fresnel reflectance term of the spec
// equation (aka F()) Implementation of fresnel from [4], Equation 15
vec3 F_Schlick(vec3 f0, vec3 f90, float VdotH) {
  return f0 + (f90 - f0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

float F_Schlick(float f0, float f90, float VdotH) {
  return f0 + (f90 - f0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

// Smith Joint GGX
// Note: Vis = G / (4 * NdotL * NdotV)
// see Eric Heitz. 2014. Understanding the Masking-Shadowing Function in
// Microfacet-Based BRDFs. Journal of Computer Graphics Techniques, 3 see
// Real-Time Rendering. Page 331 to 336. see
// https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf/geometricshadowing(specularg)
float V_GGX(float NdotL, float NdotV, float alphaRoughness) {
  float alphaRoughnessSq = alphaRoughness * alphaRoughness;

  float GGXV =
      NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
  float GGXL =
      NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);

  float GGX = GGXV + GGXL;
  if (GGX > 0.0) {
    return 0.5 / GGX;
  }
  return 0.0;
}

//  https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments
//  AppendixB
vec3 BRDF_specularGGX(vec3 f0,
                      vec3 f90,
                      float alphaRoughness,
                      float VdotH,
                      float NdotL,
                      float NdotV,
                      float NdotH) {
  vec3 F = F_Schlick(f0, f90, VdotH);
  float V = V_GGX(NdotL, NdotV, alphaRoughness);
  float D = D_GGX(NdotH, max(0.001, alphaRoughness));

  return F * V * D;
}

vec3 EvalSpecularGltf(float roughness,
                      vec3 f0,
                      vec3 f90,
                      vec3 V,
                      vec3 N,
                      vec3 L,
                      vec3 H,
                      out float pdf) {
  pdf = 0;
  float NdotL = dot(N, L);

  if (NdotL < 0.0)
    return vec3(0.0);

  float NdotV = dot(N, V);
  float NdotH = clamp(dot(N, H), 0, 1);
  float LdotH = clamp(dot(L, H), 0, 1);
  float VdotH = clamp(dot(V, H), 0, 1);

  NdotL = clamp(NdotL, 0.001, 1.0);
  NdotV = clamp(abs(NdotV), 0.001, 1.0);

  pdf = D_GGX(NdotH, roughness) * NdotH / (4.0 * LdotH);
  return BRDF_specularGGX(f0, f90, roughness, VdotH, NdotL, NdotV, NdotH);
}

vec3 PbrEval(float eta,
             float metallic,
             float roughness,
             vec3 baseColor,
             vec3 specularColor,
             vec3 V,
             vec3 N,
             vec3 L,
             inout float pdf) {
  vec3 H;

  if (dot(N, L) < 0.0)
    H = normalize(L * (1.0 / eta) + V);
  else
    H = normalize(L + V);

  if (dot(N, H) < 0.0)
    H = -H;

  vec3 brdf = vec3(0.0);
  float brdfPdf = 0.0;

  if (dot(N, L) > 0) {
    float pdf;

    float diffuseRatio = 0.5 * (1.0 - metallic);
    float specularRatio = 1.0 - diffuseRatio;
    float primarySpecRatio = 1.0;

    // Compute reflectance.
    // Anything less than 2% is physically impossible and is instead considered
    // to be shadowing. Compare to "Real-Time-Rendering" 4th editon on page 325.
    vec3 specularCol = specularColor;
    float reflectance = max(max(specularCol.r, specularCol.g), specularCol.b);
    vec3 f0 = specularCol.rgb;
    vec3 f90 = vec3(clamp(reflectance * 50.0, 0.0, 1.0));

    // Calculation of analytical lighting contribution
    // Diffuse
    brdf += EvalDiffuseGltf(baseColor, metallic, f0, f90, V, N, L, H, pdf);
    brdfPdf += pdf * diffuseRatio;

    // Specular
    brdf += EvalSpecularGltf(roughness, f0, f90, V, N, L, H, pdf);
    brdfPdf += pdf * primarySpecRatio * specularRatio;
  }

  pdf = brdfPdf;

  return brdf;
}

vec3 CosineSampleHemisphere(float r1, float r2) {
  vec3 dir;
  float r = sqrt(r1);
  float phi = M_TWO_PI * r2;
  dir.x = r * cos(phi);
  dir.y = r * sin(phi);
  dir.z = sqrt(max(0.0, 1.0 - dir.x * dir.x - dir.y * dir.y));

  return dir;
}

vec3 GgxSampling(float specularAlpha, float r1, float r2) {
  float phi = r1 * 2.0 * M_PI;

  float cosTheta =
      sqrt((1.0 - r2) / (1.0 + (specularAlpha * specularAlpha - 1.0) * r2));
  float sinTheta = clamp(sqrt(1.0 - (cosTheta * cosTheta)), 0.0, 1.0);
  float sinPhi = sin(phi);
  float cosPhi = cos(phi);

  return vec3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
}

vec3 PbrSample(vec3 baseColor,
               vec3 specularColor,
               float eta,
               float ior,
               float transmission,
               float metallic,
               float roughness,
               vec3 tangent,
               vec3 bitangent,
               vec3 V,
               vec3 N,
               inout vec3 L,
               inout float pdf,
               inout uint seed) {
  pdf = 0.0;
  vec3 brdf = vec3(0.0);

  float probability = rand(seed);
  float diffuseRatio = 0.5 * (1.0 - metallic);
  float specularRatio = 1.0 - diffuseRatio;
  float transWeight = (1.0 - metallic) * transmission;

  float r1 = rand(seed);
  float r2 = rand(seed);

  if (rand(seed) > transWeight) {
    // Anything less than 2% is physically impossible and is instead considered
    // to be shadowing. Compare to "Real-Time-Rendering" 4th editon on page 325.
    vec3 specularCol = specularColor;
    float reflectance = max(max(specularCol.r, specularCol.g), specularCol.b);
    vec3 f0 = specularCol.rgb;
    vec3 f90 = vec3(clamp(reflectance * 50.0, 0.0, 1.0));

    vec3 T = tangent;
    vec3 B = bitangent;

    if (probability < diffuseRatio)  // sample diffuse
    {
      L = CosineSampleHemisphere(r1, r2);
      L = L.x * T + L.y * B + L.z * N;

      vec3 H = normalize(L + V);

      brdf = EvalDiffuseGltf(baseColor, metallic, f0, f90, V, N, L, H, pdf);
      pdf *= diffuseRatio;
    } else {
      float primarySpecRatio = 1.0;
      vec3 H = GgxSampling(roughness, r1, r2);
      H = T * H.x + B * H.y + N * H.z;
      L = reflect(-V, H);

      // Sample primary specular lobe
      if (rand(seed) < primarySpecRatio) {
        // Specular
        brdf = EvalSpecularGltf(roughness, f0, f90, V, N, L, H, pdf);
        pdf *= primarySpecRatio * specularRatio;
      }
    }

    brdf *= (1.0 - transWeight);
    pdf *= (1.0 - transWeight);
  }

  return brdf;
}

#endif  // PBR_GLTF_GLSL
