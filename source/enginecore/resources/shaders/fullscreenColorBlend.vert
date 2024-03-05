#version 460
vec2 positions[4] =
    vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));

vec2 texCoords[4] =
    vec2[](vec2(0.0, 1.0), vec2(1.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 0.0));

// Push constants
layout(push_constant) uniform constants {
  vec4 colors[4];
}
FullScreenColorPushConst;

layout(location = 0) out vec4 color;
layout(location = 1) out vec2 fragTexCoord;

void main() {
  gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
  fragTexCoord = texCoords[gl_VertexIndex];
  color = FullScreenColorPushConst.colors[gl_VertexIndex];
}
