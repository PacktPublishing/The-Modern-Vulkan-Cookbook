#ifndef RAYTRACE_UTILS_GLSL
#define RAYTRACE_UTILS_GLSL 1

#include "random.glsl"

//-------------------------------------------------------------------------------------------------
// Avoiding self intersections
//-----------------------------------------------------------------------
vec3 offsetRay(in vec3 p, in vec3 n) {
  // Smallest epsilon that can be added without losing precision is 1.19209e-07,
  // but we play safe
  const float epsilon = 1.0f / 65536.0f;  // Safe epsilon

  float magnitude = length(p);
  float offset = epsilon * magnitude;
  // multiply the direction vector by the smallest offset
  vec3 offsetVector = n * offset;
  // add the offset vector to the starting point
  vec3 offsetPoint = p + offsetVector;

  return offsetPoint;
}

/*
 * Power heuristic often reduces variance even further for multiple importance
 * sampling Chapter 13.10.1 of pbrbook
 */
float powerHeuristic(float a, float b) {
  float t = a * a;
  return t / (b * b + t);
}

float max3(vec3 v) {
  return max(max(v.x, v.y), v.z);
}
float sRGB2linear(float v) {
  return (v < 0.04045) ? (v * 0.0773993808) : pow((v + 0.055) / 1.055, 2.4);
}

vec3 sRGB2linear(vec3 v) {
  return vec3(sRGB2linear(v.x), sRGB2linear(v.y), sRGB2linear(v.z));
}

float linear2sRGB(float x) {
  return x <= 0.0031308 ? 12.92 * x : 1.055 * pow(x, 0.41666) - 0.055;
}

vec3 linear2sRGB(vec3 rgb) {
  return vec3(linear2sRGB(rgb.x), linear2sRGB(rgb.y), linear2sRGB(rgb.z));
}

// From:
// https://github.com/LWJGL/lwjgl3-demos/blob/main/res/org/lwjgl/demo/opengl/raytracing/randomCommon.glsl
vec3 randomSpherePoint(vec3 rand) {
  float ang1 = (rand.x + 1.0) * PI;  // [-1..1) -> [0..2*PI)
  float u = rand.y;  // [-1..1), cos and acos(2v-1) cancel each other out, so we
                     // arrive at
                     // [-1..1)
  float u2 = u * u;
  float sqrt1MinusU2 = sqrt(1.0 - u2);
  float x = sqrt1MinusU2 * cos(ang1);
  float y = sqrt1MinusU2 * sin(ang1);
  float z = u;
  return vec3(x, y, z);
}

// this is a hash function for generating pseudo-random numbers. Taken from
// here: http://jcgt.org/published/0009/03/02/
uvec3 pcg_uvec3_uvec3(uvec3 v) {
  v = v * 1664525u + 1013904223u;
  v.x += v.y * v.z;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  v = v ^ (v >> 16u);
  v.x += v.y * v.z;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  return v;
}

// From:
// https://github.com/LWJGL/lwjgl3-demos/blob/main/res/org/lwjgl/demo/opengl/raytracing/randomCommon.glsl
vec3 randomHemispherePoint(uint seed, vec3 n) {
  /**
   * Generate random sphere point and swap vector along the normal, if it
   * points to the wrong of the two hemispheres.
   * This method provides a uniform distribution over the hemisphere,
   * provided that the sphere distribution is also uniform.
   */
  vec3 v = randomSpherePoint(normalize(pcg_uvec3_uvec3(uvec3(seed))));
  return v * sign(dot(v, n));
}

#endif
