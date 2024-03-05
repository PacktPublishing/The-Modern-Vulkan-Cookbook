#version 460

// Define a structure to hold glyph information
struct GlyphInfo {
  vec4 bbox;  // Bounding box of the glyph

  // Cell info:
  // - point offset
  // - cell offset
  // - cell count in x
  // - cell count in y
  uvec4 cell_info;
};

// Define a shader storage buffer object for glyphs
layout(set = 0, binding = 0) buffer GlyphBuffer {
  GlyphInfo glyphs[];
}
glyph_buffer;

// Define the input variables
layout(location = 0) in vec4 in_rect;         // Input rectangle
layout(location = 1) in uint in_glyph_index;  // Index of the glyph
layout(location = 2) in float in_sharpness;   // Sharpness of the glyph

// Define the output variables
layout(location = 0) out vec2 out_glyph_pos;   // Output glyph position
layout(location = 1) out uvec4 out_cell_info;  // Output cell information
layout(location = 2) out float out_sharpness;  // Output sharpness
layout(location = 3) out vec2 out_cell_coord;  // Output cell coordinates

void main() {
  // Get the glyph information
  GlyphInfo gi = glyph_buffer.glyphs[in_glyph_index];

  // Define the positions of the corners of the rectangle
  vec2 pos[4] = vec2[](vec2(in_rect.x, in_rect.y),  // Bottom-left corner
                       vec2(in_rect.z, in_rect.y),  // Bottom-right corner
                       vec2(in_rect.x, in_rect.w),  // Top-left corner
                       vec2(in_rect.z, in_rect.w)   // Top-right corner
  );

  // Define the positions of the corners of the glyph
  vec2 glyph_pos[4] =
      vec2[](vec2(gi.bbox.x, gi.bbox.y),  // Bottom-left corner of the glyph
             vec2(gi.bbox.z, gi.bbox.y),  // Bottom-right corner of the glyph
             vec2(gi.bbox.x, gi.bbox.w),  // Top-left corner of the glyph
             vec2(gi.bbox.z, gi.bbox.w)   // Top-right corner of the glyph
      );

  // Define the cell coordinates
  vec2 cell_coord[4] =
      vec2[](vec2(0, 0),                           // Bottom-left corner
             vec2(gi.cell_info.z, 0),              // Bottom-right corner
             vec2(0, gi.cell_info.w),              // Top-left corner
             vec2(gi.cell_info.z, gi.cell_info.w)  // Top-right corner
      );

  // Set the position, glyph position, cell information, sharpness, and cell
  // coordinates
  gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
  out_glyph_pos = glyph_pos[gl_VertexIndex];
  out_cell_info = gi.cell_info;
  out_sharpness = in_sharpness;
  out_cell_coord = cell_coord[gl_VertexIndex];
}
