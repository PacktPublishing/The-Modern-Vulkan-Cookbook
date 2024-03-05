#version 460
vec2 positions[4] =
    vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));

// Push constants
layout(push_constant) uniform constants {
  vec4 colors[4];
}
FullScreenColorPushConst;

layout(location = 0) out vec4 color;

void main() {
  gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
  color = FullScreenColorPushConst.colors[gl_VertexIndex];
}
