#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

struct OutData {
  std::vector<glm::vec2> points;
  glm::vec4 bbox;
  std::vector<uint32_t> cellData;
  uint32_t cellX;
  uint32_t cellY;
  float horizontalAdvance;
};

class FontManager {
 public:
  // loads all characters data from A to Z
  std::vector<OutData> loadFont(const std::string& fontFile);
};
