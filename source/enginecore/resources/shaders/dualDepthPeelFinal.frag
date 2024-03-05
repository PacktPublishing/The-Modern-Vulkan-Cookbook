#version 460
layout(set = 0, binding = 0) uniform sampler2D front;
layout(set = 0, binding = 1) uniform sampler2D back;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
  const vec4 frontColor = texture(front, fragTexCoord);
  const vec4 backColor = texture(back, fragTexCoord);
  outColor = vec4(((backColor)*frontColor.a + frontColor).rgb,
                  (1.0 - backColor.a) * frontColor.a);
}
