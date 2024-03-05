#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : require

#include "random.glsl"

#include "raytrace_struct.glsl"

#include "raytrace_utils.glsl"

#include "pbr_gltf.glsl"

#include "raytrace_hdr.glsl"



layout(location = 0) rayPayloadInEXT RayPayload rayPayload;
layout(location = 1) rayPayloadEXT bool inshadow;

// automatically populated by intersection shader, in case of triangles, its
// barycentric coordinates of the intersection point
hitAttributeEXT vec2 attribs;

layout(set = 0,
       binding = 0) uniform accelerationStructureEXT topLevelAccelStruct;

layout(set = 0, binding = 2) uniform CameraProperties {
  mat4 viewInverse;
  mat4 projInverse;
  uint frameId;
}
camProps;

// we could use vec3 etc but that can cause alignment issue, so we prefer float
struct Vertex {
  float posX;
  float posY;
  float posZ;
  float normalX;
  float normalY;
  float normalZ;
  float tangentX;
  float tangentY;
  float tangentZ;
  float tangentW;
  float uvX;
  float uvY;
  float uvX2;
  float uvY2;
  int material;
};

struct IndirectDrawDataAndMeshData {
  uint indexCount;
  uint instanceCount;
  uint firstIndex;
  uint vertexOffset;
  uint firstInstance;

  uint meshId;
  int materialIndex;
  // in future we can model mat etc, things specific to Mesh
};

struct MaterialData {
  int basecolorIndex;
  int basecolorSamplerIndex;
  int metallicRoughnessIndex;
  int metallicRoughnessSamplerIndex;
  int normalIndex;
  int normalSamplerIndex;
  int emissiveIndex;
  int emissiveSamplerIndex;
  float metallicFactor;
  float roughnessFactor;
  vec2 padding;
  vec4 basecolor;
};

layout(set = 1, binding = 0) uniform texture2D BindlessImage2D[];
layout(set = 2, binding = 0) uniform sampler BindlessSampler[];

layout(set = 3, binding = 0) readonly buffer VertexBuffer {
  Vertex vertices[];
}
vertexAlias[4];

layout(set = 3, binding = 0) readonly buffer IndexBuffer {
  uint indices[];
}
indexAlias[4];

layout(set = 3, binding = 0) readonly buffer IndirectDrawDataAndMeshDataBuffer {
  IndirectDrawDataAndMeshData meshDraws[];
}
indirectDrawAlias[4];

layout(set = 3, binding = 0) readonly buffer MaterialBufferForAllMesh {
  MaterialData materials[];
}
materialDataAlias[4];

const int VERTEX_INDEX = 0;
const int INDICIES_INDEX = 1;
const int INDIRECT_DRAW_INDEX = 2;
const int MATERIAL_DATA_INDEX = 3;

