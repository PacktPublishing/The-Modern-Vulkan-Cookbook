#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#include "random.glsl"
#include "raytrace_struct.glsl"

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;

#include "raytrace_hdr.glsl"

void main() {
  rayPayload.radiance = envMapColor(gl_WorldRayDirectionEXT);
  rayPayload.exit = true;
}
