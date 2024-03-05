#version 460
#extension GL_GOOGLE_include_directive : require

layout(location = 0) out vec4 outputColor;
layout(location = 1) out float outputAlpha;

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in float inViewSpaceDepth;

layout(set = 1, binding = 0) uniform ObjectProperties {
  vec4 color;
  mat4 model;
}
objectProperties;

void main() {
  // The depth weight is calculated based on the view space depth, which is
  // scaled by a factor of 3/200. Scale of 3/200 is choosen chosen empirically
  // based on scene being rendered. The purpose of scaling the depth is to map
  // the depth values from the range they're naturally in to a range that is
  // appropriate for the weight calculations. The depth weight is a measure of
  // the fragment's importance based on its distance from the camera. Closer
  // fragments have larger weights.
  const float scaledDepth = -(inViewSpaceDepth * 3.0) / 200;

  // Calculate maximum color component multiplied by alpha
  // the reason is to give more weight to pixels that are more vibrant
  // brighter the color, the more significant their weight is
  float maxColorComponent =
      max(max(objectProperties.color.r, objectProperties.color.g),
          objectProperties.color.b);
  float weightedColor = maxColorComponent * objectProperties.color.a;

  // Ensure weightedColor is no more than 1.0 and take the maximum value between
  // this and the alpha
  float weightedColorAlpha =
      max(min(1.0, weightedColor), objectProperties.color.a);

  // Calculate the depth weight, which is larger for closer objects
  float depthWeight = 0.03 / (1e-5 + pow(scaledDepth, 4.0));

  // Clamp the depth weight between 0.01 and 4000
  depthWeight = clamp(depthWeight, 0.01, 4000);

  // The final weight is the product of the color and depth weights
  const float weight = weightedColorAlpha * depthWeight;

  // premultiply alpha since otherwise saturation will happen i.e. If colors
  // weren't premultiplied by their alpha values, when we blend a
  // semi-transparent color with another color, the result could be overly
  // saturated. This is because the RGB color values would be contributing fully
  // to the result, regardless of the level of transparency.
  outputColor = vec4(objectProperties.color.rgb * objectProperties.color.a,
                     objectProperties.color.a) *
                weight;

  outputAlpha = objectProperties.color.a;
}
