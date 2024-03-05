#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_debug_printf : enable

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

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec4 outPos;
layout(location = 2) out flat int outMaterial;

void main() {
  Vertex vertex = vertexBuffer.vertices[gl_VertexIndex];
  vec3 position = vec3(vertex.posX, vertex.posY, vertex.posZ);
  vec3 normal = vec3(vertex.normalX, vertex.normalY, vertex.normalZ);
  vec2 uv = vec2(vertex.uvX, vertex.uvY);
  gl_Position = MVP.projection * MVP.view * MVP.model * vec4(position, 1.0);
  outTexCoord = uv;
  outMaterial = vertex.material;
  outPos = vec4(position, 1.0);
}
