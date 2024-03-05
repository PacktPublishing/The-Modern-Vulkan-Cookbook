#include "FontManager.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BBOX_H
#include FT_OUTLINE_H

extern "C" {
#include "outline.h"
}

std::vector<OutData> FontManager::loadFont(const std::string& fontFile) {
  FT_Library library;
  FT_CHECK(FT_Init_FreeType(&library));

  FT_Face face;
  FT_CHECK(FT_New_Face(library, fontFile.c_str(), 0, &face));

  FT_CHECK(FT_Set_Char_Size(face, 0, 1000 * 64, 96, 96));

  std::vector<OutData> glyphData;  // A to Z
  glyphData.resize(26);

  uint32_t totalPoints = 0;
  uint32_t totalCells = 0;

  for (uint32_t i = 0; i < glyphData.size(); i++) {
    char c = i + 'A';
    FT_UInt glyph_index = FT_Get_Char_Index(face, c);
    FT_CHECK(FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_HINTING));

    FT_Outline outline = face->glyph->outline;

    fd_Outline outData;

    fd_outline_convert(&outline, &outData, c);

    for (int j = 0; j < outData.num_of_points; j++) {
      glyphData[i].points.push_back(
          glm::vec2(outData.points[j][0], outData.points[j][1]));
    }

    for (int j = 0; j < outData.cell_count_x * outData.cell_count_y; j++) {
      glyphData[i].cellData.push_back(outData.cells[j]);
    }
    glyphData[i].cellX = outData.cell_count_x;
    glyphData[i].cellY = outData.cell_count_y;

    glyphData[i].bbox.x = outData.bbox.min_x;
    glyphData[i].bbox.y = outData.bbox.min_y;
    glyphData[i].bbox.z = outData.bbox.max_x;
    glyphData[i].bbox.w = outData.bbox.max_y;

    glyphData[i].horizontalAdvance = face->glyph->metrics.horiAdvance / 64.0f;
  }

  return glyphData;
}