void main() {
  IndirectDrawDataAndMeshData indirectData =
      indirectDrawAlias[INDIRECT_DRAW_INDEX].meshDraws[gl_InstanceID];

  uint primitiveIndex = gl_PrimitiveID;
  uint indexBase = primitiveIndex * 3;

  // Indices of the vertices in the VertexBuffer
  uint vertexIndex0 = indexAlias[INDICIES_INDEX]
                          .indices[indexBase + 0 + indirectData.firstIndex];
  uint vertexIndex1 = indexAlias[INDICIES_INDEX]
                          .indices[indexBase + 1 + indirectData.firstIndex];
  uint vertexIndex2 = indexAlias[INDICIES_INDEX]
                          .indices[indexBase + 2 + indirectData.firstIndex];

  Vertex vertex0 = vertexAlias[VERTEX_INDEX]
                       .vertices[vertexIndex0 + indirectData.vertexOffset];
  Vertex vertex1 = vertexAlias[VERTEX_INDEX]
                       .vertices[vertexIndex1 + indirectData.vertexOffset];
  Vertex vertex2 = vertexAlias[VERTEX_INDEX]
                       .vertices[vertexIndex2 + indirectData.vertexOffset];

  vec3 p0 = vec3(vertex0.posX, vertex0.posY, vertex0.posZ);
  vec3 p1 = vec3(vertex1.posX, vertex1.posY, vertex1.posZ);
  vec3 p2 = vec3(vertex2.posX, vertex2.posY, vertex2.posZ);

  vec3 n0 = vec3(vertex0.normalX, vertex0.normalY, vertex0.normalZ);
  vec3 n1 = vec3(vertex1.normalX, vertex1.normalY, vertex1.normalZ);
  vec3 n2 = vec3(vertex2.normalX, vertex2.normalY, vertex2.normalZ);

  vec4 t0 = vec4(vertex0.tangentX, vertex0.tangentY, vertex0.tangentZ,
                 vertex0.tangentW);
  vec4 t1 = vec4(vertex1.tangentX, vertex1.tangentY, vertex1.tangentZ,
                 vertex1.tangentW);
  vec4 t2 = vec4(vertex2.tangentX, vertex2.tangentY, vertex2.tangentZ,
                 vertex2.tangentW);

  const vec3 barycentricCoords =
      vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
  const vec3 position = p0 * barycentricCoords.x + p1 * barycentricCoords.y +
                        p2 * barycentricCoords.z;
  const vec3 worldPosition = vec3(gl_ObjectToWorldEXT * vec4(position, 1.0));

  const vec3 normal =
      normalize(n0 * barycentricCoords.x + n1 * barycentricCoords.y +
                n2 * barycentricCoords.z);
  const vec3 worldNormal = normalize(vec3(normal * gl_WorldToObjectEXT));

  const vec4 tangent = t0 * barycentricCoords.x + t1 * barycentricCoords.y +
                       t2 * barycentricCoords.z;
  const vec4 worldTangent =
      vec4((gl_WorldToObjectEXT * vec4(tangent.xyz, 0)).xyz, tangent.w);

  vec2 uv0 = vec2(vertex0.uvX, vertex0.uvY);
  vec2 uv1 = vec2(vertex1.uvX, vertex1.uvY);
  vec2 uv2 = vec2(vertex2.uvX, vertex2.uvY);

  const vec2 fragTexCoord0 = uv0 * barycentricCoords.x +
                             uv1 * barycentricCoords.y +
                             uv2 * barycentricCoords.z;

  int matid = vertex0.material;

  int basecolorIndex = -1;
  int normalIndex = -1;
  int metallicRoughnessIndex = -1;
  int emissiveIndex = -1;
  vec4 baseColorMat;

  uint samplerIndex = 0;
  float metallicFactor = 1.0;
  float roughnessFactor = 1.0;

  if (matid != -1) {
    MaterialData mat = materialDataAlias[MATERIAL_DATA_INDEX].materials[matid];
    basecolorIndex = mat.basecolorIndex;
    normalIndex = mat.normalIndex;
    metallicRoughnessIndex = mat.metallicRoughnessIndex;
    emissiveIndex = mat.emissiveIndex;
    baseColorMat = mat.basecolor;
    metallicFactor = mat.metallicFactor;
    roughnessFactor = mat.roughnessFactor;
  }

  vec4 baseColor = vec4(0.5, .5, 0.5, 1.0);

  if (basecolorIndex != -1) {
    baseColor = texture(sampler2D(BindlessImage2D[basecolorIndex],
                                  BindlessSampler[samplerIndex]),
                        fragTexCoord0);
    baseColor.rgb = sRGB2linear(baseColor.rgb) * baseColorMat.rgb;
  } else {
    baseColor.rgb = baseColorMat.rgb;
  }

  vec3 N = normalize(worldNormal);
  const vec3 T = normalize(worldTangent.xyz);
  const vec3 B = worldTangent.w * cross(worldNormal, worldTangent.xyz);
  const mat3 TBN = mat3(T, B, N);

  if (normalIndex != -1) {
    vec4 normalTexSampled = texture(
        sampler2D(BindlessImage2D[normalIndex], BindlessSampler[samplerIndex]),
        fragTexCoord0);

    vec3 tangentNormal = normalTexSampled.xyz * 2.0 - 1.0;

    N = normalize(TBN * tangentNormal);
  }

  // face forward normal
  vec3 ffnormal = dot(worldNormal, rayPayload.direction) <= 0.0
                      ? normalize(worldNormal)
                      : normalize(worldNormal) * -1.0;

  float metallic = 0.0;
  float roughness = 0.0;

  if (metallicRoughnessIndex != -1) {
    vec4 metallicRoughnessTexSampled =
        texture(sampler2D(BindlessImage2D[metallicRoughnessIndex],
                          BindlessSampler[samplerIndex]),
                fragTexCoord0);

    metallic = metallicRoughnessTexSampled.b;
    roughness = metallicRoughnessTexSampled.g;

    metallic *= metallicFactor;
    roughness *= roughnessFactor;
  } else {
    metallic = metallicFactor;
    roughness = roughnessFactor;
  }

  // D_GGX takes roughness to compute pdf (pbr_gltf.glsl), if not clamped, this
  // can cause artifacts We could use Delta distribution but thats more work :)
  // (https://pbr-book.org/4ed/Reflection_Models/BSDF_Representation#DeltaDistributionsinBSDFs)
  roughness = max(roughness, .001);

  const float materialIOR = 1.5;  // should be pulled from gltf

  // Refraction indices are generally denoted with the Greek letter eta (<math
  // xmlns="http://www.w3.org/1998/Math/MathML">
  // <mi>&#x3B7;</mi>
  // </math>)
  float eta = materialIOR;  // dot(normal, ffnormal) > 0.0 ? (1.0 / materialIOR)
                            // : materialIOR;

  float dielectricSpecular = (materialIOR - 1) / (materialIOR + 1);
  dielectricSpecular *= dielectricSpecular;

  const float specular_level = .5;

  vec3 specularColor = mix(vec3(dielectricSpecular), baseColor.rgb, metallic);

  vec3 directLightColor = vec3(0);

  vec3 envLightColor = vec3(0);

  vec4 dirPdf = envSample(envLightColor, rayPayload.seed);
  vec3 lightDir = dirPdf.xyz;
  float lightPdf = dirPdf.w;

  // Define ray tracing parameters
  uint rayFlags = gl_RayFlagsTerminateOnFirstHitEXT |
                  gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsOpaqueEXT;
  float rayMinDist = 0.001;
  float rayMaxDist = 10000.0;
  float shadowBias = 0.001;
  uint cullMask = 0xFFu;

  inshadow = true;

  const int layoutLocation = 1;

  // Trace the shadow ray
  traceRayEXT(topLevelAccelStruct, rayFlags, cullMask, 0, 0, 1, worldPosition,
              rayMinDist, lightDir, rayMaxDist, layoutLocation);

  if (!inshadow) {
    float pdf;

    // returns diffuse & specular both
    vec3 F = PbrEval(eta, metallic, roughness, baseColor.rgb, specularColor,
                     -rayPayload.direction, N, lightDir, pdf);

    float cosTheta = abs(dot(lightDir, N));

    float misWeight = max(0.0, powerHeuristic(lightPdf, pdf));

    if (misWeight > 0.0) {
      directLightColor +=
          misWeight * F * cosTheta * envLightColor / (lightPdf + EPS);
    }
  }

  rayPayload.radiance += directLightColor * rayPayload.throughput;

  // remove firefly caused bec of specular
  rayPayload.radiance = clamp(rayPayload.radiance, vec3(0.0), vec3(1.0));

  // sample for next ray
  vec3 bsdfDirNextRay;
  float bsdfpdfNextRay;

  const float transmission =
      0.0;  // can usually be loaded through gltf material (using
            // KHR_materials_transmission extension)

  vec3 F =
      PbrSample(baseColor.rgb, specularColor, eta, materialIOR, transmission,
                metallic, roughness, T, B, -rayPayload.direction, ffnormal,
                bsdfDirNextRay, bsdfpdfNextRay, rayPayload.seed);

  if (bsdfpdfNextRay <= 0.0) {
    rayPayload.exit = true;
    return;
  }

  float cosTheta = abs(dot(N, bsdfDirNextRay));

  rayPayload.throughput *= F * cosTheta / (bsdfpdfNextRay);

  // Russian roulette
  float rrPcont = min(max3(rayPayload.throughput) * eta * eta + 0.001, 0.95);

  if (rand(rayPayload.seed) >= rrPcont) {
    rayPayload.exit = true;
  }

  rayPayload.throughput /= rrPcont;

  // update new ray direction & position
  rayPayload.direction = bsdfDirNextRay;
  rayPayload.origin = offsetRay(
      worldPosition,
      dot(bsdfDirNextRay, worldNormal) > 0 ? worldNormal : -worldNormal);

  if (rayPayload.isCameraRay) {
    for (uint i = 0; i < 5; ++i) {
      vec3 dir = randomHemispherePoint(rayPayload.seed, N);

      inshadow = true;

      // Trace the shadow ray
      traceRayEXT(topLevelAccelStruct, rayFlags, cullMask, 0, 0, 1,
                  worldPosition, rayMinDist, dir, rayMaxDist, layoutLocation);

      if (inshadow) {
        rayPayload.ao += (1.0 / 5.0);
      }
    }
  }

  rayPayload.isCameraRay = false;
}
