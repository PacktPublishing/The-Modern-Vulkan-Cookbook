#version 460
#extension GL_EXT_nonuniform_qualifier : require

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

layout(set = 0, binding = 0) uniform Transforms {
  mat4 model;
  mat4 view;
  mat4 projection;
  mat4 prevView;
  mat4 jitterMat;
}
MVP;

layout(set = 1, binding = 0) uniform texture2D BindlessImage2D[];
layout(set = 1, binding = 1) uniform sampler BindlessSampler[];

layout(set = 2, binding = 0) readonly buffer VertexBuffer {
  Vertex vertices[];
}
vertexBuffer;

layout(set = 2, binding = 1) readonly buffer IndexBuffer {
  uint indices[];
}
indexBuffer;

layout(set = 3, binding = 0) readonly buffer MaterialBufferForAllMesh {
  MaterialData materials[];
}
materials;

layout(push_constant) uniform ObjectData {
  uint basecolorIndex;
  uint basecolorSamplerIndex;
  uint metallicRoughnessIndex;
  uint metallicRoughnessSamplerIndex;
  uint normalIndex;
  uint normalSamplerIndex;
  uint emissiveIndex;
  uint emissiveSamplerIndex;
}
PushConstants;

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec4 inPos;
layout(location = 2) in flat int inMaterial;

layout(location = 0) out vec4 outColor;

void main() {
  MaterialData material = materials.materials[inMaterial];

  if (material.basecolorIndex != -1 && material.basecolorSamplerIndex != -1) {
    outColor =
        texture(sampler2D(BindlessImage2D[material.basecolorIndex],
                          BindlessSampler[material.basecolorSamplerIndex]),
                inTexCoord);
  } else {
    outColor = vec4(0.5, .5, 0.5, 1.0);
  }
}
