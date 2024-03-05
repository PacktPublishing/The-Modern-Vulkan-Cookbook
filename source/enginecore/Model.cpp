#include "Model.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace EngineCore {

Vertex16Bit to16bitVertex(const Vertex& v) {
  Vertex16Bit out;

  out.posx = glm::packHalf1x16(v.pos.x);
  out.posy = glm::packHalf1x16(v.pos.y);
  out.posz = glm::packHalf1x16(v.pos.z);
  out.normalx = glm::packHalf1x16(v.normal.x);
  out.normaly = glm::packHalf1x16(v.normal.y);
  out.normalz = glm::packHalf1x16(v.normal.z);
  out.tangentx = glm::packHalf1x16(v.tangent.x);
  out.tangenty = glm::packHalf1x16(v.tangent.y);
  out.tangentz = glm::packHalf1x16(v.tangent.z);
  out.tangentw = glm::packHalf1x16(v.tangent.w);
  out.texcoordu = glm::packHalf1x16(v.texCoord.x);
  out.texcoordv = glm::packHalf1x16(v.texCoord.y);
  out.texcoord1u = glm::packHalf1x16(v.texCoord1.x);
  out.texcoord1v = glm::packHalf1x16(v.texCoord1.y);
  out.material = v.material;

  return out;
}

stbImageData::stbImageData(const std::vector<uint8_t>& imageData, bool useFloat) {
  if (!useFloat) {
    data = stbi_load_from_memory(imageData.data(), imageData.size(), &width, &height,
                                 &channels, STBI_rgb_alpha);
  } else {
    data = stbi_loadf_from_memory(imageData.data(), imageData.size(), &width, &height,
                                  &channels, STBI_rgb_alpha);
  }
}

stbImageData::stbImageData(const std::vector<char>& imageData, bool useFloat) {
  if (!useFloat) {
    data = stbi_load_from_memory(reinterpret_cast<const uint8_t*>(imageData.data()),
                                 imageData.size(), &width, &height, &channels,
                                 STBI_rgb_alpha);
  } else {
    data = stbi_loadf_from_memory(reinterpret_cast<const uint8_t*>(imageData.data()),
                                  imageData.size(), &width, &height, &channels,
                                  STBI_rgb_alpha);
  }
}

stbImageData::~stbImageData() { stbi_image_free(data); }

}  // namespace EngineCore
