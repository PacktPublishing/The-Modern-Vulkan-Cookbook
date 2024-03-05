#ifndef RAYTRACE_STRUCT_GLSL
#define RAYTRACE_STRUCT_GLSL 1

const int MAX_SAMPLES = 4;

const int MAX_BOUNCES = 10;

const float PI = 3.141592653589793;
const float TWO_PI = 6.28318530717958647692;
const float INV_PI = 0.31830988618379067154;
const float INV_2PI = 0.15915494309189533577;

const float EPS = 0.001;

struct RayPayload {
  vec3 radiance;
  vec3 origin;
  vec3 direction;
  vec3 throughput;
  vec3 ao;
  int currentBounceIndex;
  uint seed;
  bool exit;
  bool isCameraRay;
};

#endif
