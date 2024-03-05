#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

layout(location = 0) rayPayloadEXT float visibilityRayPayload;

void main() {
  visibilityRayPayload = 1.0;
}
