#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_debug_printf : enable

#extension GL_GOOGLE_include_directive : require
#include "CommonStructs.glsl"
#include "IndirectCommon.glsl"

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out flat uint outflatMeshId;
layout(location = 2) out flat int outflatMaterialId;

struct Line {
  vec3 v0;
  vec4 c0;
  vec3 v1;
  vec4 c1;
};

struct VkDrawIndirectCommand {
  uint vertexCount;
  uint instanceCount;
  uint firstVertex;
  uint firstInstance;
};

layout(set = 4, binding = 0) buffer GPULinesBuffer {
  uint size;
  uint row;
  uint pad1;
  uint pad2;
  VkDrawIndirectCommand cmd;
  Line lines[];
}
lineBuffer;

void addLine(vec3 v0, vec3 v1, vec4 c0, vec4 c1) {
  uint idx = atomicAdd(lineBuffer.cmd.instanceCount, 1);

  if (idx >= lineBuffer.size) {
    atomicMin(lineBuffer.cmd.instanceCount, lineBuffer.size);
    return;
  }

  lineBuffer.lines[idx].v0 = v0;
  lineBuffer.lines[idx].v1 = v1;
  lineBuffer.lines[idx].c0 = c0;
  lineBuffer.lines[idx].c1 = c1;
}

// Add segments' vertices
//          0
//    v0 +----+ v1
//    3  |  1 |  4
//    v2 +----+ v3
//    5  |  2 | 6
//    v4 +--|-+ v5

vec2 v[] = {vec2(-.5f, 1.f),   vec2(.5f, 1.f),   vec2(-.5f, 0.f),
            vec2(.5f, 0.f),    vec2(-.5f, -1.f), vec2(.5f, -1.f),
            vec2(0.0f, -0.8f), vec2(0.0f, -1.f)};

uvec2 i[] = {uvec2(0, 1), uvec2(2, 3), uvec2(4, 5), uvec2(0, 2),
             uvec2(1, 3), uvec2(2, 4), uvec2(3, 5), uvec2(6, 7)};

void printSegment(int segment, vec2 pos, vec2 scale) {
  uint idx = i[segment].x;
  uint idy = i[segment].y;
  vec3 v0 = vec3(v[idx] * scale + pos, 1.0);
  vec3 v1 = vec3(v[idy] * scale + pos, 1.0);

  addLine(v0, v1, vec4(0, 0, 0, 1), vec4(0, 0, 0, 1));

  debugPrintfEXT("v0: (%f, %f, %f) , v1: (%f, %f, %f)", v0.x, v0.y, v0.z, v1.x,
                 v1.y, v1.z);
}

void printDigit(int digit, uint linenum, uint column) {
  float pixelWidth = 10;           // pixels
  float pixelHeight = 10;          // pixels
  float horSpaceNDC = 5.0 / 1600;  // pixels
  float verSpaceNDC = 5.0 / 1200;  // pixels
  float charWidthNDC = pixelWidth / 1600;
  float charHeightNDC = pixelHeight / 1200;
  float colx =
      column * (charWidthNDC + horSpaceNDC) + charWidthNDC + horSpaceNDC;
  float coly =
      linenum * (charHeightNDC + 3 * verSpaceNDC) + charHeightNDC + verSpaceNDC;

  debugPrintfEXT(
      "width = %f, height = %f, colx = %f, coly = %f, horspace = %f, verspace "
      "= %f",
      charWidthNDC, charHeightNDC, colx, coly, horSpaceNDC, verSpaceNDC);

  switch (digit) {
    case 0:
      printSegment(0, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(3, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(4, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(5, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(6, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(2, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      break;

    case 1:
      printSegment(4, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(6, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      break;

    case 2:
      printSegment(0, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(4, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(1, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(5, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(2, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      break;

    case 3:
      printSegment(0, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(4, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(1, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(6, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(2, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));

      break;
    case 4:
      printSegment(3, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(4, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(1, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(6, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      break;
    case 5:
      printSegment(0, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(3, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(1, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(6, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(2, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      break;
    case 6:
      printSegment(0, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(3, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(1, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(5, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(6, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(2, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      break;
    case 7:
      printSegment(0, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(4, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(6, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      break;
    case 8:
      printSegment(0, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(1, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(2, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(3, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(4, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(5, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(6, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(2, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      break;
    case 9:
      printSegment(0, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(3, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(4, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(1, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      printSegment(6, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      break;
    case 10:
      printSegment(7, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      break;  // dot
    case 11:
      printSegment(1, vec2(colx, coly), vec2(charWidthNDC, -charHeightNDC));
      break;  // minus sign
  }
}

uint printNumber(highp int value, float decimals, uint linenum, uint column) {
  if (value == 0) {
    printDigit(0, linenum, column);
    return column + 1;
  }

  int counter = 0;
  int copy = value;
  int tens = 1;
  while (copy > 0) {
    counter++;
    copy = copy / 10;
    tens *= 10;
  }
  tens /= 10;

  for (int i = counter; i > 0; --i) {
    int digit = int(value / tens);
    printDigit(digit, linenum, column);
    value = value - (digit * tens);
    tens /= 10;
    column++;
    // debugPrintfEXT("tens = %d, digit = %d, value = %d, column = %d", tens,
    // digit, value, column);
  }

  return column;
}

void parse(float val, uint decimals) {
  int d = int(log(val));
  int base = int(pow(10, d));

  float tens = pow(10, decimals);

  // Very expensive
  uint line = atomicAdd(lineBuffer.row, 1);
  uint column = 0;

  // Minus sign
  if (val < 0) {
    printDigit(11, line, column);
    column++;
  }

  // Only positive values
  val = abs(val);

  // Integer part
  int intPart = int(val);
  column = printNumber(intPart, 0, line, column);

  // Decimal
  if (decimals > 0) {
    // Dot
    printDigit(10, line, column);
    column++;

    int decimal = int(val * tens - intPart * tens);
    printNumber(decimal, decimals, line, column);
  }
}

void main() {
  Vertex vertex = vertexAlias[VERTEX_INDEX].vertices[gl_VertexIndex];

  if (gl_VertexIndex == 0) {
    parse(123456, 0);
    parse(789, 0);
    parse(780.12, 3);
    parse(-23, 1);
    parse(0.3, 2);
  }

  vec3 position = vec3(vertex.posX, vertex.posY, vertex.posZ);
  vec3 normal = vec3(vertex.normalX, vertex.normalY, vertex.normalZ);

  addLine(position, position + normal, vec4(0, 0, 0, 1), vec4(0, 1, 0, 1));

  vec2 uv = vec2(vertex.uvX, vertex.uvY);
  gl_Position = MVP.projection * MVP.view * MVP.model * vec4(position, 1.0);
  outTexCoord = uv;
  outflatMeshId =
      indirectDrawAlias[INDIRECT_DRAW_INDEX].meshDraws[gl_DrawID].meshId;
  outflatMaterialId = vertex.material;
}
