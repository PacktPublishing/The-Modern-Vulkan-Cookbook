#version 460
vec2 positions[4] =
    vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));

vec2 texCoords[4] =
    vec2[](vec2(0.0, 1.0), vec2(1.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 0.0));

layout(location = 0) out vec2 fragTexCoord;

void main() {
  gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
  fragTexCoord = texCoords[gl_VertexIndex];
}
